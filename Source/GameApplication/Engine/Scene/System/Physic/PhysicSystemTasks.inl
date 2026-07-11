#pragma once

#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Scene/Component/ColliderComponent.h"
#include "Scene/Component/CustomScriptComponent.h"
#include "Scene/Component/transformComponent.h"
#include "Scene/Registry/componentRegistry.h"
#include "Scene/Registry/entityRegistry.h"

namespace PhysicSystemTaskDetail {

template<typename T>
inline void ReleaseRef(T*& resource){
	if(!resource) return;
	resource->release();
	resource = nullptr;
}

inline void ReleaseShapeRuntime(
	ColliderComponent* collider,
	size_t shapeIndex
){
	if(!collider || shapeIndex >= collider->colliders.size()) return;

	ColliderShape& shapeRuntime = collider->colliders[shapeIndex];
	physx::PxRigidActor* actor = collider->pRigidbodyDynamic
		? static_cast<physx::PxRigidActor*>(collider->pRigidbodyDynamic)
		: static_cast<physx::PxRigidActor*>(collider->pRigidbodyStatic);

	if(actor && shapeRuntime.pxShape){
		actor->detachShape(*shapeRuntime.pxShape);
	}
	shapeRuntime.pxShape = nullptr;

	ReleaseRef(shapeRuntime.pxMaterial);
	ReleaseRef(shapeRuntime.pxHeightField);
	ReleaseRef(shapeRuntime.pxTriangleMesh);
	ReleaseRef(shapeRuntime.pxConvexMesh);
}

} // namespace PhysicSystemTaskDetail

inline void PhysicSystem::ReleaseColliderRuntime(ColliderComponent* collider){
	if(!collider) return;

	// ActorEntityInfo ownership belongs exclusively to m_actorEntityInfos.
	// Never delete PxActor::userData directly: DetachActorEntityInfo clears
	// the non-owning alias and erases the corresponding unique_ptr exactly once.
	auto releaseActor = [this](auto*& actor){
		if(!actor) return;
		DetachActorEntityInfo(actor);
		actor->release();
		actor = nullptr;
	};

	releaseActor(collider->pRigidbodyDynamic);
	releaseActor(collider->pRigidbodyStatic);

	for(ColliderShape& shapeRuntime : collider->colliders){
		shapeRuntime.pxShape = nullptr;
		PhysicSystemTaskDetail::ReleaseRef(shapeRuntime.pxMaterial);
		PhysicSystemTaskDetail::ReleaseRef(shapeRuntime.pxHeightField);
		PhysicSystemTaskDetail::ReleaseRef(shapeRuntime.pxTriangleMesh);
		PhysicSystemTaskDetail::ReleaseRef(shapeRuntime.pxConvexMesh);
	}
}

inline void PhysicSystem::ReleaseColliderShapeRuntime(
	ColliderComponent* collider,
	size_t shapeIndex
){
	PhysicSystemTaskDetail::ReleaseShapeRuntime(collider, shapeIndex);
}

inline void PhysicSystem::RegisterTasks(SystemScheduleBuilder& builder){
	SystemAccess uploadAccess;
	uploadAccess
		.ReadComponent<TransformComponent>()
		.WriteComponent<ColliderComponent>()
		.WriteResource<PhysicsSceneResource>();

	builder.AddTask(
		"PhysicSystem.Scene.Upload",
		SystemTaskDomain::Fixed,
		SystemPhase::Late,
		0,
		std::move(uploadAccess),
		ThreadAffinity::MainThread,
		[this](const SystemTaskContext&){
			PhysicsUpload();
		}
	);

	SystemAccess beginAccess;
	beginAccess.WriteResource<PhysicsSceneResource>();
	builder.AddTask(
		"PhysicSystem.Simulation.Simulate",
		SystemTaskDomain::Fixed,
		SystemPhase::Late,
		10,
		std::move(beginAccess),
		ThreadAffinity::AnyWorker,
		[this](const SystemTaskContext& context){
			PhysicsBegin(context.deltaTime);
		}
	);

	SystemAccess fetchAccess;
	fetchAccess
		.WriteResource<PhysicsSceneResource>()
		.WriteResource<PhysicsEventResource>();
	builder.AddTask(
		"PhysicSystem.Simulation.Fetch",
		SystemTaskDomain::Fixed,
		SystemPhase::Late,
		20,
		std::move(fetchAccess),
		ThreadAffinity::AnyWorker,
		[this](const SystemTaskContext&){
			PhysicsFetch();
		}
	);

	SystemAccess downloadAccess;
	downloadAccess
		.ReadComponent<ColliderComponent>()
		.WriteComponent<TransformComponent>()
		.WriteResource<PhysicsSceneResource>();
	builder.AddTask(
		"PhysicSystem.Scene.Download",
		SystemTaskDomain::Fixed,
		SystemPhase::Late,
		30,
		std::move(downloadAccess),
		ThreadAffinity::MainThread,
		[this](const SystemTaskContext&){
			PhysicsDownload();
		}
	);

	SystemAccess dispatchAccess = SystemAccess::LegacyExclusive();
	dispatchAccess
		.ReadResource<PhysicsSceneResource>()
		.ReadResource<PhysicsEventResource>();
	builder.AddTask(
		"PhysicSystem.Collision.Dispatch",
		SystemTaskDomain::Fixed,
		SystemPhase::Late,
		40,
		std::move(dispatchAccess),
		ThreadAffinity::MainThread,
		[this](const SystemTaskContext&){
			CollisionEventDispatch();
		}
	);
}

