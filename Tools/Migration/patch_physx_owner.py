from pathlib import Path
import re

path = Path("Source/GameApplication/Engine/Scene/System/Physic/physicSystem.cpp")
text = path.read_text(encoding="utf-8-sig")

pattern = r"PhysicSystem\* g_physicSystem = nullptr;\s*\n\s*physx::PxFilterFlags PhysicsFilterShader\(.*?\n\}"
replacement = '''physx::PxFilterFlags PhysicsFilterShader(
\tphysx::PxFilterObjectAttributes attr0, physx::PxFilterData data0,
\tphysx::PxFilterObjectAttributes attr1, physx::PxFilterData data1,
\tphysx::PxPairFlags& pairFlags,
\tconst void* constantBlock,
\tphysx::PxU32 constantBlockSize) {
\tif(!constantBlock || constantBlockSize != sizeof(PhysicSystem*)){
\t\treturn physx::PxFilterFlag::eSUPPRESS;
\t}
\tPhysicSystem* owner = *static_cast<PhysicSystem* const*>(constantBlock);
\tif(!owner) return physx::PxFilterFlag::eSUPPRESS;
\tconst int layerA = owner->FindLayerIndex(data0.word0);
\tconst int layerB = owner->FindLayerIndex(data1.word0);
\tif(layerA < 0 || layerB < 0 || !owner->GetCollisionEnabled(layerA, layerB)){
\t\treturn physx::PxFilterFlag::eSUPPRESS;
\t}
\tif(physx::PxFilterObjectIsTrigger(attr0) || physx::PxFilterObjectIsTrigger(attr1)){
\t\tpairFlags = physx::PxPairFlag::eTRIGGER_DEFAULT;
\t\treturn physx::PxFilterFlag::eDEFAULT;
\t}
\tpairFlags = physx::PxPairFlag::eCONTACT_DEFAULT |
\t\tphysx::PxPairFlag::eDETECT_DISCRETE_CONTACT |
\t\tphysx::PxPairFlag::eNOTIFY_TOUCH_FOUND |
\t\tphysx::PxPairFlag::eNOTIFY_TOUCH_PERSISTS |
\t\tphysx::PxPairFlag::eNOTIFY_TOUCH_LOST;
\treturn physx::PxFilterFlag::eDEFAULT;
}'''
text, count = re.subn(pattern, replacement, text, count=1, flags=re.S)
if count != 1:
    raise SystemExit(f"filter replacement count={count}")

replacements = [
    ("\n\tg_physicSystem = this;\n", "\n"),
    ("\tscene_desc.filterShader = PhysicsFilterShader;\n\tscene_desc.cpuDispatcher = g_pDispatcher;\n\tm_simCallback = new PhysicsSimulationCallback();\n\tscene_desc.simulationEventCallback = m_simCallback;",
     "\tscene_desc.filterShader = PhysicsFilterShader;\n\tscene_desc.filterShaderData = &m_filterOwner;\n\tscene_desc.filterShaderDataSize = sizeof(m_filterOwner);\n\tscene_desc.cpuDispatcher = g_pDispatcher;\n\tm_simCallback = std::make_unique<PhysicsSimulationCallback>(*this);\n\tscene_desc.simulationEventCallback = m_simCallback.get();"),
    ("\n\tg_physicSystem = nullptr;\n\n\tdelete m_simCallback;\n\tm_simCallback = nullptr;", "\n\tm_simCallback.reset();"),
]
for old, new in replacements:
    if text.count(old) != 1:
        raise SystemExit("expected exact PhysX ownership snippet")
    text = text.replace(old, new, 1)

path.write_text(text, encoding="utf-8", newline="\n")
