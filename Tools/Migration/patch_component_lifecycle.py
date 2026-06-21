from pathlib import Path

path = Path("Source/GameApplication/Engine/Scene/Registry/componentRegistry.h")
text = path.read_text(encoding="utf-8-sig")


def replace_once(old: str, new: str, label: str) -> None:
    global text
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected one match, found {count}")
    text = text.replace(old, new, 1)


replace_once(
    "\tstd::function<void(void*, SceneContext*)> inspector;\n};",
    "\tstd::function<void(void*, SceneContext*)> inspector;\n"
    "\tstd::function<void(void*, SceneContext*)> beforeRemove;\n};",
    "ComponentOperations beforeRemove",
)

replace_once(
    "\t\t\t[](void* raw, SceneContext* context){\n"
    "\t\t\t\tstatic_cast<T*>(raw)->inspector(context);\n"
    "\t\t\t}\n"
    "\t\t};",
    "\t\t\t[](void* raw, SceneContext* context){\n"
    "\t\t\t\tstatic_cast<T*>(raw)->inspector(context);\n"
    "\t\t\t},\n"
    "\t\t\t[](void* raw, SceneContext* context){\n"
    "\t\t\t\tif constexpr(std::is_base_of_v<IComponent, T>){\n"
    "\t\t\t\t\tstatic_cast<T*>(raw)->OnBeforeRemove(context);\n"
    "\t\t\t\t}\n"
    "\t\t\t}\n"
    "\t\t};",
    "Registered removal callback",
)

replace_once(
    "\t\tif(iterator == m_storages.end()) return;\n"
    "\t\titerator->second->Remove(entity);",
    "\t\tif(iterator == m_storages.end()) return;\n"
    "\t\tif constexpr(std::is_base_of_v<IComponent, T>){\n"
    "\t\t\tif(T* component = static_cast<T*>(iterator->second->GetRaw(entity))){\n"
    "\t\t\t\tcomponent->OnBeforeRemove(m_context);\n"
    "\t\t\t}\n"
    "\t\t}\n"
    "\t\titerator->second->Remove(entity);",
    "Typed component removal",
)

replace_once(
    "\t\tauto storageIterator = m_storages.find(typeIterator->second);\n"
    "\t\tif(storageIterator == m_storages.end()) return;\n"
    "\t\tstorageIterator->second->Remove(entity);",
    "\t\tauto storageIterator = m_storages.find(typeIterator->second);\n"
    "\t\tif(storageIterator == m_storages.end()) return;\n"
    "\t\tif(void* raw = storageIterator->second->GetRaw(entity)){\n"
    "\t\t\tauto operations = m_componentOperations.find(typeID);\n"
    "\t\t\tif(operations != m_componentOperations.end() && operations->second.beforeRemove){\n"
    "\t\t\t\toperations->second.beforeRemove(raw, m_context);\n"
    "\t\t\t}\n"
    "\t\t}\n"
    "\t\tstorageIterator->second->Remove(entity);",
    "ID component removal",
)

replace_once(
    "\tvoid OnEntityDestroyed(Entity entity){\n"
    "\t\tfor(auto& [typeIndex, storage] : m_storages){\n"
    "\t\t\t(void)typeIndex;\n"
    "\t\t\tstorage->Remove(entity);\n"
    "\t\t}\n"
    "\t\tm_entityMasks.erase(entity);\n"
    "\t}",
    "\tvoid OnEntityDestroyed(Entity entity){\n"
    "\t\tfor(auto& [typeIndex, storage] : m_storages){\n"
    "\t\t\tif(void* raw = storage->GetRaw(entity)){\n"
    "\t\t\t\tauto type = m_typeToID.find(typeIndex);\n"
    "\t\t\t\tif(type != m_typeToID.end()){\n"
    "\t\t\t\t\tauto operations = m_componentOperations.find(type->second);\n"
    "\t\t\t\t\tif(operations != m_componentOperations.end() && operations->second.beforeRemove){\n"
    "\t\t\t\t\t\toperations->second.beforeRemove(raw, m_context);\n"
    "\t\t\t\t\t}\n"
    "\t\t\t\t}\n"
    "\t\t\t}\n"
    "\t\t\tstorage->Remove(entity);\n"
    "\t\t}\n"
    "\t\tm_entityMasks.erase(entity);\n"
    "\t}",
    "Entity destruction lifecycle",
)

path.write_text(text, encoding="utf-8", newline="\n")
