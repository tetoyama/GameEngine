#include "physicSystem.h"
# include <mutex>
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

// Dynamic Rigidbodyの作成
physx::PxRigidDynamic* PhysicSystem::CreateDynamic(const physx::PxTransform& t,
							  const physx::PxGeometry& geometry, physx::PxMaterial& material, physx::PxReal density) {

	physx::PxRigidDynamic* rigid_dynamic = PxCreateDynamic(*g_pPhysics, t, geometry, material, density);
	g_pScene->addActor(*rigid_dynamic);

	return rigid_dynamic;
}

physx::PxRigidStatic* PhysicSystem::CreateStatic(const physx::PxTransform& t, const physx::PxGeometry& geometry, physx::PxMaterial& material) {

	physx::PxRigidStatic* rigid_static = PxCreateStatic(*g_pPhysics, t, geometry, material);
	g_pScene->addActor(*rigid_static);

	return rigid_static;
}

void PhysicSystem::Initialize(){
	UpdatingPhysics = false;
	// Foundationのインスタンス化
	g_pFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, g_defaultAllocator, g_defaultErrorCallback);
	if(g_pFoundation){
	    // PVDと接続する設定
	    g_pPvd = physx::PxCreatePvd(*g_pFoundation);
	    // PVD側のデフォルトポートは5425
	    physx::PxPvdTransport* transport = physx::PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
	    g_pPvd->connect(*transport, physx::PxPvdInstrumentationFlag::eALL);
	}

	// Physicsのインスタンス化
	g_pPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *g_pFoundation, physx::PxTolerancesScale(), true, g_pPvd);
	// 拡張機能用
	PxInitExtensions(*g_pPhysics, g_pPvd);
	// 処理に使うスレッドを指定する
	g_pDispatcher = physx::PxDefaultCpuDispatcherCreate(8);
	// 空間の設定
	physx::PxSceneDesc scene_desc(g_pPhysics->getTolerancesScale());
	scene_desc.gravity = physx::PxVec3(0, -9, 0);
	scene_desc.filterShader = physx::PxDefaultSimulationFilterShader;
	scene_desc.cpuDispatcher = g_pDispatcher;

	g_pScene = g_pPhysics->createScene(scene_desc);
	if (g_pScene) {
		g_pScene->setVisualizationParameter(physx::PxVisualizationParameter::eSCALE, 1.0f);
		g_pScene->setVisualizationParameter(physx::PxVisualizationParameter::eCOLLISION_SHAPES, 1.0f);
		g_pScene->setVisualizationParameter(physx::PxVisualizationParameter::eACTOR_AXES, 1.0f);
	}

	// 空間のインスタンス化
	{
	    // PVDの表示設定
	    physx::PxPvdSceneClient* pvd_client;
	    pvd_client = g_pScene->getScenePvdClient();
	    pvd_client->setScenePvdFlag(physx::PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
	    pvd_client->setScenePvdFlag(physx::PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
	    pvd_client->setScenePvdFlag(physx::PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
	}


	// ライン描画用シェーダーをロード
	LoadDebugShaders();
}

void PhysicSystem::LoadDebugShaders() {
	auto device = m_context->manager->graphics->GetDevice();

	// VS
	ID3DBlob* vsBlob = nullptr;
	D3DCompileFromFile(L"Source/Shader/DebugLineVS.hlsl", nullptr, nullptr, "main", "vs_5_0", 0, 0, &vsBlob, nullptr);
	device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_debugVS);

	// PS
	ID3DBlob* psBlob = nullptr;
	D3DCompileFromFile(L"Source/Shader/DebugLinePS.hlsl", nullptr, nullptr, "main", "ps_5_0", 0, 0, &psBlob, nullptr);
	device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_debugPS);

	if (vsBlob) vsBlob->Release();
	if (psBlob) psBlob->Release();
}

