#pragma once

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

	// Physics側が生成・解放するRuntime資源への非所有エイリアス。
	// Component側から直接releaseしてはならない。
	physx::PxShape* pxShape = nullptr;
	physx::PxMaterial* pxMaterial = nullptr;
	physx::PxHeightField* pxHeightField = nullptr;
	physx::PxTriangleMesh* pxTriangleMesh = nullptr;
	physx::PxConvexMesh* pxConvexMesh = nullptr;
};

class ColliderComponent: public IComponent {
public:
	~ColliderComponent() override;

	// Physics側が所有するActorへの非所有エイリアス。
	// 互換APIがActorへアクセスする間だけ保持し、解放はRelease Bridgeへ委譲する。
	physx::PxRigidDynamic* pRigidbodyDynamic = nullptr;
	physx::PxRigidStatic* pRigidbodyStatic = nullptr;

	bool needsUpdate = true;
	bool isDynamic = false;
	bool autoMass = true;
	float Mass = 0.0f;
	std::vector<ColliderShape> colliders;

	YAML::Node encode() override;
	bool decode(SceneContext* context, const YAML::Node& node) override;
	void inspector(SceneContext* context) override;
};

#include "Operations/ColliderComponentOperations.h"
