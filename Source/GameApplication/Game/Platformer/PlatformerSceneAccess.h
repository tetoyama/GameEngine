#pragma once

#include "Engine/Scene/scene.h"
#include "Engine/Scene/Reference/ComponentRef.h"
#include "Engine/Scene/Reference/EntityRef.h"
#include "Engine/Scene/Registry/componentRegistry.h"
#include "Engine/Scene/System/Physic/physicSystem.h"
#include "Engine/Scene/sceneManager.h"

#include <cmath>

// Platformer-only physics query adapter. Gameplay uses short probes for the
// capsule foot and walls, while the camera uses a much longer obstruction ray.
// Keeping the extra robustness here avoids changing the engine-wide Raycast API.
class PlatformerPhysicsProbe {
public:
	void Bind(PhysicSystem* value) { physics = value; }

	RayHit RaycastWithMask(
		const physx::PxVec3& origin,
		const physx::PxVec3& direction,
		physx::PxReal maxDistance,
		physx::PxU32 layerMask
	) const {
		if(!physics) return {};

		// Older Platformer scenes serialized SelfLayerBit as 10 (Player |
		// Environment). For the controller's short foot/wall probes this made the
		// query ignore the stage itself once Environment layers were authored.
		// A self-layer field is singular, so use its least-significant bit for
		// short gameplay probes. Camera obstruction rays keep their full mask.
		physx::PxU32 effectiveMask = layerMask;
		const bool shortGameplayProbe = maxDistance <= 0.75f;
		if(shortGameplayProbe && effectiveMask != 0) {
			effectiveMask &= (~effectiveMask + 1u);
		}

		const bool downwardGroundProbe =
			shortGameplayProbe &&
			direction.y < -0.9f &&
			std::abs(direction.x) < 0.1f &&
			std::abs(direction.z) < 0.1f;
		if(!downwardGroundProbe) {
			return physics->RaycastWithMask(origin, direction, maxDistance, effectiveMask);
		}

		// Keep the probe footprint well inside the 0.25 m capsule radius. The wider
		// nine-point disc allowed one trailing corner ray to keep reporting ground
		// after the capsule centre had already left a ledge, effectively preventing
		// falling. A centre plus four-point cross is sufficient for triangle seams.
		constexpr float probeRadius = 0.15f;
		const physx::PxVec3 offsets[] = {
			physx::PxVec3(0.0f, 0.0f, 0.0f),
			physx::PxVec3(probeRadius, 0.0f, 0.0f),
			physx::PxVec3(-probeRadius, 0.0f, 0.0f),
			physx::PxVec3(0.0f, 0.0f, probeRadius),
			physx::PxVec3(0.0f, 0.0f, -probeRadius)
		};

		RayHit best{};
		bool centreSupported = false;
		int peripheralSupportCount = 0;
		float peripheralReferenceDistance = 0.0f;

		for(int index = 0; index < 5; ++index) {
			const RayHit candidate = physics->RaycastWithMask(
				origin + offsets[index],
				direction,
				maxDistance,
				effectiveMask);
			if(!candidate.hit || candidate.normal.y <= 0.35f) continue;

			if(index == 0) {
				centreSupported = true;
			} else if(peripheralSupportCount == 0) {
				peripheralReferenceDistance = candidate.distance;
				peripheralSupportCount = 1;
			} else if(std::abs(candidate.distance - peripheralReferenceDistance) <= 0.08f) {
				++peripheralSupportCount;
			} else if(candidate.distance < peripheralReferenceDistance) {
				peripheralReferenceDistance = candidate.distance;
				peripheralSupportCount = 1;
			}

			const bool clearlyCloser = !best.hit ||
				candidate.distance + 0.025f < best.distance;
			const bool sameHeightMoreWalkable = best.hit &&
				std::abs(candidate.distance - best.distance) <= 0.025f &&
				candidate.normal.y > best.normal.y + 0.001f;
			if(clearlyCloser || sameHeightMoreWalkable) best = candidate;
		}

		// A centre hit is authoritative. If the centre lies on a triangle seam, two
		// surrounding hits at the same height are enough. One isolated edge hit is
		// deliberately rejected so the player detaches and gravity starts normally.
		if(!centreSupported && peripheralSupportCount < 2) return {};
		return best;
	}

	PhysicSystem* Raw() const { return physics; }

private:
	PhysicSystem* physics = nullptr;
};

// Game-facing access boundary. Platformer scripts use this helper instead of
// scattering SceneContext registry/service traversal throughout gameplay code.
class PlatformerSceneAccess {
public:
	template<typename T>
	static ComponentRef<T> Get(EntityRef entity) {
		if(!entity.IsValid()) return {};
		return ComponentRef<T>(entity);
	}

	template<typename T>
	static ComponentRef<T> Get(Entity entity, SceneContext* context) {
		if(!context || !entity) return {};
		return ComponentRef<T>(entity, context);
	}

	template<typename T>
	static ComponentRef<T> FindFirst(SceneContext* context) {
		if(!context || !context->component) return {};
		const auto entities = context->component->FindEntitiesWithComponent<T>();
		if(entities.empty()) return {};
		return ComponentRef<T>(entities.front(), context);
	}

	template<typename T>
	static EntityRef FindFirstEntity(SceneContext* context) {
		if(!context || !context->component) return {};
		const auto entities = context->component->FindEntitiesWithComponent<T>();
		if(entities.empty()) return {};
		return EntityRef(entities.front(), context);
	}

	static PlatformerPhysicsProbe* Physics(SceneContext* context) {
		static thread_local PlatformerPhysicsProbe probe;
		PhysicSystem* physics = context && context->system
			? context->system->GetSystem<PhysicSystem>()
			: nullptr;
		probe.Bind(physics);
		return physics ? &probe : nullptr;
	}

	static AudioContext* Audio(SceneContext* context) {
		return context && context->manager ? context->manager->audio : nullptr;
	}

	static ResourceService* Resources(SceneContext* context) {
		return context && context->manager ? context->manager->resource : nullptr;
	}

	static bool IsAlive(const EntityRef& entity) {
		return entity.IsValid();
	}
};
