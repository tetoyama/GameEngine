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

		const bool shortGameplayProbe = maxDistance <= 0.75f;
		physx::PxU32 effectiveMask = layerMask;
		if(shortGameplayProbe) {
			// SelfLayerBit was serialized as 10 in older Platformer scenes even though
			// the player shape may use either the canonical Player bit or the legacy
			// Default bit. Never preserve the Environment bit from that stale value.
			const physx::PxU32 playerLayer = physics->GetLayerBit("Player");
			effectiveMask = playerLayer != 0
				? playerLayer
				: (layerMask != 0 ? layerMask & (~layerMask + 1u) : 0u);
		}

		const bool downwardGroundProbe =
			shortGameplayProbe &&
			direction.y < -0.9f &&
			std::abs(direction.x) < 0.1f &&
			std::abs(direction.z) < 0.1f;
		if(!downwardGroundProbe) {
			return physics->RaycastWithMask(origin, direction, maxDistance, effectiveMask);
		}

		// Keep the probe footprint well inside the 0.25 m capsule radius. A centre
		// plus four-point cross preserves support across mesh seams without allowing
		// one trailing corner to keep the player grounded after leaving a ledge.
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
			physx::PxU32 queryMask = effectiveMask;
			RayHit candidate{};

			// The foot ray starts inside the player capsule and can also pass through
			// CameraZone, Checkpoint and Coin trigger volumes. Those shapes participate
			// in scene queries even though they are not simulation surfaces. Retry after
			// excluding the actual layer of any dynamic actor or trigger hit so only a
			// static, non-trigger support surface can become ground.
			for(int attempt = 0; attempt < 6; ++attempt) {
				candidate = physics->RaycastWithMask(
					origin + offsets[index],
					direction,
					maxDistance,
					queryMask);
				if(!candidate.hit) break;

				const bool dynamicActor = candidate.hitActor &&
					candidate.hitActor->getType() == physx::PxActorType::eRIGID_DYNAMIC;
				const bool triggerShape = candidate.hitShape &&
					candidate.hitShape->getFlags().isSet(physx::PxShapeFlag::eTRIGGER_SHAPE);
				if(!dynamicActor && !triggerShape) break;

				const physx::PxU32 hitLayer = candidate.hitShape
					? candidate.hitShape->getQueryFilterData().word0
					: 0u;
				if(hitLayer == 0u || (queryMask & hitLayer) != 0u) {
					candidate = {};
					break;
				}
				queryMask |= hitLayer;
			}

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