inline void PhysicSystem::PhysicsUpload(){
	if(!g_pScene || !g_pPhysics) return;
	UpdateCollider();
}

inline void PhysicSystem::PhysicsBegin(float fixedDeltaTime){
	if(!g_pScene || fixedDeltaTime <= 0.0f) return;

	bool expected = false;
	if(!m_simulationInFlight.compare_exchange_strong(
		expected,
		true,
		std::memory_order_acq_rel)){
		return;
	}

	// PxScene::simulate is a write call. Keep it under the scene write lock
	// so checked builds with eREQUIRE_RW_LOCK validate the same contract.
	g_pScene->lockWrite();
	const bool submitted = g_pScene->simulate(fixedDeltaTime);
	g_pScene->unlockWrite();

	if(!submitted){
		m_simulationInFlight.store(false, std::memory_order_release);
		OutputDebugStringA("PhysicSystem::PhysicsBegin simulate failed\n");
	}
}

inline void PhysicSystem::PhysicsFetch(){
	if(!g_pScene || !m_simulationInFlight.load(std::memory_order_acquire)){
		return;
	}

	// PxScene::fetchResults fires callbacks and swaps simulation buffers; it is
	// a write call, not a read-only query. A read lock here violates the PhysX
	// scene-lock contract when eREQUIRE_RW_LOCK is enabled.
	physx::PxU32 errorState = 0;
	g_pScene->lockWrite();
	const bool fetched = g_pScene->fetchResults(true, &errorState);
	g_pScene->unlockWrite();

	if(!fetched){
		OutputDebugStringA("PhysicSystem::PhysicsFetch fetchResults failed\n");
		return;
	}

	m_simulationInFlight.store(false, std::memory_order_release);
	if(errorState != 0){
		OutputDebugStringA("PhysicSystem::PhysicsFetch reported a PhysX error\n");
	}
}

inline void PhysicSystem::PhysicsDownload(){
	if(!m_context || !m_context->sceneManager) return;

	for(auto& [name, scene] : m_context->sceneManager->GetActiveScenes()){
		(void)name;
		if(!scene) continue;

		SceneContext* context = scene->GetSceneContext();
		if(!context || !context->component) continue;

		const auto colliderEntities =
			context->component->FindEntitiesWithComponent<ColliderComponent>();

		for(Entity entity : colliderEntities){
			ColliderComponent* collider =
				context->component->GetComponent<ColliderComponent>(entity);
			TransformComponent* transform =
				context->component->GetComponent<TransformComponent>(entity);
			if(!collider || !transform) continue;

			physx::PxRigidActor* actor = nullptr;
			if(collider->pRigidbodyDynamic){
				actor = collider->pRigidbodyDynamic;
			} else if(collider->pRigidbodyStatic){
				actor = collider->pRigidbodyStatic;
			}
			if(!actor) continue;

			const physx::PxTransform physicsTransform = actor->getGlobalPose();
			transform->position = Vector3(
				physicsTransform.p.x,
				physicsTransform.p.y,
				physicsTransform.p.z
			);
			transform->SetRotation(DirectX::XMFLOAT4(
				physicsTransform.q.x,
				physicsTransform.q.y,
				physicsTransform.q.z,
				physicsTransform.q.w
			));
		}
	}
}

inline bool PhysicSystem::QueueScriptCollisionEvent(
	CustomScriptComponent* script,
	ScriptCollisionEventType eventType,
	const HitInfo& hit
){
	if(!script) return false;

	std::scoped_lock lock(m_collisionEventMutex);
	m_pendingCollisionEvents.push_back({script, eventType, hit});
	return true;
}

inline void PhysicSystem::CollisionEventDispatch(){
	std::vector<PendingScriptCollisionEvent> events;
	{
		std::scoped_lock lock(m_collisionEventMutex);
		events.swap(m_pendingCollisionEvents);
	}

	for(const PendingScriptCollisionEvent& event : events){
		CustomScriptComponent* script = event.script;
		if(!script || !script->IsInitialized()) continue;

		switch(event.eventType){
			case ScriptCollisionEventType::CollisionEnter:
				script->CollisionEnter(event.hit);
				break;
			case ScriptCollisionEventType::CollisionStay:
				script->CollisionStay(event.hit);
				break;
			case ScriptCollisionEventType::CollisionExit:
				script->CollisionExit(event.hit);
				break;
			case ScriptCollisionEventType::TriggerEnter:
				script->TriggerEnter(event.hit);
				break;
			case ScriptCollisionEventType::TriggerExit:
				script->TriggerExit(event.hit);
				break;
		}
	}
}
