// =======================================================================
// 
// physicSystem.cpp
// 
// =======================================================================
#include "physicSystem.h"
#include <mutex>
#include <Scene/scene.h>
#include <Scene/sceneManager.h>
#include <Scene/Component/ColliderComponent.h>
#include <Scene/Component/transformComponent.h>
#include <Scene/Component/modelRendererComponent.h>
#include <Scene/Component/terrainComponent.h>
#include <Scene/Component/CustomScriptComponent.h>
#include <d3dcompiler.h>

#include "Backends/ImGuiFunc.h"
#include "Editor/Command/CommandManager.h"
#include "Editor/Command/PropertyChangeCommand.h"

#pragma comment(lib, "PhysX_64.lib")
#pragma comment(lib, "PhysXCommon_64.lib")
#pragma comment(lib, "PhysXCooking_64.lib")
#pragma comment(lib, "PhysXExtensions_static_64.lib")
#pragma comment(lib, "PhysXFoundation_64.lib")
#pragma comment(lib, "PhysXPvdSDK_static_64.lib")
#pragma comment(lib, "PhysXTask_static_64.lib")
#pragma comment(lib, "SceneQuery_static_64.lib")
#pragma comment(lib, "SimulationController_static_64.lib")

PhysicSystem* g_physicSystem = nullptr;

physx::PxFilterFlags PhysicsFilterShader(
	physx::PxFilterObjectAttributes attr0, physx::PxFilterData data0,
	physx::PxFilterObjectAttributes attr1, physx::PxFilterData data1,
	physx::PxPairFlags& pairFlags,
	const void*, physx::PxU32) {
	
	int layerA = g_physicSystem->FindLayerIndex(data0.word0);
	int layerB = g_physicSystem->FindLayerIndex(data1.word0);

	if (layerA < 0 || layerB < 0)
		return physx::PxFilterFlag::eSUPPRESS;

	if (!g_physicSystem->GetCollisionEnabled(layerA, layerB))
		return physx::PxFilterFlag::eSUPPRESS;

	// トリガーシェイプが含まれるペアはコンタクトではなくトリガーイベントとして処理する
	if (physx::PxFilterObjectIsTrigger(attr0) || physx::PxFilterObjectIsTrigger(attr1)) {
		pairFlags = physx::PxPairFlag::eTRIGGER_DEFAULT;
		return physx::PxFilterFlag::eDEFAULT;
	}

	pairFlags =
		physx::PxPairFlag::eCONTACT_DEFAULT |
		physx::PxPairFlag::eDETECT_DISCRETE_CONTACT |
		physx::PxPairFlag::eNOTIFY_TOUCH_FOUND |
		physx::PxPairFlag::eNOTIFY_TOUCH_PERSISTS |
		physx::PxPairFlag::eNOTIFY_TOUCH_LOST;

	return physx::PxFilterFlag::eDEFAULT;
}

// PhysX シミュレーションイベントコールバック
class PhysicsSimulationCallback : public physx::PxSimulationEventCallback {

	static std::vector<CustomScriptComponent*> GetScripts(SceneContext* ctx, Entity entity) {
		std::vector<CustomScriptComponent*> result;
		if (!ctx) return result;
		for (auto& [e, script] : ctx->component->GetAllBaseComponents<CustomScriptComponent>()) {
			if (e == entity && script && script->IsInitialized())
				result.push_back(script);
		}
		return result;
	}

public:
	// Stop/Finalize 前に false にセットすることでコールバックを無効化する
	bool m_active = true;

	void onContact(const physx::PxContactPairHeader& pairHeader,
				   const physx::PxContactPair* pairs,
				   physx::PxU32 nbPairs) override
	{
		if (!m_active) return;
		// 削除されたアクターへのアクセスを防ぐ
		if (pairHeader.flags & (physx::PxContactPairHeaderFlag::eREMOVED_ACTOR_0 |
								physx::PxContactPairHeaderFlag::eREMOVED_ACTOR_1))
			return;
		auto* infoA = static_cast<ActorEntityInfo*>(pairHeader.actors[0]->userData);
		auto* infoB = static_cast<ActorEntityInfo*>(pairHeader.actors[1]->userData);
		if (!infoA || !infoB) return;

		EntityRef refA(infoA->entity, infoA->context);
		EntityRef refB(infoB->entity, infoB->context);

		for (physx::PxU32 i = 0; i < nbPairs; ++i) {
			const physx::PxContactPair& pair = pairs[i];
			// 削除されたシェイプへのアクセスを防ぐ
			if (pair.flags & (physx::PxContactPairFlag::eREMOVED_SHAPE_0 |
							  physx::PxContactPairFlag::eREMOVED_SHAPE_1))
				continue;
			uint32_t layerA = pair.shapes[0]->getSimulationFilterData().word0;
			uint32_t layerB = pair.shapes[1]->getSimulationFilterData().word0;

			HitInfo hitForA{ refB, layerB };
			HitInfo hitForB{ refA, layerA };

			auto scriptsA = GetScripts(infoA->context, infoA->entity);
			auto scriptsB = GetScripts(infoB->context, infoB->entity);

			if (pair.events & physx::PxPairFlag::eNOTIFY_TOUCH_FOUND) {
				for (auto* s : scriptsA) s->CollisionEnter(hitForA);
				for (auto* s : scriptsB) s->CollisionEnter(hitForB);
			}
			if (pair.events & physx::PxPairFlag::eNOTIFY_TOUCH_PERSISTS) {
				for (auto* s : scriptsA) s->CollisionStay(hitForA);
				for (auto* s : scriptsB) s->CollisionStay(hitForB);
			}
			if (pair.events & physx::PxPairFlag::eNOTIFY_TOUCH_LOST) {
				for (auto* s : scriptsA) s->CollisionExit(hitForA);
				for (auto* s : scriptsB) s->CollisionExit(hitForB);
			}
		}
	}

	void onTrigger(physx::PxTriggerPair* pairs, physx::PxU32 count) override {
		if (!m_active) return;
		for (physx::PxU32 i = 0; i < count; ++i) {
			const physx::PxTriggerPair& pair = pairs[i];
			if (pair.flags & (physx::PxTriggerPairFlag::eREMOVED_SHAPE_TRIGGER |
							  physx::PxTriggerPairFlag::eREMOVED_SHAPE_OTHER))
				continue;

			auto* triggerInfo = static_cast<ActorEntityInfo*>(pair.triggerActor->userData);
			auto* otherInfo   = static_cast<ActorEntityInfo*>(pair.otherActor->userData);
			if (!triggerInfo || !otherInfo) continue;

			uint32_t triggerLayer = pair.triggerShape->getSimulationFilterData().word0;
			uint32_t otherLayer   = pair.otherShape->getSimulationFilterData().word0;

			EntityRef refTrigger(triggerInfo->entity, triggerInfo->context);
			EntityRef refOther  (otherInfo->entity,   otherInfo->context);

			HitInfo hitForTrigger{ refOther,   otherLayer   };
			HitInfo hitForOther  { refTrigger, triggerLayer };

			auto scriptsTrigger = GetScripts(triggerInfo->context, triggerInfo->entity);
			auto scriptsOther   = GetScripts(otherInfo->context,   otherInfo->entity);

			if (pair.status & physx::PxPairFlag::eNOTIFY_TOUCH_FOUND) {
				for (auto* s : scriptsTrigger) s->TriggerEnter(hitForTrigger);
				for (auto* s : scriptsOther)   s->TriggerEnter(hitForOther);
			} else if (pair.status & physx::PxPairFlag::eNOTIFY_TOUCH_LOST) {
				for (auto* s : scriptsTrigger) s->TriggerExit(hitForTrigger);
				for (auto* s : scriptsOther)   s->TriggerExit(hitForOther);
			}
		}
	}

