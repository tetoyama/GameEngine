from pathlib import Path


def replace_block(text: str, marker: str, replacement: str, expected_count: int = 1) -> str:
    lines = text.splitlines(keepends=True)
    count = 0
    index = 0

    while index < len(lines):
        if marker not in lines[index]:
            index += 1
            continue

        start = index
        depth = 0
        began = False

        while index < len(lines):
            depth += lines[index].count("{")
            depth -= lines[index].count("}")
            began = began or "{" in lines[index]
            index += 1
            if began and depth == 0:
                break

        lines[start:index] = [replacement]
        index = start + 1
        count += 1

    if count != expected_count:
        raise RuntimeError(f"marker count mismatch: {marker}: {count}")

    return "".join(lines)


def migrate_scene() -> None:
    path = Path("Source/GameApplication/Engine/Scene/scene.cpp")
    text = path.read_text(encoding="utf-8-sig")

    replacement = """\t\tfor(ComponentView component : m_componentRegistry->GetAllComponentsOfEntitySorted(e)){\n\t\t\tconst std::string componentName = m_componentRegistry->GetComponentName(component);\n\t\t\tif(componentName.empty()) continue;\n\n\t\t\tYAML::Node componentNode;\n\t\t\tcomponentNode[\"Component\"] = componentName;\n\t\t\tconst YAML::Node encoded = m_componentRegistry->EncodeComponent(component);\n\t\t\tif(encoded && encoded.IsMap()){\n\t\t\t\tfor(auto iterator = encoded.begin(); iterator != encoded.end(); ++iterator){\n\t\t\t\t\tcomponentNode[iterator->first.as<std::string>()] = iterator->second;\n\t\t\t\t}\n\t\t\t}\n\t\t\tcomponentsNode.push_back(componentNode);\n\t\t}\n"""

    text = replace_block(
        text,
        "for (IComponent* comp : m_componentRegistry->GetAllComponentsOfEntitySorted(e))",
        replacement,
    )
    text = replace_block(
        text,
        "for(IComponent* comp : m_componentRegistry->GetAllComponentsOfEntitySorted(e))",
        replacement,
    )
    path.write_text(text, encoding="utf-8")


def migrate_prefab() -> None:
    path = Path("Source/GameApplication/Engine/Scene/Prefab/PrefabSystem.cpp")
    text = path.read_text(encoding="utf-8-sig")

    replacement = """\t\tfor(ComponentView component : context->component->GetAllComponentsOfEntitySorted(e)){\n\t\t\tconst std::string componentName = context->component->GetComponentName(component);\n\t\t\tif(componentName.empty()) continue;\n\n\t\t\tYAML::Node componentNode;\n\t\t\tcomponentNode[\"Component\"] = componentName;\n\t\t\tYAML::Node encoded = context->component->EncodeComponent(component);\n\t\t\tif(component.typeID == ComponentType::Get<TransformComponent>()){\n\t\t\t\tencoded[\"Parent\"] = 0;\n\t\t\t}\n\t\t\tif(encoded && encoded.IsMap()){\n\t\t\t\tfor(auto iterator = encoded.begin(); iterator != encoded.end(); ++iterator){\n\t\t\t\t\tcomponentNode[iterator->first.as<std::string>()] = iterator->second;\n\t\t\t\t}\n\t\t\t}\n\t\t\tcomponentsSeq.push_back(componentNode);\n\t\t}\n"""

    text = replace_block(
        text,
        "for (IComponent* comp : context->component->GetAllComponentsOfEntitySorted(e))",
        replacement,
    )
    path.write_text(text, encoding="utf-8")


def migrate_entity_commands() -> None:
    path = Path("Source/GameApplication/Engine/Editor/Command/EntityCommand.h")
    text = path.read_text(encoding="utf-8-sig")

    duplicate_replacement = """\t\tfor(ComponentView component : m_context->component->GetAllComponentsOfEntitySorted(src)){\n\t\t\tconst std::string componentName = m_context->component->GetComponentName(component);\n\t\t\tif(componentName.empty()) continue;\n\t\t\tm_context->component->CreateFromYAML(\n\t\t\t\tcomponentName,\n\t\t\t\tnewEntity,\n\t\t\t\tm_context->component->EncodeComponent(component)\n\t\t\t);\n\t\t}\n"""
    text = replace_block(
        text,
        "for (IComponent* comp : comps)",
        duplicate_replacement,
    )

    parent_replacement = """\t\tfor(ComponentView component : m_context->component->GetAllComponentsOfEntitySorted(m_newParent)){\n\t\t\tconst std::string componentName = m_context->component->GetComponentName(component);\n\t\t\tif(componentName.empty()) continue;\n\t\t\tm_snapshot.emplace_back(\n\t\t\t\tcomponentName,\n\t\t\t\tm_context->component->EncodeComponent(component)\n\t\t\t);\n\t\t}\n"""
    text = replace_block(
        text,
        "for (IComponent* comp : m_context->component->GetAllComponentsOfEntitySorted(m_newParent))",
        parent_replacement,
    )
    path.write_text(text, encoding="utf-8")


if __name__ == "__main__":
    migrate_scene()
    migrate_prefab()
    migrate_entity_commands()
