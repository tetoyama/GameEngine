from pathlib import Path
import re

cpp_path = Path("Source/GameApplication/Engine/Scene/System/Physic/physicSystem.cpp")
cpp = cpp_path.read_text(encoding="utf-8-sig")

old = "public:\n\t// Stop/Finalize 前に false にセットすることでコールバックを無効化する"
new = "public:\n\texplicit PhysicsSimulationCallback(PhysicSystem& owner)\n\t\t: m_owner(owner){}\n\n\t// Stop/Finalize 前に false にセットすることでコールバックを無効化する"
if cpp.count(old) != 1:
    raise SystemExit("callback constructor anchor mismatch")
cpp = cpp.replace(old, new, 1)

mapping = {
    "for (auto* s : scriptsA) s->CollisionEnter(hitForA);": "for(auto* s : scriptsA) m_owner.QueueScriptCollisionEvent(s, ScriptCollisionEventType::CollisionEnter, hitForA);",
    "for (auto* s : scriptsB) s->CollisionEnter(hitForB);": "for(auto* s : scriptsB) m_owner.QueueScriptCollisionEvent(s, ScriptCollisionEventType::CollisionEnter, hitForB);",
    "for (auto* s : scriptsA) s->CollisionStay(hitForA);": "for(auto* s : scriptsA) m_owner.QueueScriptCollisionEvent(s, ScriptCollisionEventType::CollisionStay, hitForA);",
    "for (auto* s : scriptsB) s->CollisionStay(hitForB);": "for(auto* s : scriptsB) m_owner.QueueScriptCollisionEvent(s, ScriptCollisionEventType::CollisionStay, hitForB);",
    "for (auto* s : scriptsA) s->CollisionExit(hitForA);": "for(auto* s : scriptsA) m_owner.QueueScriptCollisionEvent(s, ScriptCollisionEventType::CollisionExit, hitForA);",
    "for (auto* s : scriptsB) s->CollisionExit(hitForB);": "for(auto* s : scriptsB) m_owner.QueueScriptCollisionEvent(s, ScriptCollisionEventType::CollisionExit, hitForB);",
    "for (auto* s : scriptsTrigger) s->TriggerEnter(hitForTrigger);": "for(auto* s : scriptsTrigger) m_owner.QueueScriptCollisionEvent(s, ScriptCollisionEventType::TriggerEnter, hitForTrigger);",
    "for (auto* s : scriptsOther)   s->TriggerEnter(hitForOther);": "for(auto* s : scriptsOther) m_owner.QueueScriptCollisionEvent(s, ScriptCollisionEventType::TriggerEnter, hitForOther);",
    "for (auto* s : scriptsTrigger) s->TriggerExit(hitForTrigger);": "for(auto* s : scriptsTrigger) m_owner.QueueScriptCollisionEvent(s, ScriptCollisionEventType::TriggerExit, hitForTrigger);",
    "for (auto* s : scriptsOther)   s->TriggerExit(hitForOther);": "for(auto* s : scriptsOther) m_owner.QueueScriptCollisionEvent(s, ScriptCollisionEventType::TriggerExit, hitForOther);",
}
for source, target in mapping.items():
    if cpp.count(source) != 1:
        raise SystemExit(f"event anchor mismatch: {source}")
    cpp = cpp.replace(source, target, 1)

old = "\tvoid onAdvance(const physx::PxRigidBody* const*, const physx::PxTransform*, physx::PxU32) override {}\n};"
new = "\tvoid onAdvance(const physx::PxRigidBody* const*, const physx::PxTransform*, physx::PxU32) override {}\n\nprivate:\n\tPhysicSystem& m_owner;\n};\n\nPhysicSystem::~PhysicSystem() = default;"
if cpp.count(old) != 1:
    raise SystemExit("callback footer anchor mismatch")
cpp = cpp.replace(old, new, 1)
cpp_path.write_text(cpp, encoding="utf-8", newline="\n")

inl_path = Path("Source/GameApplication/Engine/Scene/System/Physic/PhysicSystemTasks.inl")
inl = inl_path.read_text(encoding="utf-8-sig")
inl, count = re.subn(r"\nclass CollisionDispatchScope \{.*?\n\};\n", "\n", inl, count=1, flags=re.S)
if count != 1:
    raise SystemExit(f"dispatch scope replacement count={count}")
inl, count = re.subn(r"\n\tPhysicSystemTaskDetail::CollisionDispatchScope dispatchScope\(.*?\n\t\);\n", "\n", inl, count=1, flags=re.S)
if count != 1:
    raise SystemExit(f"fetch scope replacement count={count}")
inl_path.write_text(inl, encoding="utf-8", newline="\n")