	void onConstraintBreak(physx::PxConstraintInfo*, physx::PxU32) override {}
	void onWake(physx::PxActor**, physx::PxU32) override {}
	void onSleep(physx::PxActor**, physx::PxU32) override {}
	void onAdvance(const physx::PxRigidBody* const*, const physx::PxTransform*, physx::PxU32) override {}
};

physx::PxRigidDynamic* PhysicSystem::CreateDynamic(const physx::PxTransform& t, const physx::PxGeometry& geometry, physx::PxMaterial& material, physx::PxReal density){
	physx::PxRigidDynamic* rigid_dynamic = PxCreateDynamic(*g_pPhysics, t, geometry, material, density);
	g_pScene->addActor(*rigid_dynamic);
	return rigid_dynamic;
}

physx::PxRigidStatic* PhysicSystem::CreateStatic(const physx::PxTransform& t, const physx::PxGeometry& geometry, physx::PxMaterial& material){
	physx::PxRigidStatic* rigid_static = PxCreateStatic(*g_pPhysics, t, geometry, material);
	g_pScene->addActor(*rigid_static);
	return rigid_static;
}

// PX_MIN_HEIGHTFIELD_Y_SCALE  = (0.0001f / PxReal(0xFFFF)) — PxReal not in scope here
static constexpr float kMinHeightFieldYScale  = 0.0001f / 65535.0f;
// PX_MIN_HEIGHTFIELD_XZ_SCALE = 1e-8f
static constexpr float kMinHeightFieldXZScale = 1e-8f;

// =============================================================
// TerrainComponent の HeightMap から PxHeightField を構築
// row → X軸, column → Z軸 に対応 (PxHeightFieldDesc の仕様通り)
// =============================================================
static void BuildTerrainHeightField(ColliderShape& col, TerrainComponent* terrain) {
	if (!terrain) return;

	// 古い HeightField を解放
	if (col.pxHeightField) {
		col.pxHeightField->release();
		col.pxHeightField = nullptr;
	}

	int gridSize = terrain->Scale;
	physx::PxU32 nbRows = (physx::PxU32)(gridSize + 1);  // row → X 軸
	physx::PxU32 nbCols = (physx::PxU32)(gridSize + 1);  // col → Z 軸

	const auto& hm = terrain->HeightMap;
	if ((physx::PxU32)hm.size() < nbRows * nbCols) return;

	// PxI16 サンプルに変換 (100 倍でスケーリング、CreatePxShape 側で 1/100 を heightScale に使用)
	std::vector<physx::PxHeightFieldSample> samples((size_t)nbRows * nbCols);
	for (physx::PxU32 r = 0; r < nbRows; ++r) {       // r = x
		for (physx::PxU32 c = 0; c < nbCols; ++c) {   // c = z
			// terrain の HeightMap インデックス: x + (gridSize - z) * (gridSize + 1)
			int hmIdx = (int)r + (gridSize - (int)c) * (gridSize + 1);
			float h = (hmIdx >= 0 && hmIdx < (int)hm.size()) ? hm[hmIdx] : 0.0f;
			samples[r * nbCols + c].height = (physx::PxI16)std::clamp(std::round(h * 100.0f), -32768.0f, 32767.0f);
			samples[r * nbCols + c].materialIndex0 = 0;
			samples[r * nbCols + c].materialIndex1 = 0;
		}
	}

	physx::PxHeightFieldDesc hfDesc;
	hfDesc.format       = physx::PxHeightFieldFormat::eS16_TM;
	hfDesc.nbRows       = nbRows;
	hfDesc.nbColumns    = nbCols;
	hfDesc.samples.data   = samples.data();
	hfDesc.samples.stride = sizeof(physx::PxHeightFieldSample);

	col.pxHeightField = PxCreateHeightField(hfDesc);
	if (!col.pxHeightField) {
		OutputDebugStringA("BuildTerrainHeightField: PxCreateHeightField failed\n");
	}
}


void PhysicSystem::BuildMeshCollider(ColliderShape& col, ModelRendererComponent* modelRenderer) {
	if (!modelRenderer || !modelRenderer->model || !modelRenderer->model->AiScene) return;

	// 古いメッシュを解放
	if (col.pxTriangleMesh) {
		col.pxTriangleMesh->release();
		col.pxTriangleMesh = nullptr;
	}
	if (col.pxConvexMesh) {
		col.pxConvexMesh->release();
		col.pxConvexMesh = nullptr;
	}

	const aiScene* scene = modelRenderer->model->AiScene;

	// 全メッシュの頂点・インデックスを収集
	std::vector<physx::PxVec3> vertices;
	std::vector<physx::PxU32>  indices;

	for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
		const aiMesh* mesh = scene->mMeshes[m];
		physx::PxU32 baseIndex = static_cast<physx::PxU32>(vertices.size());

		for (unsigned int v = 0; v < mesh->mNumVertices; ++v) {
			const aiVector3D& p = mesh->mVertices[v];
			vertices.push_back(physx::PxVec3(p.x, p.y, p.z));
		}
		for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
			const aiFace& face = mesh->mFaces[f];
			if (face.mNumIndices != 3) continue;
			indices.push_back(baseIndex + face.mIndices[0]);
			indices.push_back(baseIndex + face.mIndices[1]);
			indices.push_back(baseIndex + face.mIndices[2]);
		}
	}

	if (vertices.empty() || indices.empty()) return;

	physx::PxCookingParams params(g_pPhysics->getTolerancesScale());

	// Triangle mesh (used for static actors)
	physx::PxTriangleMeshDesc meshDesc;
	meshDesc.points.count  = (physx::PxU32)vertices.size();
	meshDesc.points.stride = sizeof(physx::PxVec3);
	meshDesc.points.data   = vertices.data();

	meshDesc.triangles.count  = (physx::PxU32)(indices.size() / 3);
	meshDesc.triangles.stride = sizeof(physx::PxU32) * 3;
	meshDesc.triangles.data   = indices.data();

	col.pxTriangleMesh = PxCreateTriangleMesh(
		params,
		meshDesc,
		g_pPhysics->getPhysicsInsertionCallback()
	);

	if (!col.pxTriangleMesh) {
		OutputDebugStringA("BuildMeshCollider: PxCreateTriangleMesh failed\n");
	}

	// Convex mesh (used for dynamic actors — convex hulls support non-kinematic PxRigidDynamic)
	// eCOMPUTE_CONVEX tells PhysX to compute the convex hull from the supplied vertices.
	physx::PxConvexMeshDesc convexDesc;
	convexDesc.points.count  = (physx::PxU32)vertices.size();
	convexDesc.points.stride = sizeof(physx::PxVec3);
	convexDesc.points.data   = vertices.data();
	convexDesc.flags         = physx::PxConvexFlag::eCOMPUTE_CONVEX;

	col.pxConvexMesh = PxCreateConvexMesh(
		params,
		convexDesc,
		g_pPhysics->getPhysicsInsertionCallback()
	);

	if (!col.pxConvexMesh) {
		OutputDebugStringA("BuildMeshCollider: PxCreateConvexMesh failed\n");
	}
}


