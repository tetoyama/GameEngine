// =======================================================================
// 
// physicSystem.h
// 
// =======================================================================
#pragma once

#include "Interface/ISystem.h"
#include "Backends/PhysX/PxPhysicsAPI.h"
#include "Scene/Entity/Entity.h"
#include "System/Physic/HitInfo.h"
#include "System/Physic/ScriptCollisionEvent.h"

#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>
#include <string>
#include <DirectXMath.h>
#include <d3d11.h>

struct SceneManagerContext;
struct SceneContext;
struct ColliderShape;
class ColliderComponent;
class CustomScriptComponent;
class ModelRendererComponent;
class TransformComponent;
class Vector3;
class PhysicsSimulationCallback;

//======================================================================
// アクターとエンティティを関連付けるユーザーデータ
//======================================================================
struct ActorEntityInfo {
	Entity        entity;
	SceneContext* context;
};

//======================================================================
// レイキャスト結果情報
//======================================================================
struct RayHit
{
	bool                    hit;
	physx::PxVec3           position;
	physx::PxVec3           normal;
	physx::PxReal           distance;
	physx::PxShape* hitShape;
	physx::PxRigidActor* hitActor;
};

//======================================================================
// 物理レイヤー情報
//======================================================================
struct PhysicsLayer
{
	std::string name;
	uint32_t    bit;
};

constexpr uint32_t kMaxPhysicsLayers = 32;

//======================================================================
// PhysicSystem
// PhysX を用いた物理シミュレーション管理クラス
//======================================================================
class PhysicSystem: public ISystem
{
public:
	explicit PhysicSystem(SceneManagerContext* context);

	~PhysicSystem() override;

	void Initialize() override;
	void Finalize() override;

	void Start() override;
	void FixedUpdate(float fixedDeltaTime) override;
	void Draw() override;
	void Stop() override;

	void RegisterTasks(SystemScheduleBuilder& builder) override;

	const char* GetSystemName() const override{
		return "PhysicSystem";
	}

	YAML::Node encode() override;
	bool decode(const YAML::Node& node) override;

	void SystemSetting() override;
	bool HasSystemSetting() const override { return true; }

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
						   physx::PxU32 layerMask);

	const std::vector<PhysicsLayer>& GetLayers() const{
		return m_layers;
	}

	uint32_t GetLayerBit(const std::string& name) const;

	void ReleaseColliderRuntime(ColliderComponent* collider);
	void ReleaseColliderShapeRuntime(ColliderComponent* collider, size_t shapeIndex);

	float Gravity = -9.0f;

	int FindLayerIndex(uint32_t bit) const;

	void RebuildLayerBits();
	void RebuildCollisionMatrix();
	void RemoveLayer(int removeIndex);
	void AddLayer(const std::string& name);

	struct LayerSnapshot {
		std::vector<PhysicsLayer> layers;
		bool matrix[kMaxPhysicsLayers][kMaxPhysicsLayers]{};
	};

	LayerSnapshot TakeLayerSnapshot() const {
		LayerSnapshot snap;
		snap.layers = m_layers;
		for (int y = 0; y < (int)kMaxPhysicsLayers; ++y)
			for (int x = 0; x < (int)kMaxPhysicsLayers; ++x)
				snap.matrix[y][x] = m_collisionMatrix[y][x];
		return snap;
	}

	void RestoreLayerSnapshot(const LayerSnapshot& snap) {
		m_layers = snap.layers;
		for (int y = 0; y < (int)kMaxPhysicsLayers; ++y)
			for (int x = 0; x < (int)kMaxPhysicsLayers; ++x)
				m_collisionMatrix[y][x] = snap.matrix[y][x];
		RebuildLayerBits();
	}

private:
	friend class PhysicsSimulationCallback;

	struct PhysicsSceneResource {};
	struct PhysicsEventResource {};

	struct PendingScriptCollisionEvent {
		CustomScriptComponent* script = nullptr;
		ScriptCollisionEventType eventType =
			ScriptCollisionEventType::CollisionEnter;
		HitInfo hit;
	};

	void PhysicsUpload();
	void PhysicsBegin(float fixedDeltaTime);
	void PhysicsFetch();
	void PhysicsDownload();
	void CollisionEventDispatch();

	bool QueueScriptCollisionEvent(
		CustomScriptComponent* script,
		ScriptCollisionEventType eventType,
		const HitInfo& hit
	);

	SceneManagerContext* m_context = nullptr;
	PhysicSystem* m_filterOwner = nullptr;

	std::vector<PhysicsLayer> m_layers;
	bool m_collisionMatrix[kMaxPhysicsLayers][kMaxPhysicsLayers]{};

	physx::PxDefaultAllocator      g_defaultAllocator;
	physx::PxDefaultErrorCallback  g_defaultErrorCallback;

	physx::PxFoundation* g_pFoundation = nullptr;
	physx::PxPhysics* g_pPhysics = nullptr;
	physx::PxDefaultCpuDispatcher* g_pDispatcher = nullptr;
	physx::PxScene* g_pScene = nullptr;
	physx::PxPvd* g_pPvd = nullptr;

	std::mutex mtx;
	bool       UpdatingPhysics = false;
	std::atomic<bool> m_simulationInFlight{false};
	std::unique_ptr<PhysicsSimulationCallback> m_simCallback;

	std::mutex m_collisionEventMutex;
	std::vector<PendingScriptCollisionEvent> m_pendingCollisionEvents;

	physx::PxRigidDynamic* CreateDynamic(
		const physx::PxTransform& t,
		const physx::PxGeometry& geometry,
		physx::PxMaterial& material,
		physx::PxReal density = 10.0f);

	physx::PxRigidStatic* CreateStatic(
		const physx::PxTransform& t,
		const physx::PxGeometry& geometry,
		physx::PxMaterial& material);

	void UpdateCollider();

	void UpdateColliderParam(
		TransformComponent* transform,
		ColliderComponent* collider,
		size_t entity,
		size_t index);

	physx::PxShape* CreatePxShape(
		physx::PxRigidActor* actor,
		const ColliderShape& col,
		const Vector3& scale,
		physx::PxMaterial& material);

	void BuildMeshCollider(ColliderShape& col, ModelRendererComponent* modelRenderer);

	void DrawLayerEditor();
};

#include "PhysicSystemTasks.inl"
