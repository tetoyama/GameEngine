from pathlib import Path


def replace_one(path: str, pairs: list[tuple[str, str, str]]) -> None:
    file = Path(path)
    text = file.read_text(encoding="utf-8-sig")
    for old, new, label in pairs:
        count = text.count(old)
        if count != 1:
            raise SystemExit(f"{label}: expected one match, found {count}")
        text = text.replace(old, new, 1)
    file.write_text(text, encoding="utf-8", newline="\n")


replace_one(
    "Source/GameApplication/Engine/Scene/scene.h",
    [
        ("std::shared_ptr<EntityRegistry> m_entityRegistry;", "std::unique_ptr<EntityRegistry> m_entityRegistry;", "EntityRegistry owner"),
        ("std::shared_ptr<ComponentRegistry> m_componentRegistry;", "std::unique_ptr<ComponentRegistry> m_componentRegistry;", "ComponentRegistry owner"),
        ("std::shared_ptr<PrefabSystem> m_prefabSystem;", "std::unique_ptr<PrefabSystem> m_prefabSystem;", "PrefabSystem owner"),
    ],
)

replace_one(
    "Source/GameApplication/Engine/Scene/scene.cpp",
    [
        ("m_entityRegistry = std::make_shared<EntityRegistry>();", "m_entityRegistry = std::make_unique<EntityRegistry>();", "EntityRegistry construction"),
        ("m_componentRegistry = std::make_shared<ComponentRegistry>(m_entityRegistry.get(),&m_SceneContext);", "m_componentRegistry = std::make_unique<ComponentRegistry>(m_entityRegistry.get(), &m_SceneContext);", "ComponentRegistry construction"),
        ("m_prefabSystem = std::make_shared<PrefabSystem>();", "m_prefabSystem = std::make_unique<PrefabSystem>();", "PrefabSystem construction"),
    ],
)