physx::PxShape* PhysicSystem::CreatePxShape(
	physx::PxRigidActor* actor,
	const ColliderShape& col,
	const Vector3& scale,
	physx::PxMaterial& material){
	if(!actor) return nullptr;

	physx::PxShape* shape = nullptr;

	switch(col.type){
		case ColliderType::BOX:
			shape = physx::PxRigidActorExt::createExclusiveShape(
				*actor,
				physx::PxBoxGeometry(
					col.size.x * 0.5f * scale.x,
					col.size.y * 0.5f * scale.y,
					col.size.z * 0.5f * scale.z),
				material
			);
			break;

		case ColliderType::SPHERE:
		{
			float r = col.radius * (std::max)({scale.x, scale.y, scale.z});
			shape = physx::PxRigidActorExt::createExclusiveShape(
				*actor,
				physx::PxSphereGeometry(r),
				material
			);
			break;
		}

		case ColliderType::CAPSULE:
		{
			float rxz = (std::max)(scale.x, scale.z);
			float ry = scale.y;
			shape = physx::PxRigidActorExt::createExclusiveShape(
				*actor,
				physx::PxCapsuleGeometry(col.radius * rxz, col.height * 0.5f * ry),
				material
			);
			break;
		}

		case ColliderType::MESH:
			if (actor->getType() == physx::PxActorType::eRIGID_DYNAMIC) {
				// Dynamic actors require a convex mesh — triangle meshes are not supported as
				// eSIMULATION_SHAPE on non-kinematic PxRigidDynamic. A convex hull allows
				// gravity and forces to apply normally.
				if (col.pxConvexMesh) {
					physx::PxConvexMeshGeometry geom(
						col.pxConvexMesh,
						physx::PxMeshScale(physx::PxVec3(scale.x, scale.y, scale.z))
					);
					shape = physx::PxRigidActorExt::createExclusiveShape(*actor, geom, material);
				}
			} else {
				// Static actors support the full triangle mesh for exact collision.
				if (col.pxTriangleMesh) {
					physx::PxTriangleMeshGeometry geom(
						col.pxTriangleMesh,
						physx::PxMeshScale(physx::PxVec3(scale.x, scale.y, scale.z)),
						physx::PxMeshGeometryFlags(physx::PxMeshGeometryFlag::eDOUBLE_SIDED)
					);
					shape = physx::PxRigidActorExt::createExclusiveShape(*actor, geom, material);
				}
			}
			break;

		case ColliderType::HEIGHT_MAP:
		{
			if (col.pxHeightField) {
				// Heightfield shapes cannot be used as eSIMULATION_SHAPE on a non-kinematic
				// PxRigidDynamic. Promote the actor to kinematic before attaching.
				if (actor->getType() == physx::PxActorType::eRIGID_DYNAMIC) {
					static_cast<physx::PxRigidDynamic*>(actor)->setRigidBodyFlag(
						physx::PxRigidBodyFlag::eKINEMATIC, true);
				}
				physx::PxU32 nbRows    = col.pxHeightField->getNbRows();
				physx::PxU32 nbCols    = col.pxHeightField->getNbColumns();
				int gridSize = (int)nbRows - 1;
				if (gridSize > 0) {
					// heightScale: PxI16 サンプルは float * 100 で作成したので 1/100 * scale.y を使用
					float heightScale = (std::max)(scale.y / 100.0f, kMinHeightFieldYScale);
					// rowScale: row → X軸, gridSize セル分が scale.x 世界単位に対応
					float rowScale = (std::max)(scale.x / (float)gridSize, kMinHeightFieldXZScale);
					// colScale: col → Z軸, gridSize セル分が scale.z 世界単位に対応
					float colScaleVal = (std::max)(scale.z / (float)gridSize, kMinHeightFieldXZScale);

					physx::PxHeightFieldGeometry hfGeom(
						col.pxHeightField,
						physx::PxMeshGeometryFlags(physx::PxMeshGeometryFlag::eDOUBLE_SIDED),
						heightScale, rowScale, colScaleVal
					);
					shape = physx::PxRigidActorExt::createExclusiveShape(*actor, hfGeom, material);
				}
			}
			break;
		}
	}

	// =============================================================
	//  回転オフセットの反映（オイラー角 → PxQuat）
	// =============================================================
	if(shape){
		physx::PxVec3 offset(
			col.offset.x * scale.x,
			col.offset.y * scale.y,
			col.offset.z * scale.z
		);

		// HeightMap は原点がコーナーにあるため、地形メッシュ中心に合わせるオフセットを加算
		if (col.type == ColliderType::HEIGHT_MAP) {
			offset.x -= 0.5f * scale.x;
			offset.z -= 0.5f * scale.z;
		}

		// オイラー角をラジアンに変換
		const float DegToRad = physx::PxPi / 180.0f;
		physx::PxVec3 eulerRad(
			col.rotationOffset.x * DegToRad,
			col.rotationOffset.y * DegToRad,
			col.rotationOffset.z * DegToRad
		);

		// XYZ順でクォータニオンを合成
		physx::PxQuat qx(eulerRad.x, physx::PxVec3(1, 0, 0));
		physx::PxQuat qy(eulerRad.y, physx::PxVec3(0, 1, 0));
		physx::PxQuat qz(eulerRad.z, physx::PxVec3(0, 0, 1));

		// 回転順序：Z→Y→X（一般的な右手座標系向け）
		physx::PxQuat rot = qz * qy * qx;

		// オフセット＋回転をローカルポーズとして設定
		shape->setLocalPose(physx::PxTransform(offset, rot));
	}

	return shape;
}

