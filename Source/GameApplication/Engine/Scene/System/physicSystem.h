#pragma once
#include "Interface/ISystem.h"
#include "Source/GameApplication/BackEnds/PhysX/PxPhysicsAPI.h"
#include <mutex>
#include <vector>
#include <DirectXMath.h>
#include <d3d11.h>

struct SceneContext;
struct ColliderShape;
class ColliderComponent;
class Vector3;

class PhysicSystem: public ISystem {
public:
	PhysicSystem(SceneContext* context): m_context(context){}
	~PhysicSystem(){}

	void Initialize() override;
	void Finalize() override;

	void Start() override;
	void Update(float deltaTime) override{}
	void FixedUpdate(float fidedDeltaTime) override;
	void Draw() override;
	void EditorUpdate(float deltaTime) override{}
	void Stop() override;

	physx::PxPhysics* GetPhysics(){
		return g_pPhysics;
	}
	const physx::PxRenderBuffer& GetRenderBuffer(){
		return g_pScene->getRenderBuffer();
	}
private:
	SceneContext* m_context;

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

	void UpdateColliderParam(ColliderComponent* collider, size_t entity ,size_t index);
	physx::PxShape* CreatePxShape(physx::PxRigidActor* actor, const ColliderShape& col, const Vector3& scale, physx::PxMaterial& material);
};
