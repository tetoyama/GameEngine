from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8-sig")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding="utf-8")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


def replace_function(text: str, signature: str, replacement: str) -> str:
    start = text.find(signature)
    if start < 0:
        raise RuntimeError(f"function not found: {signature}")

    brace = text.find("{", start)
    if brace < 0:
        raise RuntimeError(f"opening brace not found: {signature}")

    depth = 0
    end = -1
    for index in range(brace, len(text)):
        char = text[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                end = index + 1
                break

    if end < 0:
        raise RuntimeError(f"closing brace not found: {signature}")

    return text[:start] + replacement.rstrip() + text[end:]


# -----------------------------------------------------------------------
# PhysicSystem header: Store and lifecycle helpers
# -----------------------------------------------------------------------
header_path = "Source/GameApplication/Engine/Scene/System/Physic/physicSystem.h"
header = read(header_path)
header = replace_once(
    header,
    '#include "Scene/Entity/Entity.h"\n#include "System/Physic/ScriptCollisionDispatchBridge.h"',
    '#include "Scene/Entity/Entity.h"\n#include "System/Physic/PhysicsRuntimeStore.h"\n#include "System/Physic/ScriptCollisionDispatchBridge.h"',
    "physics runtime include",
)
header = replace_once(
    header,
    "\tvoid CollisionEventDispatch();\n\n\tbool QueueScriptCollisionEvent(",
    "\tvoid CollisionEventDispatch();\n\n"
    "\tvoid PreparePhysicsRuntime();\n"
    "\tvoid CapturePhysicsRuntime();\n"
    "\tvoid ReleaseAllPhysicsRuntime();\n"
    "\tvoid DrainSimulationAndEvents();\n\n"
    "\tbool QueueScriptCollisionEvent(",
    "physics runtime method declarations",
)
header = replace_once(
    header,
    "\tPhysicsSimulationCallback* m_simCallback = nullptr;\n\n"
    "\tstd::mutex m_collisionEventMutex;",
    "\tPhysicsSimulationCallback* m_simCallback = nullptr;\n\n"
    "\tPhysicsRuntimeStore m_runtimeStore;\n\n"
    "\tstd::mutex m_collisionEventMutex;",
    "physics runtime store member",
)
write(header_path, header)


# -----------------------------------------------------------------------
# Task pipeline: prepare/capture ownership and lifecycle helpers
# -----------------------------------------------------------------------
tasks_path = "Source/GameApplication/Engine/Scene/System/Physic/PhysicSystemTasks.inl"
tasks = read(tasks_path)
tasks = replace_function(
    tasks,
    "inline void PhysicSystem::PhysicsUpload()",
    r'''inline void PhysicSystem::PhysicsUpload(){
	if(!g_pScene || !g_pPhysics) return;

	PreparePhysicsRuntime();
	UpdateCollider();
	CapturePhysicsRuntime();
}''',
)

tasks = replace_once(
    tasks,
    "\t\t\tphysx::PxRigidActor* actor = nullptr;\n"
    "\t\t\tif(collider->pRigidbodyDynamic){\n"
    "\t\t\t\tactor = collider->pRigidbodyDynamic;\n"
    "\t\t\t} else if(collider->pRigidbodyStatic){\n"
    "\t\t\t\tactor = collider->pRigidbodyStatic;\n"
    "\t\t\t}\n",
    "\t\t\tphysx::PxRigidActor* actor =\n"
    "\t\t\t\tm_runtimeStore.GetActor(context, entity);\n",
    "download actor lookup",
)

helpers = r'''

namespace PhysicSystemTaskDetail {

inline void ClearRuntimeAliases(ColliderComponent& collider){
	collider.pRigidbodyDynamic = nullptr;
	collider.pRigidbodyStatic = nullptr;
	for(ColliderShape& shape : collider.colliders){
		shape.pxShape = nullptr;
		shape.pxMaterial = nullptr;
		shape.pxHeightField = nullptr;
		shape.pxTriangleMesh = nullptr;
		shape.pxConvexMesh = nullptr;
	}
}

inline PhysicsEntityRuntime CaptureRuntime(ColliderComponent& collider){
	PhysicsEntityRuntime runtime;
	runtime.dynamicActor = collider.pRigidbodyDynamic;
	runtime.staticActor = collider.pRigidbodyStatic;

	physx::PxRigidActor* actor = runtime.dynamicActor
		? static_cast<physx::PxRigidActor*>(runtime.dynamicActor)
		: static_cast<physx::PxRigidActor*>(runtime.staticActor);
	if(actor){
		runtime.actorUserData = actor->userData;
		runtime.destroyActorUserData = [](void* raw){
			delete static_cast<ActorEntityInfo*>(raw);
		};
	}

	runtime.shapes.reserve(collider.colliders.size());
	for(const ColliderShape& shape : collider.colliders){
		runtime.shapes.push_back({
			shape.pxShape,
			shape.pxMaterial,
			shape.pxHeightField,
			shape.pxTriangleMesh,
			shape.pxConvexMesh
		});
	}
	return runtime;
}

} // namespace PhysicSystemTaskDetail

inline void PhysicSystem::PreparePhysicsRuntime(){
	if(!m_context || !m_context->sceneManager) return;

	std::vector<PhysicsRuntimeKey> liveKeys;
	for(auto& [name, scene] : m_context->sceneManager->GetActiveScenes()){
		(void)name;
		if(!scene) continue;

		SceneContext* context = scene->GetSceneContext();
		if(!context || !context->component) continue;

		const auto entities =
			context->component->FindEntitiesWithComponent<ColliderComponent>();
		liveKeys.reserve(liveKeys.size() + entities.size());
		for(Entity entity : entities){
			liveKeys.push_back({context, entity});
			ColliderComponent* collider =
				context->component->GetComponent<ColliderComponent>(entity);
			if(!collider) continue;

			if(collider->needsUpdate && m_runtimeStore.Contains(context, entity)){
				m_runtimeStore.Release(context, entity);
				PhysicSystemTaskDetail::ClearRuntimeAliases(*collider);
			}
		}
	}

	m_runtimeStore.ReleaseMissing(liveKeys);
}

inline void PhysicSystem::CapturePhysicsRuntime(){
	if(!m_context || !m_context->sceneManager) return;

	for(auto& [name, scene] : m_context->sceneManager->GetActiveScenes()){
		(void)name;
		if(!scene) continue;

		SceneContext* context = scene->GetSceneContext();
		if(!context || !context->component) continue;

		const auto entities =
			context->component->FindEntitiesWithComponent<ColliderComponent>();
		for(Entity entity : entities){
			if(m_runtimeStore.Contains(context, entity)) continue;

			ColliderComponent* collider =
				context->component->GetComponent<ColliderComponent>(entity);
			if(!collider ||
				(!collider->pRigidbodyDynamic && !collider->pRigidbodyStatic)){
				continue;
			}

			m_runtimeStore.Adopt(
				context,
				entity,
				PhysicSystemTaskDetail::CaptureRuntime(*collider)
			);
		}
	}
}

inline void PhysicSystem::ReleaseAllPhysicsRuntime(){
	// Start直後、最初のFixed Tick前でも生成済みActorを回収できるようにする。
	CapturePhysicsRuntime();
	m_runtimeStore.ReleaseAll();

	if(!m_context || !m_context->sceneManager) return;
	for(auto& [name, scene] : m_context->sceneManager->GetActiveScenes()){
		(void)name;
		if(!scene) continue;
		SceneContext* context = scene->GetSceneContext();
		if(!context || !context->component) continue;

		const auto entities =
			context->component->FindEntitiesWithComponent<ColliderComponent>();
		for(Entity entity : entities){
			ColliderComponent* collider =
				context->component->GetComponent<ColliderComponent>(entity);
			if(collider){
				PhysicSystemTaskDetail::ClearRuntimeAliases(*collider);
			}
		}
	}
}

inline void PhysicSystem::DrainSimulationAndEvents(){
	ScriptCollisionDispatchBridge::Remove(this);

	if(g_pScene && m_simulationInFlight.load(std::memory_order_acquire)){
		g_pScene->lockRead();
		g_pScene->fetchResults(true);
		g_pScene->unlockRead();
		m_simulationInFlight.store(false, std::memory_order_release);
	}

	std::scoped_lock lock(m_collisionEventMutex);
	m_pendingCollisionEvents.clear();
}
'''

if "inline void PhysicSystem::PreparePhysicsRuntime()" in tasks:
    raise RuntimeError("physics runtime helpers already present")
tasks = tasks.rstrip() + helpers + "\n"
write(tasks_path, tasks)


# -----------------------------------------------------------------------
# Collider is data + non-owning runtime aliases. Inspector never releases.
# -----------------------------------------------------------------------
ops_path = "Source/GameApplication/Engine/Scene/Component/Operations/ColliderComponentOperations.h"
ops = read(ops_path)
ops = replace_once(
    ops,
    "\t\t\tif(ImGui::SmallButton(\"Remove\")){\n"
    "\t\t\t\tReleaseShapeRuntime(component.colliders[index]);\n"
    "\t\t\t\tcomponent.colliders.erase(component.colliders.begin() + index);",
    "\t\t\tif(ImGui::SmallButton(\"Remove\")){\n"
    "\t\t\t\t// PhysX資源はPhysicsRuntimeStoreが次のUploadで解放する。\n"
    "\t\t\t\tcomponent.colliders.erase(component.colliders.begin() + index);",
    "inspector shape release removal",
)
ops = replace_once(
    ops,
    "inline ColliderComponent::~ColliderComponent(){\n"
    "\tColliderComponentOperations::ReleaseActor(pRigidbodyStatic);\n"
    "\tColliderComponentOperations::ReleaseActor(pRigidbodyDynamic);\n"
    "}",
    "inline ColliderComponent::~ColliderComponent(){\n"
    "\t// PhysXネイティブ資源はPhysicsRuntimeStoreが所有する。\n"
    "\t// ここに残るポインタは移行期間中の非所有エイリアス。\n"
    "}",
    "collider destructor ownership removal",
)
write(ops_path, ops)


# -----------------------------------------------------------------------
# PhysicSystem lifecycle: drain, release Store, then release PhysX SDK.
# -----------------------------------------------------------------------
cpp_path = "Source/GameApplication/Engine/Scene/System/Physic/physicSystem.cpp"
cpp = read(cpp_path)
cpp = replace_function(
    cpp,
    "void PhysicSystem::Finalize()",
    r'''void PhysicSystem::Finalize(){
	OutputDebugStringA("PhysicSystem::Finalize\n");
	if(m_simCallback) m_simCallback->m_active = false;

	DrainSimulationAndEvents();
	ReleaseAllPhysicsRuntime();

	PxCloseExtensions();
	if(g_pScene){
		g_pScene->release();
		g_pScene = nullptr;
	}
	if(g_pDispatcher){
		g_pDispatcher->release();
		g_pDispatcher = nullptr;
	}
	if(g_pPhysics){
		g_pPhysics->release();
		g_pPhysics = nullptr;
	}

	if(g_pPvd){
		g_pPvd->disconnect();
		physx::PxPvdTransport* transport = g_pPvd->getTransport();
		if(transport) transport->release();
		g_pPvd->release();
		g_pPvd = nullptr;
	}

	if(g_pFoundation){
		g_pFoundation->release();
		g_pFoundation = nullptr;
	}

	g_physicSystem = nullptr;
	delete m_simCallback;
	m_simCallback = nullptr;
}''',
)
cpp = replace_function(
    cpp,
    "void PhysicSystem::Stop()",
    r'''void PhysicSystem::Stop(){
	if(m_simCallback) m_simCallback->m_active = false;

	DrainSimulationAndEvents();
	ReleaseAllPhysicsRuntime();
}''',
)
cpp = replace_function(
    cpp,
    "void PhysicSystem::FixedUpdate(float deltaTime)",
    r'''void PhysicSystem::FixedUpdate(float deltaTime){
	// RegisterTasks未使用時の互換経路。
	PhysicsUpload();
	PhysicsBegin(deltaTime);
	PhysicsFetch();
	PhysicsDownload();
	CollisionEventDispatch();
}''',
)
write(cpp_path, cpp)