// =============================================================
// Collider の更新（PhysXへの反映）
// =============================================================
void PhysicSystem::UpdateColliderParam(TransformComponent* transform, ColliderComponent* collider, size_t entity, size_t index){
	OutputDebugStringA("PhysicSystem::UpdateColliderParam\n");

	if(!collider) return;
	if(index >= collider->colliders.size()) return;

	ColliderShape& col = collider->colliders[index];
	physx::PxRigidActor* actor = collider->isDynamic ?
		static_cast<physx::PxRigidActor*>(collider->pRigidbodyDynamic) :
		static_cast<physx::PxRigidActor*>(collider->pRigidbodyStatic);

	if(!actor) return;

	g_pScene->lockWrite();

	// 1) 古い shape を安全に detach
	if(col.pxShape){
		actor->detachShape(*col.pxShape);
		col.pxShape = nullptr;
	}

	// 2) 古い material 解放
	if(col.pxMaterial){
		col.pxMaterial->release();
		col.pxMaterial = nullptr;
	}

	// 3) 新しい material 作成
	physx::PxMaterial* material = g_pPhysics->createMaterial(
		col.staticFriction, col.dynamicFriction, col.restitution
	);
	col.pxMaterial = material;

	// 4) Transform スケール取得
	Vector3 scale = transform ? transform->scale : Vector3{1.0f, 1.0f, 1.0f};

	// 5) 新しい shape 作成
	physx::PxShape* newShape = CreatePxShape(actor, col, scale, *material);
	col.pxShape = newShape;

	// 6) レイヤー設定
	if (newShape) {
		physx::PxFilterData fd;
		fd.word0 = col.collisionLayer;

		newShape->setSimulationFilterData(fd);
		newShape->setQueryFilterData(fd);

		if (col.isTrigger) {
			newShape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, false);
			newShape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, true);
		} else {
			newShape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, true);
			newShape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, false);
		}

	}


	// 7) 回転ロック設定
	if(collider->isDynamic){
		physx::PxRigidDynamic* dyn = static_cast<physx::PxRigidDynamic*>(actor);
		physx::PxRigidDynamicLockFlags lockFlags;
		if(col.lockRotX) lockFlags |= physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_X;
		if(col.lockRotY) lockFlags |= physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y;
		if(col.lockRotZ) lockFlags |= physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z;
		dyn->setRigidDynamicLockFlags(lockFlags);
	}

	g_pScene->unlockWrite();
}



void PhysicSystem::Initialize(){

	g_physicSystem = this;

	// --- デフォルトレイヤー ---
	m_layers.clear();
	m_layers.push_back({ "Default",  1u << 0 });
	m_layers.push_back({ "Player",   1u << 1 });
	m_layers.push_back({ "Enemy",    1u << 2 });
	m_layers.push_back({ "Environment", 1u << 3 });

	for (uint32_t i = 0; i < kMaxPhysicsLayers; ++i) {
		for (uint32_t j = 0; j < kMaxPhysicsLayers; ++j) {
			m_collisionMatrix[i][j] = true; // デフォルトは全衝突
		}
	}

	UpdatingPhysics = false;
	g_pFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, g_defaultAllocator, g_defaultErrorCallback);

	if(g_pFoundation){
		g_pPvd = physx::PxCreatePvd(*g_pFoundation);
		physx::PxPvdTransport* transport = 
			physx::PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
			//physx::PxDefaultPvdFileTransportCreate("physx_capture.pvd");

		g_pPvd->connect(*transport, physx::PxPvdInstrumentationFlag::eALL);
	}

	g_pPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *g_pFoundation, physx::PxTolerancesScale(), true, g_pPvd);
	PxInitExtensions(*g_pPhysics, g_pPvd);

	g_pDispatcher = physx::PxDefaultCpuDispatcherCreate(8);
	physx::PxSceneDesc scene_desc(g_pPhysics->getTolerancesScale());
	scene_desc.gravity = physx::PxVec3(0, Gravity, 0);
	scene_desc.filterShader = PhysicsFilterShader;
	scene_desc.cpuDispatcher = g_pDispatcher;
	m_simCallback = new PhysicsSimulationCallback();
	scene_desc.simulationEventCallback = m_simCallback;

	g_pScene = g_pPhysics->createScene(scene_desc);
	if(g_pScene){
		g_pScene->setVisualizationParameter(physx::PxVisualizationParameter::eSCALE, 1.0f);
		g_pScene->setVisualizationParameter(physx::PxVisualizationParameter::eCOLLISION_SHAPES, 1.0f);
		g_pScene->setVisualizationParameter(physx::PxVisualizationParameter::eACTOR_AXES, 1.0f);
	}

	if(g_pScene){
		physx::PxPvdSceneClient* pvd_client = g_pScene->getScenePvdClient();
		pvd_client->setScenePvdFlag(physx::PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
		pvd_client->setScenePvdFlag(physx::PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
		pvd_client->setScenePvdFlag(physx::PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
	}
}
void PhysicSystem::Finalize(){
	OutputDebugStringA("PhysicSystem::Finalize\n");
	if (m_simCallback) m_simCallback->m_active = false;
	for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
		auto context = scene->GetSceneContext();
		const auto& colliderEntity = context->component->FindEntitiesWithComponent<ColliderComponent>();
		for (Entity entity : colliderEntity) {
			auto Collider = context->component->GetComponent<ColliderComponent>(entity);
			for (auto& col : Collider->colliders) {
				if (col.pxMaterial) {
					OutputDebugStringA(("Finalize Release Material: " + std::to_string((uintptr_t)col.pxMaterial) + "\n").c_str());
					col.pxMaterial->release();
					col.pxMaterial = nullptr;
				}
				if (col.pxShape) {
					OutputDebugStringA(("Finalize Shape Pointer Clear: " + std::to_string((uintptr_t)col.pxShape) + "\n").c_str());
					col.pxShape = nullptr; // Actor release に任せる
				}
				if (col.pxHeightField) {
					col.pxHeightField->release();
					col.pxHeightField = nullptr;
				}
			}
			if (Collider->pRigidbodyStatic) {
				OutputDebugStringA(("Finalize Release Static Actor: " + std::to_string((uintptr_t)Collider->pRigidbodyStatic) + "\n").c_str());
				delete static_cast<ActorEntityInfo*>(Collider->pRigidbodyStatic->userData);
				Collider->pRigidbodyStatic->release();
				Collider->pRigidbodyStatic = nullptr;
			}
			if (Collider->pRigidbodyDynamic) {
				OutputDebugStringA(("Finalize Release Dynamic Actor: " + std::to_string((uintptr_t)Collider->pRigidbodyDynamic) + "\n").c_str());
				delete static_cast<ActorEntityInfo*>(Collider->pRigidbodyDynamic->userData);
				Collider->pRigidbodyDynamic->release();
				Collider->pRigidbodyDynamic = nullptr;
			}
		}
	}
	PxCloseExtensions();
	if(g_pScene){
		OutputDebugStringA(("Finalize Release Scene: " + std::to_string((uintptr_t)g_pScene) + "\n").c_str());
		g_pScene->release();
		g_pScene = nullptr;
	}
	if(g_pDispatcher){
		OutputDebugStringA(("Finalize Release Dispatcher: " + std::to_string((uintptr_t)g_pDispatcher) + "\n").c_str());
		g_pDispatcher->release();
		g_pDispatcher = nullptr;
	}
	if(g_pPhysics){
		OutputDebugStringA(("Finalize Release Physics: " + std::to_string((uintptr_t)g_pPhysics) + "\n").c_str());
		g_pPhysics->release();
		g_pPhysics = nullptr;
	}

	if(g_pPvd){
		OutputDebugStringA(("Finalize Disconnect PVD: " + std::to_string((uintptr_t)g_pPvd) + "\n").c_str());
		g_pPvd->disconnect();
		physx::PxPvdTransport* transport = g_pPvd->getTransport();
		if(transport){
			OutputDebugStringA(("Finalize Release PVD Transport: " + std::to_string((uintptr_t)transport) + "\n").c_str());
			transport->release();
		}
		g_pPvd->release();
		g_pPvd = nullptr;
	}

	if(g_pFoundation){
		OutputDebugStringA(("Finalize Release Foundation: " + std::to_string((uintptr_t)g_pFoundation) + "\n").c_str());
		g_pFoundation->release();
		g_pFoundation = nullptr;
	}

	g_physicSystem = nullptr;

	delete m_simCallback;
	m_simCallback = nullptr;
}

void PhysicSystem::Stop(){
	// コールバックを無効化してから Actor を解放する
	// （fetchResults が後で発火しても onContact/onTrigger を呼ばないようにする）
	if (m_simCallback) m_simCallback->m_active = false;

	for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
		auto context = scene->GetSceneContext();
		const auto& colliderEntity = context->component->FindEntitiesWithComponent<ColliderComponent>();
		for (Entity entity : colliderEntity) {
			auto Collider = context->component->GetComponent<ColliderComponent>(entity);

			for (auto& col : Collider->colliders) {
				if (col.pxMaterial) {
					OutputDebugStringA(("Stop Release Material: " + std::to_string((uintptr_t)col.pxMaterial) + "\n").c_str());
					col.pxMaterial->release();
					col.pxMaterial = nullptr;
				}
				if (col.pxShape) {
					OutputDebugStringA(("Stop Shape Pointer Clear: " + std::to_string((uintptr_t)col.pxShape) + "\n").c_str());
					col.pxShape = nullptr;
				}
				if (col.pxHeightField) {
					col.pxHeightField->release();
					col.pxHeightField = nullptr;
				}
			}

			if (Collider->pRigidbodyStatic) {
				OutputDebugStringA(("Stop Release Static Actor: " + std::to_string((uintptr_t)Collider->pRigidbodyStatic) + "\n").c_str());
				delete static_cast<ActorEntityInfo*>(Collider->pRigidbodyStatic->userData);
				Collider->pRigidbodyStatic->release();
				Collider->pRigidbodyStatic = nullptr;
			}
			if (Collider->pRigidbodyDynamic) {
				OutputDebugStringA(("Stop Release Dynamic Actor: " + std::to_string((uintptr_t)Collider->pRigidbodyDynamic) + "\n").c_str());
				delete static_cast<ActorEntityInfo*>(Collider->pRigidbodyDynamic->userData);
				Collider->pRigidbodyDynamic->release();
				Collider->pRigidbodyDynamic = nullptr;
			}
		}
	}

	g_pScene->lockWrite();
	g_pScene->lockRead();
	g_pScene->simulate(1.0f);
	g_pScene->fetchResults(true);
	g_pScene->unlockWrite();
	g_pScene->unlockRead();
}

