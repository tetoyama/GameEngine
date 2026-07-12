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

		// A single centre ray can fall exactly on a triangle seam or just beyond a
		// platform edge while the capsule is still visibly supported. Sample the
		// centre plus four points inside the 0.25 m capsule radius and prefer the
		// most upward-facing result, then the nearest result at the same slope.
		constexpr float probeRadius = 0.16f;
		const physx::PxVec3 offsets[] = {
			physx::PxVec3(0.0f, 0.0f, 0.0f),
			physx::PxVec3(probeRadius, 0.0f, 0.0f),
			physx::PxVec3(-probeRadius, 0.0f, 0.0f),
			physx::PxVec3(0.0f, 0.0f, probeRadius),
			physx::PxVec3(0.0f, 0.0f, -probeRadius)
		};

		RayHit best{};
		for(const physx::PxVec3& offset : offsets) {
			const RayHit candidate = physics->RaycastWithMask(
				origin + offset,
				direction,
				maxDistance,
				effectiveMask);
			if(!candidate.hit) continue;

			const bool moreWalkable = !best.hit || candidate.normal.y > best.normal.y + 0.001f;
			const bool sameSlopeAndCloser = best.hit &&
				std::abs(candidate.normal.y - best.normal.y) <= 0.001f &&
				candidate.distance < best.distance;
			if(moreWalkable || sameSlopeAndCloser) best = candidate;
		}
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
