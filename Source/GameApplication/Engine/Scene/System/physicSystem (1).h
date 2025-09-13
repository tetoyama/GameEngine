#pragma once
#include "Interface/ISystem.h"
#include "Source/GameApplication/BackEnds/PhysX/PxPhysicsAPI.h"
#include <mutex>
#include <vector>
#include <DirectXMath.h>
#include <d3d11.h>
struct SceneContext;



class PhysicSystem: public ISystem {
public:
	PhysicSystem(SceneContext* context): m_context(context){}
	~PhysicSystem(){}

	void Initialize() override;
	void Finalize()override;

	void Start() override;
	void Update(float deltaTime) override{}
	void FixedUpdate(float fidedDeltaTime) override;
	void Draw() override;
	void EditorUpdate(float deltaTime) override{}
	void Stop() override;


	void ReleasePhysics(physx::PxRigidStatic** pRigidStatic){
		std::lock_guard<std::mutex> lock(mtx); // mtxを使ってロックする
		WaitPhysicsUpdate();

		if(*pRigidStatic){
			(*pRigidStatic)->release();
			*pRigidStatic = nullptr;
		}
		UpdatingPhysics = false;

	}

	physx::PxPhysics* GetPhysics(){
		WaitPhysicsUpdate();

		std::lock_guard<std::mutex> lock(mtx); // mtxを使ってロックする
		UpdatingPhysics = false;

		return g_pPhysics;
	}

	DirectX::XMMATRIX PxMatToMatrix(const physx::PxMat44& px_mat44){
		return DirectX::XMMATRIX(
			px_mat44.column0.x, px_mat44.column0.y, px_mat44.column0.z, px_mat44.column0.w,
			px_mat44.column1.x, px_mat44.column1.y, px_mat44.column1.z, px_mat44.column1.w,
			px_mat44.column2.x, px_mat44.column2.y, px_mat44.column2.z, px_mat44.column2.w,
			px_mat44.column3.x, px_mat44.column3.y, px_mat44.column3.z, px_mat44.column3.w
		);
	}


	physx::PxTriangleMesh* ToPxTriangleMesh(physx::PxPhysics* physics, const std::vector<physx::PxU32>& vertices, const std::vector<physx::PxVec3>& triangles){
		std::lock_guard<std::mutex> lock(mtx); // mtxを使ってロックする

		physx::PxTolerancesScale tolerances_scale;
		physx::PxCookingParams cooking_params(tolerances_scale);
		cooking_params.convexMeshCookingType = physx::PxConvexMeshCookingType::Enum::eQUICKHULL;
		cooking_params.gaussMapLimit = static_cast<physx::PxU32>(vertices.size());

		physx::PxTriangleMeshDesc px_mesh_desc;
		px_mesh_desc.setToDefault();
		px_mesh_desc.triangles.count = static_cast<physx::PxU32>(vertices.size() / 3);
		px_mesh_desc.triangles.stride = sizeof(physx::PxU32) * 3;
		px_mesh_desc.triangles.data = &vertices[0];
		px_mesh_desc.points.count = static_cast<physx::PxU32>(triangles.size());
		px_mesh_desc.points.stride = sizeof(physx::PxVec3);
		px_mesh_desc.points.data = &triangles[0];
		px_mesh_desc.flags = physx::PxMeshFlag::e16_BIT_INDICES;


		assert(px_mesh_desc.isValid());

		//{
		//    physx::PxSDFDesc sdf_desc;
		//    if (spacing > 0.f) {
		//        sdf_desc.spacing = static_cast<physx::PxReal>(spacing);
		//        sdf_desc.subgridSize = static_cast<physx::PxU32>(subgrid_size);
		//        sdf_desc.bitsPerSubgridPixel = bits_per_sdf_subgrid_pixel;
		//        sdf_desc.numThreadsForSdfConstruction = 16;
		//        px_mesh_desc.sdfDesc = &sdf_desc;
		//    }
		//}

		physx::PxTriangleMesh* triangle_mesh = nullptr;
		physx::PxDefaultMemoryOutputStream write_buffer;

		bool status = PxCookTriangleMesh(cooking_params, px_mesh_desc, write_buffer);
		if(!status)
			return NULL;

		physx::PxDefaultMemoryInputData read_buffer(write_buffer.getData(), write_buffer.getSize());
		triangle_mesh = physics->createTriangleMesh(read_buffer);
		return triangle_mesh;
	}

private:
	SceneContext* m_context;

	// PhysX内で利用するアロケーター
	physx::PxDefaultAllocator g_defaultAllocator;
	// エラー時用のコールバックでエラー内容が入ってる
	physx::PxDefaultErrorCallback g_defaultErrorCallback;
	// 上位レベルのSDK(PxPhysicsなど)をインスタンス化する際に必要
	physx::PxFoundation* g_pFoundation = nullptr;
	// 実際に物理演算を行う
	physx::PxPhysics* g_pPhysics = nullptr;
	// シミュレーションをどう処理するかの設定でマルチスレッドの設定もできる
	physx::PxDefaultCpuDispatcher* g_pDispatcher = nullptr;
	// シミュレーションする空間の単位でActorの追加などもここで行う
	physx::PxScene* g_pScene = nullptr;
	// PVDと通信する際に必要
	physx::PxPvd* g_pPvd = nullptr;

	std::mutex mtx;

	bool UpdatingPhysics = false;

	// ライン描画用シェーダー
	ID3D11VertexShader* m_debugVS = nullptr;
	ID3D11PixelShader* m_debugPS = nullptr;

	void LoadDebugShaders();

	void WaitPhysicsUpdate(){
		while(UpdatingPhysics){

		}
		UpdatingPhysics = true;
	}

	physx::PxRigidDynamic* CreateDynamic(const physx::PxTransform& t, const physx::PxGeometry& geometry, physx::PxMaterial& material, physx::PxReal density = 10.0f);
	physx::PxRigidStatic* CreateStatic(const physx::PxTransform& t, const physx::PxGeometry& geometry, physx::PxMaterial& material);
};
