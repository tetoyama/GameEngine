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

struct PhysicsLayer {
	std::string name;
	uint32_t bit;   // 1 << index
};

constexpr uint32_t kMaxPhysicsLayers = 32;

class PhysicSystem: public ISystem {
public:
	PhysicSystem(SceneManagerContext* context): m_context(context){}
	~PhysicSystem(){}

	void Initialize() override;
	void Finalize() override;

	void Start() override;
	void FixedUpdate(float fidedDeltaTime) override;
	void Draw() override;

	void Stop() override;

	YAML::Node encode() override;
	bool decode(const YAML::Node& node) override;

	void SystemSetting() override;

	physx::PxPhysics* GetPhysics(){
		return g_pPhysics;
	}
	const physx::PxRenderBuffer& GetRenderBuffer(){

		return g_pScene->getRenderBuffer();
	}
	bool GetCollisionEnabled(uint32_t layerA, uint32_t layerB) const;


	RayHit RaycastWithMask(const physx::PxVec3& origin,
						   const physx::PxVec3& direction,
						   physx::PxReal maxDistance,
						   physx::PxU32 layerMask /* ここが “どのレイヤーを当たり対象にするか” */);

	const std::vector<PhysicsLayer>& GetLayers() const { return m_layers; }
	uint32_t GetLayerBit(const std::string& name) const;

	float Gravity = -9.0f;
	int FindLayerIndex(uint32_t bit) const {
		for (int i = 0; i < m_layers.size(); ++i) {
			if (m_layers[i].bit == bit)
				return i;
		}
		return -1;
	}
private:
	SceneManagerContext* m_context;
	std::vector<PhysicsLayer> m_layers;
	bool m_collisionMatrix[kMaxPhysicsLayers][kMaxPhysicsLayers]{};

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