void PhysicSystem::Finalize(){

	if (m_debugVS) { m_debugVS->Release(); m_debugVS = nullptr; }
	if (m_debugPS) { m_debugPS->Release(); m_debugPS = nullptr; }

	std::lock_guard<std::mutex> lock(mtx); // mtxを使ってロックする
	WaitPhysicsUpdate();

	const auto& colliderEntity = m_context->component->FindEntitiesWithComponent<ColliderComponent>();

	for (Entity entity : colliderEntity) {


		auto Collider = m_context->component->GetComponent<ColliderComponent>(entity);
		if (Collider->pRigidbodyStatic) {
			Collider->pRigidbodyStatic->release();
			Collider->pRigidbodyStatic = nullptr;
		}
		if (Collider->pRigidbodyDynamic) {
			Collider->pRigidbodyDynamic->release();
			Collider->pRigidbodyDynamic = nullptr;

		}
	}


	PxCloseExtensions();
	g_pScene->release();
	g_pDispatcher->release();
	g_pPhysics->release();
	if(g_pPvd){
		g_pPvd->disconnect();
		physx::PxPvdTransport* transport = g_pPvd->getTransport();
		g_pPvd->release();
		transport->release();
	}
	g_pFoundation->release();
}

void PhysicSystem::Start(){

	// コンポーネントを持つエンティティの検索
	const auto& colliderEntity = m_context->component->FindEntitiesWithComponent<ColliderComponent>();
	if (colliderEntity.empty()) {
		return;
	} else {
		for (Entity entity : colliderEntity) {
			auto Collider = m_context->component->GetComponent<ColliderComponent>(entity);
			auto Transform = m_context->component->GetComponent<TransformComponent>(entity);

			if (Transform) {

				physx::PxVec3 pos(Transform->position.x, Transform->position.y, Transform->position.z);
				physx::PxVec3 rot(Transform->rotation.x, Transform->rotation.y, Transform->rotation.z);
				physx::PxVec3 scl(Transform->scale.x, Transform->scale.y, Transform->scale.z);

				DirectX::XMVECTOR dxQuat = DirectX::XMQuaternionRotationRollPitchYaw(
					Transform->rotation.x,
					Transform->rotation.y,
					Transform->rotation.z
				);

				// PxQuatに変換
				physx::PxQuat quatRot;
				XMStoreFloat4(reinterpret_cast<DirectX::XMFLOAT4*>(&quatRot), dxQuat);
				//quatRot.x = rot.x;
				//quatRot.y = rot.y;
				//quatRot.z = rot.z;
				//quatRot.w = 0.0f;

				physx::PxTransform pxTransform(pos, quatRot);
				
				physx::PxMaterial* const kMaterial = GetPhysics()->createMaterial(0.2f, 0.2f, 0.2f);

				if (Collider->isDynamic) {
					Collider->pRigidbodyDynamic = CreateDynamic(pxTransform, physx::PxBoxGeometry(scl.x, scl.y, scl.z), *kMaterial);
				} else {
					Collider->pRigidbodyStatic = CreateStatic(pxTransform, physx::PxBoxGeometry(scl.x, scl.y, scl.z), *kMaterial);
				}
			}
		}
	}
}
physx::PxVec3 QuatToEuler(const physx::PxQuat& q) {
	float ysqr = q.y * q.y;

	// ロール (X軸回転)
	float t0 = +2.0f * (q.w * q.x + q.y * q.z);
	float t1 = +1.0f - 2.0f * (q.x * q.x + ysqr);
	float roll = std::atan2(t0, t1);

	// ピッチ (Y軸回転)
	float t2 = +2.0f * (q.w * q.y - q.z * q.x);
	t2 = t2 > 1.0f ? 1.0f : t2;
	t2 = t2 < -1.0f ? -1.0f : t2;
	float pitch = std::asin(t2);

	// ヨー (Z軸回転)
	float t3 = +2.0f * (q.w * q.z + q.x * q.y);
	float t4 = +1.0f - 2.0f * (ysqr + q.z * q.z);
	float yaw = std::atan2(t3, t4);

	return physx::PxVec3(roll, pitch, yaw);
}

