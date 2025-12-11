// AudioSystem.h
#pragma once
#include "Interface/ISystem.h"
#include "Resources/resourceService.h"
#include "Graphics/GraphicsContext.h"
#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Scene/registry/ComponentRegistry.h"
#include "Component/EffectComponent.h"
#include "Audio/audioContext.h"
#include "Scene/component/transformComponent.h"


class EffectSystem : public ISystem {
public:
	EffectSystem(SceneManagerContext* context)
		: m_context(context) {}

	~EffectSystem() {}

	void Initialize() override {
		m_audioContext = m_context->audio;
	}

	void Finalize() override {
		for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
			auto context = scene->GetSceneContext();
			m_context->graphics->GetEffectManager()->StopAllEffects();

			auto entities = context->component->FindEntitiesWithComponent<EffectComponent>();
			for (auto entity : entities) {
				auto* comp = context->component->GetComponent<EffectComponent>(entity);
				if (comp) {
					comp->Stop(context);
				}
			}
			m_context->graphics->GetEffectManager()->Update(0.0f);
		}
	}

	void Start() override {
		for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
			auto context = scene->GetSceneContext();
			auto entities = context->component->FindEntitiesWithComponent<EffectComponent>();
			for (auto entity : entities) {
				auto* comp = context->component->GetComponent<EffectComponent>(entity);
				if (!comp) continue;

				// AudioDataロードはSystem側でやる
				if (!comp->m_EffectData && !comp->FilePath.empty()) {
					comp->m_EffectData = m_context->resource->Load<EffectData>(comp->FilePath);
				}
			}
		}
	}

	void Update(float dt) override {


		m_context->graphics->GetEffectManager()->Update(dt * 60.0f);

	}

	void FixedUpdate(float dt) override {}
	void Draw() override {

	}
	void EditorUpdate(float dt) override {
		m_context->graphics->GetEffectManager()->BeginUpdate();
		for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
			auto context = scene->GetSceneContext();
			auto entities = context->component->FindEntitiesWithComponent<EffectComponent>();
			for (auto entity : entities) {
				auto* comp = context->component->GetComponent<EffectComponent>(entity);
				if (!comp) continue;

				if (comp->Playing) {
					if (!m_context->graphics->GetEffectManager()->Exists(comp->m_Handle)) {
						comp->Playing = false;
						continue;
					}

					auto* transform = context->component->GetComponent<TransformComponent>(entity);
					if (transform) {

						Vector3 m_Pos = transform->position;
						DirectX::XMFLOAT4 m_RotQuat = transform->GetRotation(); // クォータニオン
						Vector3 m_Scale = transform->scale;

						Effekseer::Matrix43 Matrix, MatrixRot;
						MatrixRot.Indentity(); // 単位行列取得

						// クォータニオンを回転行列に変換
						DirectX::XMVECTOR q = DirectX::XMLoadFloat4(&m_RotQuat);
						DirectX::XMMATRIX rotMat = DirectX::XMMatrixRotationQuaternion(q);

						// XMMATRIX から Effekseer::Matrix43 に変換
						DirectX::XMFLOAT4X4 rotMatF;
						DirectX::XMStoreFloat4x4(&rotMatF, rotMat);

						for (int i = 0; i < 3; ++i) {
							for (int j = 0; j < 3; ++j) {
								MatrixRot.Value[i][j] = rotMatF.m[i][j];
							}
						}

						// スケール・位置ベクトルを Effekseer 用に作成
						Effekseer::Vector3D vec3Ds(m_Scale.x, m_Scale.y, m_Scale.z);
						Effekseer::Vector3D vec3Dt(m_Pos.x, m_Pos.y, m_Pos.z);

						// SRT 行列作成
						Matrix.SetSRT(
							vec3Ds,
							MatrixRot,
							vec3Dt
						);

						// エフェクトに適用
						m_context->graphics->GetEffectManager()->SetBaseMatrix(comp->m_Handle, Matrix);
					}

					m_context->graphics->GetEffectManager()->UpdateHandle(comp->m_Handle, 0.0f);
				} else {
					if (comp->Loop || comp->PlayOnStart) {
						comp->Play(context);
					}
				}
			}
		}
		m_context->graphics->GetEffectManager()->EndUpdate();
	}

private:
	SceneManagerContext* m_context = nullptr;
	AudioContext* m_audioContext = nullptr;
};
