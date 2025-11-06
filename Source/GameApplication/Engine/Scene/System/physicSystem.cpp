#include "physicSystem.h"
#include <mutex>
#include "Source/GameApplication/BackEnds/PhysX/PxPhysicsAPI.h"
#include <GameApplication/Engine/Scene/scene.h>
#include <GameApplication/Engine/Scene/sceneManager.h>
#include <GameApplication/Engine/Scene/Component/ColliderComponent.h>
#include <GameApplication/Engine/Scene/Component/transformComponent.h>
#include <d3dcompiler.h>

#pragma comment(lib, "PhysX_64.lib")
#pragma comment(lib, "PhysXCommon_64.lib")
#pragma comment(lib, "PhysXCooking_64.lib")
#pragma comment(lib, "PhysXExtensions_static_64.lib")
#pragma comment(lib, "PhysXFoundation_64.lib")
#pragma comment(lib, "PhysXPvdSDK_static_64.lib")
#pragma comment(lib, "PhysXTask_static_64.lib")
#pragma comment(lib, "SceneQuery_static_64.lib")
#pragma comment(lib, "SimulationController_static_64.lib")

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



// =============================================================
// Actor に attach する形で PxShape を作成
// =============================================================
physx::PxShape* PhysicSystem::CreatePxShape(
	physx::PxRigidActor* actor,
	const ColliderShape& col,
	const Vector3& scale,
	physx::PxMaterial& material){
	if(!actor) return nullptr;

	physx::PxShape* shape = nullptr;

	switch(col.type){
		case ColliderType::Box:
			shape = physx::PxRigidActorExt::createExclusiveShape(
				*actor,
				physx::PxBoxGeometry(
					col.size.x * 0.5f * scale.x,
					col.size.y * 0.5f * scale.y,
					col.size.z * 0.5f * scale.z),
				material
			);
			break;

		case ColliderType::Sphere:
		{
			float r = col.radius * (std::max)({scale.x, scale.y, scale.z});
			shape = physx::PxRigidActorExt::createExclusiveShape(
				*actor,
				physx::PxSphereGeometry(r),
				material
			);
			break;
		}

		case ColliderType::Capsule:
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

		case ColliderType::Mesh:
			// Mesh は Cooking 必須なので別処理
			break;
	}

	// =============================================================
	// ★ 回転オフセットの反映（オイラー角 → PxQuat）
	// =============================================================
	if(shape){
		physx::PxVec3 offset(
			col.offset.x * scale.x,
			col.offset.y * scale.y,
			col.offset.z * scale.z
		);

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
void PhysicSystem::UpdateColliderParam(ColliderComponent* collider, size_t entity, size_t index){
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
	TransformComponent* transform = m_context->component->GetComponent<TransformComponent>((Entity)entity);
	Vector3 scale = transform ? transform->scale : Vector3{1.0f, 1.0f, 1.0f};

	// 5) 新しい shape 作成
	physx::PxShape* newShape = CreatePxShape(actor, col, scale, *material);
	col.pxShape = newShape;

	// 6) レイヤー設定
	if(newShape){
		physx::PxFilterData fd;
		fd.word0 = col.collisionLayer;
		newShape->setSimulationFilterData(fd);
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
	UpdatingPhysics = false;
	g_pFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, g_defaultAllocator, g_defaultErrorCallback);

	if(g_pFoundation){
		g_pPvd = physx::PxCreatePvd(*g_pFoundation);
		physx::PxPvdTransport* transport = physx::PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
		g_pPvd->connect(*transport, physx::PxPvdInstrumentationFlag::eALL);
	}

	g_pPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *g_pFoundation, physx::PxTolerancesScale(), true, g_pPvd);
	PxInitExtensions(*g_pPhysics, g_pPvd);

	g_pDispatcher = physx::PxDefaultCpuDispatcherCreate(8);
	physx::PxSceneDesc scene_desc(g_pPhysics->getTolerancesScale());
	scene_desc.gravity = physx::PxVec3(0, -9, 0);
	scene_desc.filterShader = physx::PxDefaultSimulationFilterShader;
	scene_desc.cpuDispatcher = g_pDispatcher;

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

	const auto& colliderEntity = m_context->component->FindEntitiesWithComponent<ColliderComponent>();
	if (colliderEntity.empty()) return;
	for (Entity entity : colliderEntity) {
		auto Collider = m_context->component->GetComponent<ColliderComponent>(entity);

		Collider->needsUpdate = true;
	}

	UpdateCollider();
}
void PhysicSystem::Finalize(){
	OutputDebugStringA("PhysicSystem::Finalize\n");
	const auto& colliderEntity = m_context->component->FindEntitiesWithComponent<ColliderComponent>();
	for(Entity entity : colliderEntity){
		auto Collider = m_context->component->GetComponent<ColliderComponent>(entity);
		for(auto& col : Collider->colliders){
			if(col.pxMaterial){
				OutputDebugStringA(("Finalize Release Material: " + std::to_string((uintptr_t)col.pxMaterial) + "\n").c_str());
				col.pxMaterial->release();
				col.pxMaterial = nullptr;
			}
			if(col.pxShape){
				OutputDebugStringA(("Finalize Shape Pointer Clear: " + std::to_string((uintptr_t)col.pxShape) + "\n").c_str());
				col.pxShape = nullptr; // Actor release に任せる
			}
		}
		if(Collider->pRigidbodyStatic){
			OutputDebugStringA(("Finalize Release Static Actor: " + std::to_string((uintptr_t)Collider->pRigidbodyStatic) + "\n").c_str());
			Collider->pRigidbodyStatic->release();
			Collider->pRigidbodyStatic = nullptr;
		}
		if(Collider->pRigidbodyDynamic){
			OutputDebugStringA(("Finalize Release Dynamic Actor: " + std::to_string((uintptr_t)Collider->pRigidbodyDynamic) + "\n").c_str());
			Collider->pRigidbodyDynamic->release();
			Collider->pRigidbodyDynamic = nullptr;
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
		g_pPvd->release();
		g_pPvd = nullptr;
		if(transport){
			OutputDebugStringA(("Finalize Release PVD Transport: " + std::to_string((uintptr_t)transport) + "\n").c_str());
			transport->release();
		}
	}

	if(g_pFoundation){
		OutputDebugStringA(("Finalize Release Foundation: " + std::to_string((uintptr_t)g_pFoundation) + "\n").c_str());
		g_pFoundation->release();
		g_pFoundation = nullptr;
	}
}

void PhysicSystem::Stop(){
	const auto& colliderEntity = m_context->component->FindEntitiesWithComponent<ColliderComponent>();
	for(Entity entity : colliderEntity){
		auto Collider = m_context->component->GetComponent<ColliderComponent>(entity);

		for(auto& col : Collider->colliders){
			if(col.pxMaterial){
				OutputDebugStringA(("Stop Release Material: " + std::to_string((uintptr_t)col.pxMaterial) + "\n").c_str());
				col.pxMaterial->release();
				col.pxMaterial = nullptr;
			}
			if(col.pxShape){
				OutputDebugStringA(("Stop Shape Pointer Clear: " + std::to_string((uintptr_t)col.pxShape) + "\n").c_str());
				col.pxShape = nullptr;
			}
		}

		if(Collider->pRigidbodyStatic){
			OutputDebugStringA(("Stop Release Static Actor: " + std::to_string((uintptr_t)Collider->pRigidbodyStatic) + "\n").c_str());
			Collider->pRigidbodyStatic->release();
			Collider->pRigidbodyStatic = nullptr;
		}
		if(Collider->pRigidbodyDynamic){
			OutputDebugStringA(("Stop Release Dynamic Actor: " + std::to_string((uintptr_t)Collider->pRigidbodyDynamic) + "\n").c_str());
			Collider->pRigidbodyDynamic->release();
			Collider->pRigidbodyDynamic = nullptr;
		}
	}
}

void PhysicSystem::Start(){

}

void PhysicSystem::UpdateCollider() {

	const auto& colliderEntity = m_context->component->FindEntitiesWithComponent<ColliderComponent>();
	if (colliderEntity.empty()) return;

	for (Entity entity : colliderEntity) {
		auto Collider = m_context->component->GetComponent<ColliderComponent>(entity);
		auto Transform = m_context->component->GetComponent<TransformComponent>(entity);
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
				Collider->pRigidbodyStatic->release();
				Collider->pRigidbodyStatic = nullptr;
			}

			if (Collider->pRigidbodyDynamic) {
				continue;
			}

			Collider->pRigidbodyDynamic = g_pPhysics->createRigidDynamic(pxTransform);
			g_pScene->addActor(*Collider->pRigidbodyDynamic);

			for (auto& col : Collider->colliders) {
				// 既存の scale を使う
				Vector3 scale = Transform->scale;

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
				}
			}
		} else {

			if (Collider->pRigidbodyDynamic) {
				Collider->pRigidbodyDynamic->release();
				Collider->pRigidbodyDynamic = nullptr;
			}

			if (Collider->pRigidbodyStatic) {
				continue;
			}

			Collider->pRigidbodyStatic = g_pPhysics->createRigidStatic(pxTransform);
			g_pScene->addActor(*Collider->pRigidbodyStatic);

			for (auto& col : Collider->colliders) {
				// 既存の scale を使う
				Vector3 scale = Transform->scale;

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
				}
			}
		}
	}


	for (Entity entity : colliderEntity) {
		auto Collider = m_context->component->GetComponent<ColliderComponent>(entity);

		auto Transform = m_context->component->GetComponent<TransformComponent>(entity);
		if (!Transform) continue;

		if (Collider->needsUpdate) {
			for (size_t i = 0; i < Collider->colliders.size(); ++i) {
				UpdateColliderParam(Collider, entity, i);
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
	}
}


void PhysicSystem::FixedUpdate(float deltaTime){

	const auto& colliderEntity = m_context->component->FindEntitiesWithComponent<ColliderComponent>();
	if(colliderEntity.empty()) return;

	UpdateCollider();

	g_pScene->lockWrite();
	g_pScene->lockRead();
	g_pScene->simulate(deltaTime);
	g_pScene->fetchResults(true);
	g_pScene->unlockWrite();
	g_pScene->unlockRead();

	for(Entity entity : colliderEntity){
		auto Collider = m_context->component->GetComponent<ColliderComponent>(entity);
		auto Transform = m_context->component->GetComponent<TransformComponent>(entity);
		if(!Transform) continue;

		physx::PxTransform TmpTransform;
		if(Collider->pRigidbodyDynamic) TmpTransform = Collider->pRigidbodyDynamic->getGlobalPose();
		if(Collider->pRigidbodyStatic) TmpTransform = Collider->pRigidbodyStatic->getGlobalPose();

		Transform->position = Vector3(TmpTransform.p.x, TmpTransform.p.y, TmpTransform.p.z);
		Transform->SetRotation(DirectX::XMFLOAT4(TmpTransform.q.x, TmpTransform.q.y, TmpTransform.q.z, TmpTransform.q.w));
	}
}



void PhysicSystem::Draw(){
	if(!g_pScene) return;
	// PhysX の可視化デバッグは g_pScene->getRenderBuffer() などで取得可能
}
