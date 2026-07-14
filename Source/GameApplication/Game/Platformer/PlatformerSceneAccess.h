#pragma once

#include "Engine/Scene/scene.h"
#include "Engine/Scene/Reference/ComponentRef.h"
#include "Engine/Scene/Reference/EntityRef.h"
#include "Engine/Scene/Registry/componentRegistry.h"
#include "Engine/Scene/System/Physic/physicSystem.h"
#include "Engine/Scene/sceneManager.h"

#include <cmath>

// Platformer-only physics query adapter. Gameplay probes must see static course
// geometry even when the player and old authored stage shapes share a stale
// Default layer. Dynamic actors and triggers are rejected by shape/actor type,
// rather than by excluding an entire layer bit.
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
		if(!shortGameplayProbe) {
			return physics->RaycastWithMask(origin, direction, maxDistance, layerMask);
		}

		const bool downwardGroundProbe =
			direction.y < -0.9f &&
			std::abs(direction.x) < 0.1f &&
			std::abs(direction.z) < 0.1f;
		if(!downwardGroundProbe) {
			return RaycastStaticSurface(origin, direction, maxDistance);
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
			const RayHit candidate = RaycastStaticSurface(
				origin + offsets[index],
				direction,
				maxDistance);
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

	// Existing platformer callers use Raw() only to perform more gameplay rays.
	// Returning the adapter keeps those rays on the same actor/trigger-safe path
	// instead of falling back to layer-mask exclusion and hiding Default-layer
	// course geometry together with the player.
	PlatformerPhysicsProbe* Raw() { return this; }
	const PlatformerPhysicsProbe* Raw() const { return this; }

private:
	class StaticSurfaceFilter final : public physx::PxQueryFilterCallback {
	public:
		physx::PxQueryHitType::Enum preFilter(
			const physx::PxFilterData&,
			const physx::PxShape* shape,
			const physx::PxRigidActor* actor,
			physx::PxHitFlags&
		) override {
			if(!shape) return physx::PxQueryHitType::eNONE;
			if(shape->getFlags().isSet(physx::PxShapeFlag::eTRIGGER_SHAPE)) {
				return physx::PxQueryHitType::eNONE;
			}
			if(actor && actor->getType() == physx::PxActorType::eRIGID_DYNAMIC) {
				return physx::PxQueryHitType::eNONE;
			}
			return physx::PxQueryHitType::eBLOCK;
		}

		physx::PxQueryHitType::Enum postFilter(
			const physx::PxFilterData&,
			const physx::PxQueryHit&,
			const physx::PxShape*,
			const physx::PxRigidActor*
		) override {
			return physx::PxQueryHitType::eBLOCK;
		}
	};

	RayHit RaycastStaticSurface(
		const physx::PxVec3& origin,
		const physx::PxVec3& direction,
		physx::PxReal maxDistance
	) const {
		RayHit result{};
		physx::PxScene* scene = physics ? physics->GetScene() : nullptr;
		if(!scene || maxDistance <= 0.0f) return result;

		physx::PxVec3 normalized = direction;
		if(normalized.normalize() < 1e-6f) return result;

		physx::PxQueryFilterData filterData;
		filterData.data.word0 = 0;
		filterData.flags |= physx::PxQueryFlag::ePREFILTER |
			physx::PxQueryFlag::eDISABLE_HARDCODED_FILTER;

		StaticSurfaceFilter filter;
		physx::PxRaycastBuffer hitBuffer;
		const bool status = scene->raycast(
			origin,
			normalized,
			maxDistance,
			hitBuffer,
			physx::PxHitFlags(
				physx::PxHitFlag::eDEFAULT |
				physx::PxHitFlag::eMESH_BOTH_SIDES),
			filterData,
			&filter);

		if(status && hitBuffer.hasBlock) {
			const physx::PxRaycastHit& block = hitBuffer.block;
			result.hit = true;
			result.position = block.position;
			result.normal = block.normal;
			result.distance = block.distance;
			result.hitShape = block.shape;
			result.hitActor = block.actor;
		}
		return result;
	}

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
