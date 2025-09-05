//#include "physicSystem.h"
//# include <mutex>
//#include "Source/GameApplication/BackEnds/PhysX/PxPhysicsAPI.h"
//
//#pragma comment(lib, "PhysX_64.lib")
//#pragma comment(lib, "PhysXCommon_64.lib")
//#pragma comment(lib, "PhysXCooking_64.lib")
//#pragma comment(lib, "PhysXExtensions_static_64.lib")
//#pragma comment(lib, "PhysXFoundation_64.lib")
//#pragma comment(lib, "PhysXPvdSDK_static_64.lib")
//#pragma comment(lib, "PhysXTask_static_64.lib")
//#pragma comment(lib, "SceneQuery_static_64.lib")
//#pragma comment(lib, "SimulationController_static_64.lib")
//
//void PhysicSystem::Initialize(){
//	UpdatingPhysics = false;
//	// Foundationのインスタンス化
//	g_pFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, g_defaultAllocator, g_defaultErrorCallback);
//	if(g_pFoundation){
//	    // PVDと接続する設定
//	    g_pPvd = physx::PxCreatePvd(*g_pFoundation);
//	    // PVD側のデフォルトポートは5425
//	    physx::PxPvdTransport* transport = physx::PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
//	    g_pPvd->connect(*transport, physx::PxPvdInstrumentationFlag::eALL);
//	}
//
//	// Physicsのインスタンス化
//	g_pPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *g_pFoundation, physx::PxTolerancesScale(), true, g_pPvd);
//	// 拡張機能用
//	PxInitExtensions(*g_pPhysics, g_pPvd);
//	// 処理に使うスレッドを指定する
//	g_pDispatcher = physx::PxDefaultCpuDispatcherCreate(8);
//	// 空間の設定
//	physx::PxSceneDesc scene_desc(g_pPhysics->getTolerancesScale());
//	scene_desc.gravity = physx::PxVec3(0, -9, 0);
//	scene_desc.filterShader = physx::PxDefaultSimulationFilterShader;
//	scene_desc.cpuDispatcher = g_pDispatcher;
//
//	g_pScene = g_pPhysics->createScene(scene_desc);
//
//	// 空間のインスタンス化
//	{
//	    // PVDの表示設定
//	    physx::PxPvdSceneClient* pvd_client;
//	    pvd_client = g_pScene->getScenePvdClient();
//	    pvd_client->setScenePvdFlag(physx::PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
//	    pvd_client->setScenePvdFlag(physx::PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
//	    pvd_client->setScenePvdFlag(physx::PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
//	}
//}
//
//void PhysicSystem::Finalize(){
//	std::lock_guard<std::mutex> lock(mtx); // mtxを使ってロックする
//	WaitPhysicsUpdate();
//
//	PxCloseExtensions();
//	g_pScene->release();
//	g_pDispatcher->release();
//	g_pPhysics->release();
//	if(g_pPvd){
//		g_pPvd->disconnect();
//		physx::PxPvdTransport* transport = g_pPvd->getTransport();
//		g_pPvd->release();
//		transport->release();
//	}
//	g_pFoundation->release();
//}
//
//void PhysicSystem::Start(){
//
//}
//
//void PhysicSystem::Update(float deltaTime){
//	std::lock_guard<std::mutex> lock(mtx); // mtxを使ってロックする
//
//	g_pScene->lockWrite();
//	g_pScene->lockRead();
//	g_pScene->simulate(deltaTime);
//	g_pScene->fetchResults(true);
//	g_pScene->unlockWrite();
//	g_pScene->unlockRead();
//}
