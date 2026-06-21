#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "Interface/IComponent.h"
#include "Backends/myVector3.h"

namespace physx {
class PxShape;
class PxMaterial;
class PxHeightField;
class PxTriangleMesh;
class PxConvexMesh;
class PxRigidDynamic;
class PxRigidStatic;
}

enum class ColliderType {
	Box,
	Sphere,
	Capsule,
	Mesh,
	HeightMap
};

struct ColliderShape {
	ColliderType type = ColliderType::Box;
	Vector3 size = Vector3(1.0f, 1.0f, 1.0f);
	Vector3 offset = Vector3(0.0f, 0.0f, 0.0f);
	Vector3 rotationOffset = Vector3(0.0f, 0.0f, 0.0f);
	float radius = 0.5f;
	float height = 1.0f;
	float staticFriction = 0.6f;
	float dynamicFriction = 0.6f;
	float restitution = 0.1f;
	uint32_t collisionLayer = 0;
	bool lockRotX = false;
	bool lockRotY = false;
	bool lockRotZ = false;
	bool isTrigger = false;
	std::string boneName;

	// PhysicSystemが所有するRuntime資源への非所有エイリアス。
	physx::PxShape* pxShape = nullptr;
	physx::PxMaterial* pxMaterial = nullptr;
	physx::PxHeightField* pxHeightField = nullptr;
	physx::PxTriangleMesh* pxTriangleMesh = nullptr;
	physx::PxConvexMesh* pxConvexMesh = nullptr;
};

class ColliderComponent: public IComponent {
public:
	using ReleaseEntityCallback = void(*)(void* owner, ColliderComponent* collider);
	using ReleaseShapeCallback = void(*)(
		void* owner,
		ColliderComponent* collider,
		size_t shapeIndex
	);

	~ColliderComponent() override;

	// PhysicSystemが所有するActorへの非所有エイリアス。
	physx::PxRigidDynamic* pRigidbodyDynamic = nullptr;
	physx::PxRigidStatic* pRigidbodyStatic = nullptr;

	bool needsUpdate = true;
	bool isDynamic = false;
	bool autoMass = true;
	float Mass = 0.0f;
	std::vector<ColliderShape> colliders;

	void SetRuntimeReleaseCallbacks(
		void* owner,
		ReleaseEntityCallback releaseEntity,
		ReleaseShapeCallback releaseShape
	) noexcept {
		m_runtimeOwner = owner;
		m_releaseEntity = releaseEntity;
		m_releaseShape = releaseShape;
	}

	void ClearRuntimeReleaseCallbacks(void* owner = nullptr) noexcept {
		if(owner && owner != m_runtimeOwner) return;
		m_runtimeOwner = nullptr;
		m_releaseEntity = nullptr;
		m_releaseShape = nullptr;
	}

	void ReleaseRuntimeResources() noexcept {
		if(m_runtimeOwner && m_releaseEntity){
			m_releaseEntity(m_runtimeOwner, this);
		}
	}

	void ReleaseShapeRuntime(size_t shapeIndex) noexcept {
		if(m_runtimeOwner && m_releaseShape){
			m_releaseShape(m_runtimeOwner, this, shapeIndex);
		}
	}

	YAML::Node encode() override;
	bool decode(SceneContext* context, const YAML::Node& node) override;
	void inspector(SceneContext* context) override;
	void OnBeforeRemove(SceneContext* context) override;

private:
	void* m_runtimeOwner = nullptr;
	ReleaseEntityCallback m_releaseEntity = nullptr;
	ReleaseShapeCallback m_releaseShape = nullptr;
};

#include "Operations/ColliderComponentOperations.h"