YAML::Node PhysicSystem::encode() {
	YAML::Node node;
	for (auto& layer : m_layers) {
		YAML::Node l;
		l["name"] = layer.name;
		l["bit"] = layer.bit;
		node["layers"].push_back(l);
	}

	const int count = static_cast<int>(m_layers.size());

	for (int y = 0; y < count; ++y) {
		for (int x = 0; x < count; ++x) {
			node["collision"][y][x] = m_collisionMatrix[y][x];
		}
	}

	return node;
}

bool PhysicSystem::decode(const YAML::Node& node) {
	if (!node["layers"]) return true;

	m_layers.clear();
	for (auto& l : node["layers"]) {
		PhysicsLayer layer;
		layer.name = l["name"].as<std::string>();
		layer.bit = l["bit"].as<uint32_t>();
		m_layers.push_back(layer);
	}
	return true;
}

void PhysicSystem::SystemSetting(){

	DrawLayerEditor();

	const int count = static_cast<int>(m_layers.size());
	if(count == 0)
		return;

	if(ImGui::TreeNode("Layer Collision Matrix")){



		const int columnCount = count + 1;

		ImVec2 tableSize = ImVec2(0.0f, 400.0f);

		if(ImGui::BeginTable("CollisionMatrixTable",
							 columnCount,
							 ImGuiTableFlags_Borders |
							 ImGuiTableFlags_RowBg |
							 ImGuiTableFlags_SizingFixedFit |
							 ImGuiTableFlags_ScrollX |
							 ImGuiTableFlags_ScrollY
							 //ImGuiTableFlags_ScrollFreezeTopRow
							 ,
							 tableSize)){
			// 列定義
			ImGui::TableSetupColumn("Layer",
									ImGuiTableColumnFlags_WidthFixed,
									120.0f);

			for(int i = 0; i < count; ++i){
				ImGui::TableSetupColumn("##col",
										ImGuiTableColumnFlags_WidthFixed,
										40.0f);
			}

			// ヘッダ行（高さ固定）
			ImGui::TableNextRow(ImGuiTableRowFlags_None, 80.0f);

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("Layer");

			for(int i = 0; i < count; ++i){
				ImGui::TableSetColumnIndex(i + 1);
				ImGui::DrawVerticalText(m_layers[i].name.c_str());
			}

			// 本体
			for(int y = 0; y < count; ++y){
				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::Text("%s", m_layers[y].name.c_str());

				for(int x = 0; x < count; ++x){
					ImGui::TableSetColumnIndex(x + 1);

					ImGui::PushID(y * 1000 + x);

					bool v = m_collisionMatrix[y][x];
					bool oldV = v;

					ImGui::Checkbox("##chk", &v);
					if(ImGui::IsItemDeactivatedAfterEdit()){
						if(v != oldV){
							m_collisionMatrix[y][x] = v;
							m_collisionMatrix[x][y] = v;
							if(auto* mgr = ImGui::GetCommandManager()){
								// 対称セル（y,x と x,y）を同時に変更するためセッター形式を使う
								// 既に値は適用済みなので Push（Execute を呼ばない）で積む
								auto* mat = &m_collisionMatrix;
								int capturedY = y, capturedX = x;
								mgr->Push(std::make_unique<PropertyChangeCommandWithSetter<bool>>(
									[mat, capturedY, capturedX](const bool& val){
										(*mat)[capturedY][capturedX] = val;
										(*mat)[capturedX][capturedY] = val;
									},
									oldV, v, "衝突レイヤー設定"
								));
							}
						}
					}

					ImGui::PopID();
				}
			}

			ImGui::EndTable();
		}
		ImGui::TreePop();
	}
}

