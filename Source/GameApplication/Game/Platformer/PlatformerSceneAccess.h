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

		// The authored player capsule has a 0.25 m world-space radius. The previous
		// 0.16 m cross still left a wide unsupported annulus, so landing near a ledge
		// or across a triangle seam could miss all five rays while PhysX visibly
		// supported part of the capsule. Sample a nine-point disc at 0.22 m instead.
		constexpr float probeRadius = 0.22f;
		constexpr float diagonal = probeRadius * 0.70710678f;
		const physx::PxVec3 offsets[] = {
			physx::PxVec3(0.0f, 0.0f, 0.0f),
			physx::PxVec3(probeRadius, 0.0f, 0.0f),
			physx::PxVec3(-probeRadius, 0.0f, 0.0f),
			physx::PxVec3(0.0f, 0.0f, probeRadius),
			physx::PxVec3(0.0f, 0.0f, -probeRadius),
			physx::PxVec3(diagonal, 0.0f, diagonal),
			physx::PxVec3(-diagonal, 0.0f, diagonal),
			physx::PxVec3(diagonal, 0.0f, -diagonal),
			physx::PxVec3(-diagonal, 0.0f, -diagonal)
		};

		// Give descending motion a small one-fixed-step tolerance without changing
		// the controller's authored slope limit or making wall/camera rays longer.
		constexpr physx::PxReal landingTolerance = 0.10f;
		const physx::PxReal queryDistance = maxDistance + landingTolerance;

		RayHit best{};
		for(const physx::PxVec3& offset : offsets) {
			const RayHit candidate = physics->RaycastWithMask(
				origin + offset,
				direction,
				queryDistance,
				effectiveMask);
			if(!candidate.hit) continue;

			// Prefer a clearly nearer support. For hits at approximately the same
			// height, prefer the more upward-facing triangle so seams and bevels do not
			// override a valid walkable top surface.
			const bool clearlyCloser = !best.hit ||
				candidate.distance + 0.035f < best.distance;
			const bool sameHeightMoreWalkable = best.hit &&
				std::abs(candidate.distance - best.distance) <= 0.035f &&
				candidate.normal.y > best.normal.y + 0.001f;
			if(clearlyCloser || sameHeightMoreWalkable) best = candidate;
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