from pathlib import Path

header = Path("Source/GameApplication/Engine/Scene/System/Physic/physicSystem.h")
text = header.read_text(encoding="utf-8-sig")
old = "\tPhysicSystem(SceneManagerContext* context)\n\t\t: m_context(context),\n\t\t  m_filterOwner(this){}"
new = "\texplicit PhysicSystem(SceneManagerContext* context);"
if text.count(old) != 1:
    raise SystemExit("PhysicSystem constructor declaration anchor mismatch")
header.write_text(text.replace(old, new, 1), encoding="utf-8", newline="\n")

source = Path("Source/GameApplication/Engine/Scene/System/Physic/physicSystem.cpp")
text = source.read_text(encoding="utf-8-sig")
old = "PhysicSystem::~PhysicSystem() = default;"
new = "PhysicSystem::PhysicSystem(SceneManagerContext* context)\n\t: m_context(context),\n\t  m_filterOwner(this){}\n\nPhysicSystem::~PhysicSystem() = default;"
if text.count(old) != 1:
    raise SystemExit("PhysicSystem destructor anchor mismatch")
source.write_text(text.replace(old, new, 1), encoding="utf-8", newline="\n")