bool PhysicSystem::GetCollisionEnabled(uint32_t a, uint32_t b) const {
	return m_collisionMatrix[a][b];
}


// レイキャスト用除外フィルタ: layerMask に含まれるレイヤーを持つ Shape を除外する
// layerMask = 0 の場合はすべての Shape がヒット対象になる
class ExcludeMaskFilter : public physx::PxQueryFilterCallback {
	physx::PxU32 m_excludeMask;
public:
	explicit ExcludeMaskFilter(physx::PxU32 excludeMask) : m_excludeMask(excludeMask) {}

	physx::PxQueryHitType::Enum preFilter(
		const physx::PxFilterData&, const physx::PxShape* shape,
		const physx::PxRigidActor*, physx::PxHitFlags&) override
	{
		if (m_excludeMask != 0 && (shape->getQueryFilterData().word0 & m_excludeMask) != 0)
			return physx::PxQueryHitType::eNONE;
		return physx::PxQueryHitType::eBLOCK;
	}

	physx::PxQueryHitType::Enum postFilter(
		const physx::PxFilterData&, const physx::PxQueryHit&,
		const physx::PxShape*, const physx::PxRigidActor*) override
	{
		return physx::PxQueryHitType::eBLOCK;
	}
};

RayHit PhysicSystem::RaycastWithMask(const physx::PxVec3& origin, const physx::PxVec3& direction, physx::PxReal maxDistance, physx::PxU32 layerMask) {
	RayHit result{};
	result.hit = false;

	physx::PxVec3 dirNorm = direction;
	if (dirNorm.normalize() < 1e-6f) return result;

	// layerMask は「除外」マスク: 対応するビットを持つレイヤーの Shape を除外する
	// eDISABLE_HARDCODED_FILTER を使い word0=0 の Shape（地形など）が
	// ハードコードフィルタでスキップされないようにする
	physx::PxQueryFilterData filterData;
	filterData.data.word0 = 0;
	filterData.flags |= physx::PxQueryFlag::ePREFILTER
					 |  physx::PxQueryFlag::eDISABLE_HARDCODED_FILTER;

	ExcludeMaskFilter filter(layerMask);
	physx::PxRaycastBuffer hitBuffer;

	bool status = g_pScene->raycast(
		origin,
		dirNorm,
		maxDistance,
		hitBuffer,
		physx::PxHitFlags(physx::PxHitFlag::eDEFAULT | physx::PxHitFlag::eMESH_BOTH_SIDES),
		filterData,
		&filter
	);

	if (status && hitBuffer.hasBlock) {
		const physx::PxRaycastHit& b = hitBuffer.block;
		result.hit = true;
		result.position = b.position;
		result.normal = b.normal;
		result.distance = b.distance;
		result.hitShape = b.shape;
		result.hitActor = b.actor;
	}
	return result;
}

uint32_t PhysicSystem::GetLayerBit(const std::string& name) const {
	for (const auto& layer : m_layers) {
		if (layer.name == name)
			return layer.bit;
	}
	return 0;
}

int PhysicSystem::FindLayerIndex(uint32_t bit) const{
	if(bit == 0){
		return 0;
	}

	for(int i = 0; i < m_layers.size(); ++i){
		if(m_layers[i].bit == bit)
			return i;
	}
	return -1;
}

// bit再構築
void PhysicSystem::RebuildLayerBits(){
	for(int i = 0; i < (int)m_layers.size(); ++i){
		m_layers[i].bit = (1u << i);
	}
}

// マトリクス再構築
void PhysicSystem::RebuildCollisionMatrix(){
	const int count = (int)m_layers.size();

	// 一旦バックアップ
	bool old[kMaxPhysicsLayers][kMaxPhysicsLayers]{};

	for(int y = 0; y < kMaxPhysicsLayers; ++y)
		for(int x = 0; x < kMaxPhysicsLayers; ++x)
			old[y][x] = m_collisionMatrix[y][x];

	// 全初期化
	for(int y = 0; y < kMaxPhysicsLayers; ++y)
		for(int x = 0; x < kMaxPhysicsLayers; ++x)
			m_collisionMatrix[y][x] = false;

	// 有効範囲だけ復元
	for(int y = 0; y < count; ++y){
		for(int x = 0; x < count; ++x){
			m_collisionMatrix[y][x] = old[y][x];
		}
	}
}

void PhysicSystem::RemoveLayer(int removeIndex){
	const int count = (int)m_layers.size();
	if(removeIndex < 0 || removeIndex >= count)
		return;

	// レイヤー削除
	m_layers.erase(m_layers.begin() + removeIndex);

	// 行シフト
	for(int y = removeIndex; y < count - 1; ++y){
		for(int x = 0; x < count; ++x){
			m_collisionMatrix[y][x] = m_collisionMatrix[y + 1][x];
		}
	}

	// 列シフト
	for(int x = removeIndex; x < count - 1; ++x){
		for(int y = 0; y < count - 1; ++y){
			m_collisionMatrix[y][x] = m_collisionMatrix[y][x + 1];
		}
	}

	// 末尾クリア
	for(int i = 0; i < kMaxPhysicsLayers; ++i){
		m_collisionMatrix[count - 1][i] = false;
		m_collisionMatrix[i][count - 1] = false;
	}

	RebuildLayerBits();
}
void PhysicSystem::AddLayer(const std::string& name){
	if((int)m_layers.size() >= kMaxPhysicsLayers)
		return;

	PhysicsLayer layer;
	layer.name = name;
	layer.bit = 0;

	m_layers.push_back(layer);

	int newIndex = (int)m_layers.size() - 1;

	// 新規レイヤーは全衝突ONにする例
	for(int i = 0; i <= newIndex; ++i){
		m_collisionMatrix[newIndex][i] = true;
		m_collisionMatrix[i][newIndex] = true;
	}

	RebuildLayerBits();
}
void PhysicSystem::Start(){
	// コールバックを有効化する（Stop 後の再起動にも対応）
	if (m_simCallback) m_simCallback->m_active = true;

	for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
		auto context = scene->GetSceneContext();
		const auto& colliderEntity = context->component->FindEntitiesWithComponent<ColliderComponent>();
		if (colliderEntity.empty()) return;
		for (Entity entity : colliderEntity) {
			auto Collider = context->component->GetComponent<ColliderComponent>(entity);

			Collider->needsUpdate = true;
		}
	}
	UpdateCollider();
}

