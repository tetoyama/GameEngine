from pathlib import Path

TARGETS = [
    Path("Source/GameApplication/Engine/Scene/Component/entityNameComponent.h"),
    Path("Source/GameApplication/Engine/Scene/Component/transformComponent.h"),
    Path("Source/GameApplication/Engine/Scene/Component/RenderLayerComponent.h"),
    Path("Source/GameApplication/Engine/Scene/Component/materialComponent.h"),
    Path("Source/GameApplication/Engine/Scene/Component/LightComponent.h"),
    Path("Source/GameApplication/Engine/Scene/Component/particleComponent.h"),
    Path("Source/GameApplication/Engine/Scene/Component/cameraComponent.h"),
    Path("Source/GameApplication/Engine/Scene/Component/followComponent.h"),
    Path("Source/GameApplication/Engine/Scene/Component/environmentMapComponent.h"),
]

changed_files = []
removed_classes = 0

for path in TARGETS:
    text = path.read_text(encoding="utf-8-sig")

    if "public IComponent," in text or "public IComponent ," in text:
        raise RuntimeError(f"multiple inheritance requires manual migration: {path}")

    base_count = text.count(": public IComponent")
    if base_count == 0:
        raise RuntimeError(f"IComponent base not found: {path}")

    migrated = text.replace(": public IComponent", "")
    migrated = migrated.replace(" override", "")

    if ": public IComponent" in migrated:
        raise RuntimeError(f"IComponent base remains: {path}")

    path.write_text(migrated, encoding="utf-8")
    changed_files.append(str(path))
    removed_classes += base_count

print(f"Removed IComponent inheritance from {removed_classes} dense component classes")
for path in changed_files:
    print(path)
