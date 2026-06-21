#pragma once

#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Scene/Component/ColliderComponent.h"
#include "Scene/Component/CustomScriptComponent.h"
#include "Scene/Component/transformComponent.h"
#include "Scene/Registry/componentRegistry.h"

inline void PhysicSystem::RegisterTasks(SystemScheduleBuilder& builder){
	SystemAccess uploadAccess;
	uploadAccess
		.ReadComponent<TransformComponent>()
		.WriteComponent<ColliderComponent>()
		.WriteResource<PhysicsSceneResource>();

	builder.AddTask(
		"PhysicSystem.PhysicsUpload",
		SystemTaskDomain::Fixed,
		SystemPhase::Default,
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
		"PhysicSystem.PhysicsBegin",
		SystemTaskDomain::Fixed,
		SystemPhase::Default,
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
		"PhysicSystem.PhysicsFetch",
		SystemTaskDomain::Fixed,
		SystemPhase::Default,
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
		"PhysicSystem.PhysicsDownload",
		SystemTaskDomain::Fixed,
		SystemPhase::Default,
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
		"PhysicSystem.CollisionEventDispatch",
		SystemTaskDomain::Fixed,
		SystemPhase::Default,
		40,
		std::move(dispatchAccess),
		ThreadAffinity::MainThread,
		[this](const SystemTaskContext&){
			CollisionEventDispatch();
		}
	);

	builder.AddTask(
		"PhysicSystem.Draw",
		SystemTaskDomain::Render,
		[this](const SystemTaskContext&){
			Draw();
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

	g_pScene->lockWrite();
	g_pScene->simulate(fixedDeltaTime);
	g_pScene->unlockWrite();
}

inline void PhysicSystem::PhysicsFetch(){
	if(!g_pScene || !m_simulationInFlight.load(std::memory_order_acquire)){
		return;
	}

	ScriptCollisionDispatchBridge::Install(
		this,
		[](void* owner,
		   CustomScriptComponent* script,
		   ScriptCollisionEventType eventType,
		   const HitInfo& hit) -> bool {
			return static_cast<PhysicSystem*>(owner)->QueueScriptCollisionEvent(
				script,
				eventType,
				hit
			);
		}
	);

	g_pScene->lockRead();
	g_pScene->fetchResults(true);
	g_pScene->unlockRead();

	ScriptCollisionDispatchBridge::Remove(this);
	m_simulationInFlight.store(false, std::memory_order_release);
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