void PhysicSystem::FixedUpdate(float deltaTime){
	std::lock_guard<std::mutex> lock(mtx);

	// コンポーネントを持つエンティティの検索
	const auto& colliderEntity = m_context->component->FindEntitiesWithComponent<ColliderComponent>();
	if(colliderEntity.empty()){
		return;
	} else{
		for(Entity entity : colliderEntity){
			auto Collider = m_context->component->GetComponent<ColliderComponent>(entity);
			auto Transform = m_context->component->GetComponent<TransformComponent>(entity);

			if(Transform){
				// DirectX の Transform から PhysX 用に変換
				physx::PxVec3 pos(Transform->position.x, Transform->position.y, Transform->position.z);

				DirectX::XMVECTOR dxQuat = Transform->rotationVector(); // クォータニオンを取得
				DirectX::XMFLOAT4 qf;
				DirectX::XMStoreFloat4(&qf, dxQuat);

				physx::PxQuat quatRot(qf.x, qf.y, qf.z, qf.w);
				physx::PxTransform pxTransform(pos, quatRot);

				if(Collider->pRigidbodyDynamic){
					Collider->pRigidbodyDynamic->setGlobalPose(pxTransform);
				}

				if(Collider->pRigidbodyStatic){
					Collider->pRigidbodyStatic->setGlobalPose(pxTransform);
				}
			}
		}
	}

	g_pScene->lockWrite();
	g_pScene->lockRead();
	g_pScene->simulate(deltaTime);
	g_pScene->fetchResults(true);
	g_pScene->unlockWrite();
	g_pScene->unlockRead();

	if(colliderEntity.empty()){
		return;
	} else{
		for(Entity entity : colliderEntity){
			auto Collider = m_context->component->GetComponent<ColliderComponent>(entity);
			auto Transform = m_context->component->GetComponent<TransformComponent>(entity);

			if(Transform){
				physx::PxTransform TmpTransform;
				if(Collider->pRigidbodyDynamic){
					TmpTransform = Collider->pRigidbodyDynamic->getGlobalPose();
				}

				if(Collider->pRigidbodyStatic){
					TmpTransform = Collider->pRigidbodyStatic->getGlobalPose();
				}

				physx::PxVec3 position = TmpTransform.p;
				physx::PxQuat rotation = TmpTransform.q;

				// PhysX の PxQuat → DirectX::XMFLOAT4
				DirectX::XMFLOAT4 qf(rotation.x, rotation.y, rotation.z, rotation.w);

				// TransformComponent に反映
				Transform->position = Vector3(position.x, position.y, position.z);
				Transform->rotation = qf; // クォータニオンを格納
			}
		}
	}
}

void PhysicSystem::Stop() {
	const auto& colliderEntity = m_context->component->FindEntitiesWithComponent<ColliderComponent>();

	for (Entity entity : colliderEntity) {


		auto Collider = m_context->component->GetComponent<ColliderComponent>(entity);
		if (Collider->pRigidbodyStatic) {
			Collider->pRigidbodyStatic->release();
			Collider->pRigidbodyStatic = nullptr;
		}
		if (Collider->pRigidbodyDynamic) {
			Collider->pRigidbodyDynamic->release();
			Collider->pRigidbodyDynamic = nullptr;

		}
	}
}

struct DebugVertex {
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT4 color;
};

void PhysicSystem::Draw() {
	if (!g_pScene) return;

	const physx::PxRenderBuffer& rb = g_pScene->getRenderBuffer();

	std::vector<DebugVertex> vertices;
	for (physx::PxU32 i = 0; i < rb.getNbLines(); i++) {
		const physx::PxDebugLine& line = rb.getLines()[i];

		// 色を 0-1 の範囲に変換（ARGB -> RGBA）
		DirectX::XMFLOAT4 convertColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

		DebugVertex v0;
		v0.pos = DirectX::XMFLOAT3(line.pos0.x, line.pos0.y, line.pos0.z);
		v0.color = convertColor;

		DebugVertex v1;
		v1.pos = DirectX::XMFLOAT3(line.pos1.x, line.pos1.y, line.pos1.z);
		v1.color = convertColor;

		vertices.push_back(v0);
		vertices.push_back(v1);
	}


	if (vertices.empty()) return;

	// デバイスとコンテキスト取得
	ID3D11Device* device = m_context->manager->graphics->GetDevice();
	ID3D11DeviceContext* context = m_context->manager->graphics->GetDeviceContext();

	// 頂点バッファ作成
	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(DebugVertex) * vertices.size();
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	D3D11_SUBRESOURCE_DATA initData{};
	initData.pSysMem = vertices.data();

	ID3D11Buffer* pVertexBuffer = nullptr;
	device->CreateBuffer(&bd, &initData, &pVertexBuffer);

	UINT stride = sizeof(DebugVertex);
	UINT offset = 0;
	context->IASetVertexBuffers(0, 1, &pVertexBuffer, &stride, &offset);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	// クラスメンバのシェーダーを使用
	context->VSSetShader(m_debugVS, nullptr, 0);
	context->PSSetShader(m_debugPS, nullptr, 0);

	context->Draw(vertices.size(), 0);

	pVertexBuffer->Release();
}

