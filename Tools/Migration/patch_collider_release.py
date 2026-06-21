from pathlib import Path


def load(path: str) -> str:
    return Path(path).read_text(encoding="utf-8-sig")


def save(path: str, text: str) -> None:
    Path(path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


header = "Source/GameApplication/Engine/Scene/Component/ColliderComponent.h"
text = load(header)
text = replace_once(
    text,
    "\tvoid OnBeforeRemove(SceneContext*) override {}",
    "\tvoid OnBeforeRemove(SceneContext* context) override;",
    "Collider removal declaration",
)
save(header, text)

ops = "Source/GameApplication/Engine/Scene/Component/Operations/ColliderComponentOperations.h"
text = load(ops)
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
    "Shape release bridge",
)
text = replace_once(
    text,
    "inline ColliderComponent::~ColliderComponent(){\n"
    "\tPhysicsRuntimeReleaseBridge::ReleaseEntity(this);\n"
    "}\n",
    "inline ColliderComponent::~ColliderComponent() = default;\n\n"
    "inline void ColliderComponent::OnBeforeRemove(SceneContext* context){\n"
    "\tif(!context || !context->system) return;\n"
    "\tif(auto* physics = context->system->GetSystem<PhysicSystem>()){\n"
    "\t\tphysics->ReleaseColliderRuntime(this);\n"
    "\t}\n"
    "}\n",
    "Entity release bridge",
)
save(ops, text)

bridge = "Source/GameApplication/Engine/Scene/System/Physic/PhysicsRuntimeReleaseBridge.h"
save(
    bridge,
    "#pragma once\n\n"
    "// Retired: Collider runtime resources are released by resolving\n"
    "// PhysicSystem from SceneContext before storage removal.\n",
)
