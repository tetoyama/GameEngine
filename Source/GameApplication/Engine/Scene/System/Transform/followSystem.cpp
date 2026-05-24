// =======================================================================
// 
// followSystem.cpp
// 
// =======================================================================
#include "followSystem.h"

#include "Scene/scene.h"
#include "Scene/sceneManager.h"

#include "Entity/Entity.h"
#include "Registry/componentRegistry.h"

#include "Component/followComponent.h"
#include "Component/transformComponent.h"
#include "Component/modelRendererComponent.h"

#include "Backends/Assimp/matrix4x4.h"

#include <DirectXMath.h>

void FollowSystem::Initialize() {}

void FollowSystem::Update(float deltaTime) {
	ProcessFollow();
}

void FollowSystem::EditorUpdate(float deltaTime) {
	ProcessFollow();
}

void FollowSystem::ProcessFollow() {
	for (auto& [name, scene] : m_pContext->sceneManager->GetActiveScenes()) {
		auto* context = scene->GetSceneContext();
		const auto& entities = context->component->FindEntitiesWithComponent<FollowComponent>();

		for (Entity entity : entities) {
			auto* follow = context->component->GetComponent<FollowComponent>(entity);
			auto* transform = context->component->GetComponent<TransformComponent>(entity);
			if (!follow || !transform) continue;

			// 対象エンティティが存在しない場合はスキップ
			if (follow->targetEntity == 0 ||
				!context->entity->IsAlive(follow->targetEntity)) continue;

			auto* targetTransform = context->component->GetComponent<TransformComponent>(follow->targetEntity);
			if (!targetTransform) continue;

			DirectX::XMMATRIX m_TargetWorldMat=
				targetTransform->CalculateWorldMatrix(targetTransform, context->component);

			DirectX::XMVECTOR m_WorldPos= DirectX::XMVectorZero();
			DirectX::XMFLOAT4 m_WorldQuat= { 0.0f, 0.0f, 0.0f, 1.0f };
			bool m_HasBoneResult= false;

			// --- ボーン追従 ---
			if (!follow->boneName.empty()) {
				auto* mr = context->component->GetComponent<ModelRendererComponent>(follow->targetEntity);
				if (mr && mr->model) {
					auto m_It= mr->model->m_BoneIndexMap.find(follow->boneName);
					if (it != mr->model->m_BoneIndexMap.end()) {
						uint32_t m_BoneIdx= it->second;
						const aiMatrix4x4& wm = mr->model->m_Bones[boneIdx].WorldMatrix;

						// assimp の列ベクトル規約では a4, b4, c4 が並進成分
						DirectX::XMVECTOR m_BoneModelPos=
							DirectX::XMVectorSet(wm.a4, wm.b4, wm.c4, 1.0f);

						// エンティティのワールド行列でモデル空間→ワールド空間に変換
						worldPos = DirectX::XMVector3Transform(boneModelPos, targetWorldMat);
						hasBoneResult = true;

						// 回転追従：ボーンのモデル空間軸をワールド空間に変換して回転行列を構築
						if (follow->followRotation) {
							// assimp 列 1-3 = ボーンのローカル X/Y/Z 軸（モデル空間）
							DirectX::XMVECTOR m_Bx= DirectX::XMVector3Normalize(
								DirectX::XMVector3TransformNormal(
									DirectX::XMVectorSet(wm.a1, wm.b1, wm.c1, 0.0f), targetWorldMat));
							DirectX::XMVECTOR m_By= DirectX::XMVector3Normalize(
								DirectX::XMVector3TransformNormal(
									DirectX::XMVectorSet(wm.a2, wm.b2, wm.c2, 0.0f), targetWorldMat));
							DirectX::XMVECTOR m_Bz= DirectX::XMVector3Normalize(
								DirectX::XMVector3TransformNormal(
									DirectX::XMVectorSet(wm.a3, wm.b3, wm.c3, 0.0f), targetWorldMat));

							DirectX::XMFLOAT3 fx, fy, m_Fz;
							DirectX::XMStoreFloat3(&fx, bx);
							DirectX::XMStoreFloat3(&fy, by);
							DirectX::XMStoreFloat3(&fz, bz);

							// DX 行ベクトル規約：行 = ワールド空間での軸方向
							DirectX::XMMATRIX boneWorldRotMat(
								fx.x, fx.y, fx.z, 0.0f,
								fy.x, fy.y, fy.z, 0.0f,
								fz.x, fz.y, fz.z, 0.0f,
								0.0f, 0.0f, 0.0f, 1.0f
							);
							DirectX::XMVECTOR s, q, m_T;
							if (DirectX::XMMatrixDecompose(&s, &q, &t, boneWorldRotMat)) {
								DirectX::XMStoreFloat4(&worldQuat, q);
							}
						}
					}
				}
			}

			// --- エンティティ追従（ボーンなし）---
			if (!hasBoneResult) {
				DirectX::XMFLOAT4X4 m_M;
				DirectX::XMStoreFloat4x4(&m, targetWorldMat);
				worldPos = DirectX::XMVectorSet(m._41, m._42, m._43, 1.0f);

				if (follow->followRotation) {
					// ローカル回転ではなくワールド回転を取得するため行列から分解する
					DirectX::XMVECTOR s, q, m_T;
					if (DirectX::XMMatrixDecompose(&s, &q, &t, targetWorldMat)) {
						DirectX::XMStoreFloat4(&worldQuat, q);
					}
				}
			}

			// オフセットを加算
			DirectX::XMVECTOR m_Offset= DirectX::XMVectorSet(
				follow->positionOffset.x,
				follow->positionOffset.y,
				follow->positionOffset.z,
				0.0f
			);
			DirectX::XMVECTOR m_FinalWorldPos= DirectX::XMVectorAdd(worldPos, offset);

			// --- 位置適用 ---
			if (follow->followPosition) {
				// 親がいる場合は親のワールド逆行列でローカル空間に戻す
				DirectX::XMVECTOR m_LocalPos= finalWorldPos;
				if (transform->parent != 0 && context->entity->IsAlive(transform->parent)) {
					auto* parentTransform =
						context->component->GetComponent<TransformComponent>(transform->parent);
					if (parentTransform) {
						DirectX::XMMATRIX m_ParentWorldMat=
							parentTransform->CalculateWorldMatrix(parentTransform, context->component);
						DirectX::XMMATRIX m_ParentWorldInv=
							DirectX::XMMatrixInverse(nullptr, parentWorldMat);
						localPos = DirectX::XMVector3Transform(finalWorldPos, parentWorldInv);
					}
				}
				DirectX::XMFLOAT3 m_Pos;
				DirectX::XMStoreFloat3(&pos, localPos);
				transform->position = Vector3(pos.x, pos.y, pos.z);
			}

			// --- 回転適用 ---
			if (follow->followRotation) {
				// 親がいる場合はワールド回転を親の逆回転でローカル空間に変換する
				// worldQuat = localQuat * parentQuat → localQuat = worldQuat * inv(parentQuat)
				DirectX::XMVECTOR m_FinalQuat= DirectX::XMLoadFloat4(&worldQuat);
				if (transform->parent != 0 && context->entity->IsAlive(transform->parent)) {
					auto* parentTransform =
						context->component->GetComponent<TransformComponent>(transform->parent);
					if (parentTransform) {
						DirectX::XMMATRIX m_ParentWorldMat=
							parentTransform->CalculateWorldMatrix(parentTransform, context->component);
						DirectX::XMVECTOR pS, pQ, m_PT;
						if (DirectX::XMMatrixDecompose(&pS, &pQ, &pT, parentWorldMat)) {
							finalQuat = DirectX::XMQuaternionMultiply(
								finalQuat, DirectX::XMQuaternionInverse(pQ));
						}
					}
				}
				DirectX::XMFLOAT4 m_LocalQuat;
				DirectX::XMStoreFloat4(&localQuat, finalQuat);
				transform->SetRotation(localQuat);
			}
		}
	}
}
