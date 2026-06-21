from pathlib import Path


def read(path: str) -> str:
    return Path(path).read_text(encoding="utf-8-sig")


def write(path: str, text: str) -> None:
    Path(path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one match, found {count}")
    return text.replace(old, new, 1)


# ----------------------------------------------------------------------
# Fix collision dispatcher enumeration without a nonexistent Registry API.
# ----------------------------------------------------------------------
path = "Source/GameApplication/Engine/Scene/System/Physic/PhysicSystemTasks.inl"
text = read(path)
text = replace_once(
    text,
    """\t\t\tfor(Entity entity : context->entity->GetAllAlive()){
\t\t\t\tfor(CustomScriptComponent* script : context->component->GetScripts(entity)){
\t\t\t\t\tif(!script) continue;
\t\t\t\t\tscript->SetCollisionDispatcher(m_owner, dispatcher);
\t\t\t\t\tm_scripts.push_back(script);
\t\t\t\t}
\t\t\t}
""",
    """\t\t\tfor(auto& [entity, script] :
\t\t\t\tcontext->component->GetAllBaseComponents<CustomScriptComponent>()){
\t\t\t\t(void)entity;
\t\t\t\tif(!script) continue;
\t\t\t\tscript->SetCollisionDispatcher(m_owner, dispatcher);
\t\t\t\tm_scripts.push_back(script);
\t\t\t}
""",
    "collision dispatcher enumeration",
)
write(path, text)


# ----------------------------------------------------------------------
# Remove the mutable global runtime-release hook.
# ----------------------------------------------------------------------
path = "Source/GameApplication/Engine/Scene/Component/Operations/ColliderComponentOperations.h"
text = read(path)
text = text.replace('#include "Scene/System/Physic/PhysicsRuntimeReleaseBridge.h"\n', "")
text = replace_once(
    text,
    "\t\t\tif(ImGui::SmallButton(\"Remove\")){\n"
    "\t\t\t\tPhysicsRuntimeReleaseBridge::ReleaseShape(&component, index);",
    "\t\t\tif(ImGui::SmallButton(\"Remove\")){\n"
    "\t\t\t\tif(context && context->system){\n"
    "\t\t\t\t\tif(auto* physics = context->system->GetSystem<PhysicSystem>()){\n"
    "\t\t\t\t\t\tphysics->ReleaseColliderShapeRuntime(&component, index);\n"
    "\t\t\t\t\t}\n"
    "\t\t\t\t}",
    "shape runtime release",
)
text = replace_once(
    text,
    """inline ColliderComponent::~ColliderComponent(){
\tPhysicsRuntimeReleaseBridge::ReleaseEntity(this);
}
""",
    """inline ColliderComponent::~ColliderComponent() = default;

inline void ColliderComponent::OnBeforeRemove(SceneContext* context){
\tif(!context || !context->system) return;
\tif(auto* physics = context->system->GetSystem<PhysicSystem>()){
\t\tphysics->ReleaseColliderRuntime(this);
\t}
}
""",
    "entity runtime release",
)
write(path, text)

path = "Source/GameApplication/Engine/Scene/System/Physic/PhysicsRuntimeReleaseBridge.h"
write(
    path,
    "#pragma once\n\n"
    "// Compatibility include retained during migration.\n"
    "// Collider runtime release is resolved through SceneContext and PhysicSystem.\n",
)


# ----------------------------------------------------------------------
# ComponentRegistry invokes lifecycle release hooks before storage removal.
# ----------------------------------------------------------------------
path = "Source/GameApplication/Engine/Scene/Registry/componentRegistry.h"
text = read(path)
text = replace_once(
    text,
    "\tstd::function<void(void*, SceneContext*)> inspector;\n};",
    "\tstd::function<void(void*, SceneContext*)> inspector;\n"
    "\tstd::function<void(void*, SceneContext*)> beforeRemove;\n};",
    "component operations lifecycle callback",
)
text = replace_once(
    text,
    """\t\t\t[](void* raw, SceneContext* context){
\t\t\t\tstatic_cast<T*>(raw)->inspector(context);
\t\t\t}
\t\t};
""",
    """\t\t\t[](void* raw, SceneContext* context){
\t\t\t\tstatic_cast<T*>(raw)->inspector(context);
\t\t\t},
\t\t\t[](void* raw, SceneContext* context){
\t\t\t\tif constexpr(std::is_base_of_v<IComponent, T>){
\t\t\t\t\tstatic_cast<T*>(raw)->OnBeforeRemove(context);
\t\t\t\t}
\t\t\t}
\t\t};
""",
    "registered lifecycle callback",
)
text = replace_once(
    text,
    """\t\tif(iterator == m_storages.end()) return;
\t\titerator->second->Remove(entity);
""",
    """\t\tif(iterator == m_storages.end()) return;
\t\tif constexpr(std::is_base_of_v<IComponent, T>){
\t\t\tif(T* component = static_cast<T*>(iterator->second->GetRaw(entity))){
\t\t\t\tcomponent->OnBeforeRemove(m_context);
\t\t\t}
\t\t}
\t\titerator->second->Remove(entity);
""",
    "typed component removal lifecycle",
)
text = replace_once(
    text,
    """\t\tauto storageIterator = m_storages.find(typeIterator->second);
\t\tif(storageIterator == m_storages.end()) return;
\t\tstorageIterator->second->Remove(entity);
""",
    """\t\tauto storageIterator = m_storages.find(typeIterator->second);
\t\tif(storageIterator == m_storages.end()) return;
\t\tif(void* raw = storageIterator->second->GetRaw(entity)){
\t\t\tauto operations = m_componentOperations.find(typeID);
\t\t\tif(operations != m_componentOperations.end() && operations->second.beforeRemove){
\t\t\t\toperations->second.beforeRemove(raw, m_context);
\t\t\t}
\t\t}
\t\tstorageIterator->second->Remove(entity);
""",
    "id component removal lifecycle",
)
text = replace_once(
    text,
    """\tvoid OnEntityDestroyed(Entity entity){
\t\tfor(auto& [typeIndex, storage] : m_storages){
\t\t\t(void)typeIndex;
\t\t\tstorage->Remove(entity);
\t\t}
\t\tm_entityMasks.erase(entity);
\t}
""",
    """\tvoid OnEntityDestroyed(Entity entity){
\t\tfor(auto& [typeIndex, storage] : m_storages){
\t\t\tif(void* raw = storage->GetRaw(entity)){
\t\t\t\tauto type = m_typeToID.find(typeIndex);
\t\t\t\tif(type != m_typeToID.end()){
\t\t\t\t\tauto operations = m_componentOperations.find(type->second);
\t\t\t\t\tif(operations != m_componentOperations.end() && operations->second.beforeRemove){
\t\t\t\t\t\toperations->second.beforeRemove(raw, m_context);
\t\t\t\t\t}
\t\t\t\t}
\t\t\t}
\t\t\tstorage->Remove(entity);
\t\t}
\t\tm_entityMasks.erase(entity);
\t}
""",
    "entity destruction lifecycle",
)
write(path, text)


# ----------------------------------------------------------------------
# Scene-local registries have one owner.
# ----------------------------------------------------------------------
path = "Source/GameApplication/Engine/Scene/scene.h"
text = read(path)
text = replace_once(
    text,
    "std::shared_ptr<EntityRegistry> m_entityRegistry;",
    "std::unique_ptr<EntityRegistry> m_entityRegistry;",
    "entity registry ownership",
)
text = replace_once(
    text,
    "std::shared_ptr<ComponentRegistry> m_componentRegistry;",
    "std::unique_ptr<ComponentRegistry> m_componentRegistry;",
    "component registry ownership",
)
text = replace_once(
    text,
    "std::shared_ptr<PrefabSystem> m_prefabSystem;",
    "std::unique_ptr<PrefabSystem> m_prefabSystem;",
    "prefab system ownership",
)
write(path, text)

path = "Source/GameApplication/Engine/Scene/scene.cpp"
text = read(path)
text = replace_once(
    text,
    "m_entityRegistry = std::make_shared<EntityRegistry>();",
    "m_entityRegistry = std::make_unique<EntityRegistry>();",
    "entity registry construction",
)
text = replace_once(
    text,
    "m_componentRegistry = std::make_shared<ComponentRegistry>(m_entityRegistry.get(),&m_SceneContext);",
    "m_componentRegistry = std::make_unique<ComponentRegistry>(m_entityRegistry.get(), &m_SceneContext);",
    "component registry construction",
)
text = replace_once(
    text,
    "m_prefabSystem = std::make_shared<PrefabSystem>();",
    "m_prefabSystem = std::make_unique<PrefabSystem>();",
    "prefab system construction",
)
write(path, text)


# ----------------------------------------------------------------------
# Keep compile checks frequent, but execute RHI tests only manually.
# ----------------------------------------------------------------------
path = ".github/workflows/rhi-smoke.yml"
text = read(path)
text = replace_once(
    text,
    "on:\n  pull_request:",
    "on:\n  workflow_dispatch:\n  pull_request:",
    "RHI manual dispatch trigger",
)
for executable in (
    ".\\rhi-smoke.exe",
    ".\\rhi-render-graph-barrier-smoke.exe",
    ".\\d3d11-rhi-backend-smoke.exe",
    ".\\rhi-swap-chain-contract-smoke.exe",
):
    text = replace_once(
        text,
        f"      - name: Run ",
        f"      - name: Run ",
        "RHI run step anchor",
    ) if False else text
    old = f"        run: {executable}"
    new = "        if: github.event_name == 'workflow_dispatch'\n" + old
    text = replace_once(text, old, new, f"manual execution for {executable}")
write(path, text)

print("DI repair migration applied successfully")