void PhysicSystem::UpdateCollider() {
	for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
		auto context = scene->GetSceneContext();
		const auto& colliderEntity = context->component->FindEntitiesWithComponent<ColliderComponent>();
		if (colliderEntity.empty()) continue;

		for (Entity entity : colliderEntity) {
			auto Collider = context->component->GetComponent<ColliderComponent>(entity);
			auto Transform = context->component->GetComponent<TransformComponent>(entity);
			if (!Transform) continue;

			physx::PxVec3 pos(Transform->position.x, Transform->position.y, Transform->position.z);
			DirectX::XMVECTOR dxQuat = DirectX::XMQuaternionRotationRollPitchYaw(
				Transform->GetRotationEuler().x,
				Transform->GetRotationEuler().y,
				Transform->GetRotationEuler().z
			);
			physx::PxQuat quatRot;
			XMStoreFloat4(reinterpret_cast<DirectX::XMFLOAT4*>(&quatRot), dxQuat);
			physx::PxTransform pxTransform(pos, quatRot);

			if (Collider->isDynamic) {

				if (Collider->pRigidbodyStatic) {
					delete static_cast<ActorEntityInfo*>(Collider->pRigidbodyStatic->userData);
					Collider->pRigidbodyStatic->release();
					Collider->pRigidbodyStatic = nullptr;
				}

				if (Collider->pRigidbodyDynamic) {
					continue;
				}

				Collider->pRigidbodyDynamic = g_pPhysics->createRigidDynamic(pxTransform);
				Collider->pRigidbodyDynamic->userData = new ActorEntityInfo{ entity, context };
				g_pScene->addActor(*Collider->pRigidbodyDynamic);

				for (auto& col : Collider->colliders) {
					// 既存の scale を使う
					Vector3 scale = Transform->scale;

					// HeightMap タイプは PxHeightField を先に構築
					if (col.type == ColliderType::HEIGHT_MAP) {
						auto* terrain = context->component->GetComponent<TerrainComponent>(entity);
						BuildTerrainHeightField(col, terrain);
					}
					// Mesh タイプは ModelRendererComponent からトライアングルメッシュを構築
					if (col.type == ColliderType::MESH) {
						auto* mr = context->component->GetComponent<ModelRendererComponent>(entity);
						BuildMeshCollider(col, mr);
					}

					// マテリアル作成して保存
					physx::PxMaterial* material = g_pPhysics->createMaterial(col.staticFriction, col.dynamicFriction, col.restitution);
					col.pxMaterial = material;

					// CreatePxShape は actor に attach して PxShape* を返す実装を想定
					physx::PxShape* shape = CreatePxShape(Collider->pRigidbodyDynamic /*またはStatic*/, col, scale, *material);
					col.pxShape = shape;

					if (shape) {
						// レイヤー等のフィルタがあれば設定
						physx::PxFilterData fd;
						fd.word0 = col.collisionLayer;
						shape->setSimulationFilterData(fd);
						shape->setQueryFilterData(fd);
					}
				}
			} else {

				if (Collider->pRigidbodyDynamic) {
					delete static_cast<ActorEntityInfo*>(Collider->pRigidbodyDynamic->userData);
					Collider->pRigidbodyDynamic->release();
					Collider->pRigidbodyDynamic = nullptr;
				}

				if (Collider->pRigidbodyStatic) {
					continue;
				}

				Collider->pRigidbodyStatic = g_pPhysics->createRigidStatic(pxTransform);
				Collider->pRigidbodyStatic->userData = new ActorEntityInfo{ entity, context };
				g_pScene->addActor(*Collider->pRigidbodyStatic);

				for (auto& col : Collider->colliders) {
					// 既存の scale を使う
					Vector3 scale = Transform->scale;

					// HeightMap タイプは PxHeightField を先に構築
					if (col.type == ColliderType::HEIGHT_MAP) {
						auto* terrain = context->component->GetComponent<TerrainComponent>(entity);
						BuildTerrainHeightField(col, terrain);
					}
					// Mesh タイプは ModelRendererComponent からトライアングルメッシュを構築
					if (col.type == ColliderType::MESH) {
						auto* mr = context->component->GetComponent<ModelRendererComponent>(entity);
						BuildMeshCollider(col, mr);
					}

					// マテリアル作成して保存
					physx::PxMaterial* material = g_pPhysics->createMaterial(col.staticFriction, col.dynamicFriction, col.restitution);
					col.pxMaterial = material;

					// CreatePxShape は actor に attach して PxShape* を返す実装を想定
					physx::PxShape* shape = CreatePxShape(Collider->pRigidbodyStatic /*またはStatic*/, col, scale, *material);
					col.pxShape = shape;

					if (shape) {
						// レイヤー等のフィルタがあれば設定
						physx::PxFilterData fd;
						fd.word0 = col.collisionLayer;
						shape->setSimulationFilterData(fd);
						shape->setQueryFilterData(fd);
					}
				}
			}
		}


		for (Entity entity : colliderEntity) {
			auto Collider = context->component->GetComponent<ColliderComponent>(entity);

			auto Transform = context->component->GetComponent<TransformComponent>(entity);
			if (!Transform) continue;

			if (Collider->needsUpdate) {
				for (size_t i = 0; i < Collider->colliders.size(); ++i) {
					// HeightMap タイプは再構築前に PxHeightField を更新
					if (Collider->colliders[i].type == ColliderType::HEIGHT_MAP) {
						auto* terrain = context->component->GetComponent<TerrainComponent>(entity);
						BuildTerrainHeightField(Collider->colliders[i], terrain);
					}
					// Mesh タイプは ModelRendererComponent からトライアングルメッシュを再構築
					if (Collider->colliders[i].type == ColliderType::MESH) {
						auto* mr = context->component->GetComponent<ModelRendererComponent>(entity);
						BuildMeshCollider(Collider->colliders[i], mr);
					}
					UpdateColliderParam(Transform, Collider, entity, i);
				}
				Collider->needsUpdate = false;
			}

			physx::PxVec3 pos(Transform->position.x, Transform->position.y, Transform->position.z);
			DirectX::XMVECTOR dxQuat = Transform->rotationVector();
			DirectX::XMFLOAT4 qf;
			DirectX::XMStoreFloat4(&qf, dxQuat);
			physx::PxQuat quatRot(qf.x, qf.y, qf.z, qf.w);
			physx::PxTransform pxTransform(pos, quatRot);

			if (Collider->pRigidbodyDynamic) Collider->pRigidbodyDynamic->setGlobalPose(pxTransform);
			if (Collider->pRigidbodyStatic) Collider->pRigidbodyStatic->setGlobalPose(pxTransform);

			// ボーン名が指定されているコライダシェイプのローカルポーズをボーンのワールド変換から更新
			auto* ModelRenderer = context->component->GetComponent<ModelRendererComponent>(entity);
			if (ModelRenderer && ModelRenderer->model) {
				bool hasBoneShape = false;
				for (const auto& col : Collider->colliders) {
					if (!col.boneName.empty() && col.pxShape) { hasBoneShape = true; break; }
				}
				if (hasBoneShape) {
					g_pScene->lockWrite();
					for (auto& col : Collider->colliders) {
						if (col.boneName.empty() || !col.pxShape) continue;

						auto& boneIndexMap = ModelRenderer->model->m_BoneIndexMap;
						auto boneIt = boneIndexMap.find(col.boneName);
						if (boneIt == boneIndexMap.end()) continue;

						uint32_t boneIdx = boneIt->second;
						// WorldMatrix はモデル空間（ルート補正済み、スケール未適用）のボーン変換
						const aiMatrix4x4& boneModelMat = ModelRenderer->model->m_Bones[boneIdx].WorldMatrix;

						// ボーンのモデル空間変換を位置とクォータニオンに分解
						aiVector3D boneScale, bonePos;
						aiQuaternion boneRot;
						boneModelMat.Decompose(boneScale, boneRot, bonePos);

						// エンティティのスケールをボーン位置に乗算して描画と一致させる
						// （GPU はワールド行列に含まれるスケールを適用するが PhysX アクターは T×R のみ保持）
						const Vector3& entityScale = Transform->scale;
						physx::PxTransform boneWorldPose(
							physx::PxVec3(bonePos.x * entityScale.x, bonePos.y * entityScale.y, bonePos.z * entityScale.z),
							physx::PxQuat(boneRot.x, boneRot.y, boneRot.z, boneRot.w)
						);

						// ユーザー定義のローカルオフセット（ボーンローカル空間、同様にスケール適用）
						physx::PxVec3 userOffset(col.offset.x * entityScale.x, col.offset.y * entityScale.y, col.offset.z * entityScale.z);
						const float DegToRad = physx::PxPi / 180.0f;
						physx::PxQuat qx(col.rotationOffset.x * DegToRad, physx::PxVec3(1, 0, 0));
						physx::PxQuat qy(col.rotationOffset.y * DegToRad, physx::PxVec3(0, 1, 0));
						physx::PxQuat qz(col.rotationOffset.z * DegToRad, physx::PxVec3(0, 0, 1));
						physx::PxTransform localOffsetPose(userOffset, qz * qy * qx);

						// bone.WorldMatrix はモデル空間（アクターローカル空間）なので、
						// そのままシェイプローカルポーズとして使用する（エンティティ変換の逆行列は不要）
						physx::PxTransform shapeLocalPose = boneWorldPose.transform(localOffsetPose);

						col.pxShape->setLocalPose(shapeLocalPose);
					}
					g_pScene->unlockWrite();
				}
			}

			if (Collider->autoMass) {
				if (Collider->pRigidbodyDynamic) {
					Collider->Mass = Collider->pRigidbodyDynamic->getMass();
				}

			} else {
				if (Collider->pRigidbodyDynamic) {
					physx::PxRigidBodyExt::setMassAndUpdateInertia(*Collider->pRigidbodyDynamic, Collider->Mass);
				}
			}
		}
	}
}


