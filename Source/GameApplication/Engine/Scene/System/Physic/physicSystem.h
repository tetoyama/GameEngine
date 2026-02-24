#pragma once

#include "Interface/ISystem.h"
#include "BackEnds/PhysX/PxPhysicsAPI.h"

#include <mutex>
#include <vector>
#include <string>
#include <DirectXMath.h>
#include <d3d11.h>

struct SceneManagerContext;
struct ColliderShape;
class ColliderComponent;
class TransformComponent;
class Vector3;

//======================================================================
// レイキャスト結果情報
//======================================================================
struct RayHit
{
	bool                    hit;       // ヒットしたかどうか
	physx::PxVec3           position;  // 衝突位置
	physx::PxVec3           normal;    // 衝突面法線
	physx::PxReal           distance;  // レイ開始点からの距離
	physx::PxShape* hitShape;  // ヒットした Shape
	physx::PxRigidActor* hitActor;  // ヒットした Actor
};

//======================================================================
// 物理レイヤー情報
//======================================================================
struct PhysicsLayer
{
	std::string name;   // レイヤー名
	uint32_t    bit;    // 1 << index 形式のビット
};

constexpr uint32_t kMaxPhysicsLayers = 32;

//======================================================================
// PhysicSystem
// PhysX を用いた物理シミュレーション管理クラス
//======================================================================
class PhysicSystem: public ISystem
{
public:

	//------------------------------------------------------------------
	// コンストラクタ / デストラクタ
	//------------------------------------------------------------------
	PhysicSystem(SceneManagerContext* context)
		: m_context(context){}

	~PhysicSystem(){}

	//------------------------------------------------------------------
	// ISystem 実装
	//------------------------------------------------------------------
	void Initialize() override;
	void Finalize() override;

	void Start() override;
	void FixedUpdate(float fixedDeltaTime) override;
	void Draw() override;

	void Stop() override;

	const char* GetSystemName() const override{
		return "PhysicSystem";
	}

	YAML::Node encode() override;
	bool decode(const YAML::Node& node) override;

	void SystemSetting() override;

	//------------------------------------------------------------------
	// PhysX 取得系
	//------------------------------------------------------------------
	physx::PxPhysics* GetPhysics(){
		return g_pPhysics;
	}

	const physx::PxRenderBuffer& GetRenderBuffer(){
		return g_pScene->getRenderBuffer();
	}

	//------------------------------------------------------------------
	// レイヤー衝突判定
	//------------------------------------------------------------------
	bool GetCollisionEnabled(uint32_t layerA, uint32_t layerB) const;

	//------------------------------------------------------------------
	// マスク付きレイキャスト
	// layerMask: 当たり判定対象とするレイヤー集合
	//------------------------------------------------------------------
	RayHit RaycastWithMask(const physx::PxVec3& origin,
						   const physx::PxVec3& direction,
						   physx::PxReal maxDistance,
						   physx::PxU32 layerMask);

	//------------------------------------------------------------------
	// レイヤー情報取得
	//------------------------------------------------------------------
	const std::vector<PhysicsLayer>& GetLayers() const{
		return m_layers;
	}

	uint32_t GetLayerBit(const std::string& name) const;

	//------------------------------------------------------------------
	// 重力設定
	//------------------------------------------------------------------
	float Gravity = -9.0f;

	int FindLayerIndex(uint32_t bit) const;

	void RebuildLayerBits();
	void RebuildCollisionMatrix();
	void RemoveLayer(int removeIndex);
	void AddLayer(const std::string& name);

private:

	//------------------------------------------------------------------
	// コンテキスト
	//------------------------------------------------------------------
	SceneManagerContext* m_context = nullptr;

	//------------------------------------------------------------------
	// レイヤー管理
	//------------------------------------------------------------------
	std::vector<PhysicsLayer> m_layers;

	// レイヤー間の衝突可否マトリクス
	bool m_collisionMatrix[kMaxPhysicsLayers][kMaxPhysicsLayers]{};

	//------------------------------------------------------------------
	// PhysX 基本オブジェクト
	//------------------------------------------------------------------
	physx::PxDefaultAllocator      g_defaultAllocator;
	physx::PxDefaultErrorCallback  g_defaultErrorCallback;

	physx::PxFoundation* g_pFoundation = nullptr;
	physx::PxPhysics* g_pPhysics = nullptr;
	physx::PxDefaultCpuDispatcher* g_pDispatcher = nullptr;
	physx::PxScene* g_pScene = nullptr;
	physx::PxPvd* g_pPvd = nullptr;

	//------------------------------------------------------------------
	// 状態管理
	//------------------------------------------------------------------
	std::mutex mtx;
	bool       UpdatingPhysics = false;

private:

	//------------------------------------------------------------------
	// Actor 作成補助
	//------------------------------------------------------------------
	physx::PxRigidDynamic* CreateDynamic(
		const physx::PxTransform& t,
		const physx::PxGeometry& geometry,
		physx::PxMaterial& material,
		physx::PxReal density = 10.0f);

	physx::PxRigidStatic* CreateStatic(
		const physx::PxTransform& t,
		const physx::PxGeometry& geometry,
		physx::PxMaterial& material);

	//------------------------------------------------------------------
	// Collider 更新
	//------------------------------------------------------------------
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

	//------------------------------------------------------------------
	// レイヤー編集 UI
	//------------------------------------------------------------------
	void DrawLayerEditor();
};