#pragma once
#include "Interface/ISystem.h"
#include "BackEnds/PhysX/PxPhysicsAPI.h"
#include <mutex>
#include <vector>
#include <DirectXMath.h>
#include <d3d11.h>

struct SceneManagerContext;
struct ColliderShape;
class ColliderComponent;
class TransformComponent;
class Vector3;

enum RayLayer : physx::PxU32 {
    LAYER_DEFAULT = (1u << 0),
    LAYER_PLAYER = (1u << 1),
    LAYER_ENEMY = (1u << 2),
    LAYER_ENVIRON = (1u << 3),
    // 必要に応じて増やす
};

struct RayHit {
    bool hit;
    physx::PxVec3 position;
    physx::PxVec3 normal;
    physx::PxReal distance;
    physx::PxShape* hitShape;
    physx::PxRigidActor* hitActor;
};

class PhysicSystem: public ISystem {
public:
	PhysicSystem(SceneManagerContext* context): m_context(context){}
	~PhysicSystem(){}

	void Initialize() override;
	void Finalize() override;

	void Start() override;
	void Update(float deltaTime) override{}
	void FixedUpdate(float fidedDeltaTime) override;
	void Draw() override;
	void EditorUpdate(float deltaTime) override {}

	void Stop() override;

	physx::PxPhysics* GetPhysics(){
		return g_pPhysics;
	}
	const physx::PxRenderBuffer& GetRenderBuffer(){

		return g_pScene->getRenderBuffer();
	}

	RayHit RaycastWithMask(const physx::PxVec3& origin,
						   const physx::PxVec3& direction,
						   physx::PxReal maxDistance,
						   physx::PxU32 layerMask /* ここが “どのレイヤーを当たり対象にするか” */);

	float Gravity = -9.0f;

private:
	SceneManagerContext* m_context;

	// PhysX 内で利用するアロケーターやコールバック
	physx::PxDefaultAllocator g_defaultAllocator;
	physx::PxDefaultErrorCallback g_defaultErrorCallback;
	physx::PxFoundation* g_pFoundation = nullptr;
	physx::PxPhysics* g_pPhysics = nullptr;
	physx::PxDefaultCpuDispatcher* g_pDispatcher = nullptr;
	physx::PxScene* g_pScene = nullptr;
	physx::PxPvd* g_pPvd = nullptr;

	std::mutex mtx;
	bool UpdatingPhysics = false;

	physx::PxRigidDynamic* CreateDynamic(const physx::PxTransform& t, const physx::PxGeometry& geometry, physx::PxMaterial& material, physx::PxReal density = 10.0f);
	physx::PxRigidStatic* CreateStatic(const physx::PxTransform& t, const physx::PxGeometry& geometry, physx::PxMaterial& material);

	void UpdateCollider();

	void UpdateColliderParam(TransformComponent* transform, ColliderComponent* collider, size_t entity ,size_t index);
	physx::PxShape* CreatePxShape(physx::PxRigidActor* actor, const ColliderShape& col, const Vector3& scale, physx::PxMaterial& material);
};