void PhysicSystem::FixedUpdate(float deltaTime) {

	UpdateCollider();

	g_pScene->lockWrite();
	g_pScene->simulate(deltaTime);
	g_pScene->unlockWrite();

	g_pScene->lockRead();
	g_pScene->fetchResults(true);
	g_pScene->unlockRead();

	for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
		auto context = scene->GetSceneContext();
		const auto& colliderEntity = context->component->FindEntitiesWithComponent<ColliderComponent>();

		for (Entity entity : colliderEntity) {
			auto Collider = context->component->GetComponent<ColliderComponent>(entity);
			auto Transform = context->component->GetComponent<TransformComponent>(entity);
			if (!Transform) continue;

			physx::PxTransform TmpTransform;
			if (Collider->pRigidbodyDynamic) TmpTransform = Collider->pRigidbodyDynamic->getGlobalPose();
			if (Collider->pRigidbodyStatic) TmpTransform = Collider->pRigidbodyStatic->getGlobalPose();

			Transform->position = Vector3(TmpTransform.p.x, TmpTransform.p.y, TmpTransform.p.z);
			Transform->SetRotation(DirectX::XMFLOAT4(TmpTransform.q.x, TmpTransform.q.y, TmpTransform.q.z, TmpTransform.q.w));
		}
	}
}

void PhysicSystem::Draw(){
	if(!g_pScene) return;
	// PhysX の可視化デバッグは g_pScene->getRenderBuffer() などで取得可能
}

void PhysicSystem::DrawLayerEditor(){

	if(ImGui::TreeNode("Physics Layers")){

		// 追加ボタン
		if((int)m_layers.size() < kMaxPhysicsLayers){
			if(ImGui::Button("Add")){
				// スナップショットを取ってからレイヤーを追加しコマンドを積む
				auto before = TakeLayerSnapshot();
				PhysicsLayer layer;
				layer.name = "NewLayer";
				layer.bit = 0;
				m_layers.push_back(layer);
				RebuildLayerBits();
				RebuildCollisionMatrix();
				auto after = TakeLayerSnapshot();

				if(auto* mgr = ImGui::GetCommandManager()){
					mgr->Push(std::make_unique<PropertyChangeCommandWithSetter<LayerSnapshot>>(
						[this](const LayerSnapshot& snap){ RestoreLayerSnapshot(snap); },
						before, after, "物理レイヤーを追加"
					));
				}
			}
		} else{
			ImGui::TextDisabled("Max 32 layers reached");
		}

		ImGui::Spacing();

		// -------------------------
		// 一覧テーブル
		// -------------------------
		if(ImGui::BeginTable("LayerEditorTable", 3,
							 ImGuiTableFlags_Borders |
							 ImGuiTableFlags_RowBg |
							 ImGuiTableFlags_SizingFixedFit)){
			ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed, 50.0f);
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);

			ImGui::TableHeadersRow();

			int deleteIndex = -1;
			for(int i = 0; i < (int)m_layers.size(); ++i){
				ImGui::PushID(i);
				ImGui::TableNextRow();

				// Index
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("%d", i);

				// Name
				ImGui::TableSetColumnIndex(1);

				// UndoInputText でレイヤー名変更をUndo対応
				ImGui::UndoInputText("##name", &m_layers[i].name, 64);

				// Bit
				ImGui::TableSetColumnIndex(2);

				if(ImGui::SmallButton("Delete")){
					deleteIndex = i;
				}

				ImGui::PopID();
			}

			ImGui::EndTable();

			// テーブル外で削除処理（スナップショット方式）
			if(deleteIndex >= 0){
				auto before = TakeLayerSnapshot();
				RemoveLayer(deleteIndex);
				auto after = TakeLayerSnapshot();

				if(auto* mgr = ImGui::GetCommandManager()){
					mgr->Push(std::make_unique<PropertyChangeCommandWithSetter<LayerSnapshot>>(
						[this](const LayerSnapshot& snap){ RestoreLayerSnapshot(snap); },
						before, after, "物理レイヤーを削除"
					));
				}
			}
		}

		ImGui::TreePop();
	}
}