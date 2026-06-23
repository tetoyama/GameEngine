from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8-sig")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding="utf-8")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one match, found {count}")
    return text.replace(old, new, 1)


# ---------------------------------------------------------------------------
# RenderPacket: capture all component bindings and the final world matrix.
# ---------------------------------------------------------------------------
packet_path = "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/RenderPacket/RenderPacket.h"
text = read(packet_path)
text = replace_once(
    text,
    '#include "System/Render/RenderSystem/renderLayer.h"\n',
    '#include "System/Render/RenderSystem/renderLayer.h"\n#include <DirectXMath.h>\n\n'
    'struct SceneContext;\n'
    'class TransformComponent;\n'
    'class MaterialComponent;\n'
    'class TextureComponent;\n'
    'class ModelRendererComponent;\n'
    'class MeshRendererComponent;\n'
    'class SpriteRendererComponent;\n'
    'class BillBoardRendererComponent;\n'
    'class ParticleComponent;\n'
    'class TerrainComponent;\n'
    'class WaveComponent;\n'
    'class EffectComponent;\n',
    "RenderPacket forward declarations",
)
text = replace_once(
    text,
    'struct RenderPacketTransformSnapshot {\n'
    '\tfloat position[3] = {0.0f, 0.0f, 0.0f};\n'
    '\tfloat worldPosition[3] = {0.0f, 0.0f, 0.0f};\n'
    '\tfloat rotation[4] = {0.0f, 0.0f, 0.0f, 1.0f};\n'
    '\tfloat scale[3] = {1.0f, 1.0f, 1.0f};\n'
    '};',
    'struct RenderPacketTransformSnapshot {\n'
    '\tfloat position[3] = {0.0f, 0.0f, 0.0f};\n'
    '\tfloat worldPosition[3] = {0.0f, 0.0f, 0.0f};\n'
    '\tfloat rotation[4] = {0.0f, 0.0f, 0.0f, 1.0f};\n'
    '\tfloat scale[3] = {1.0f, 1.0f, 1.0f};\n'
    '\tDirectX::XMFLOAT4X4 worldMatrix{};\n'
    '};\n\n'
    'struct RenderPacketComponentBindings {\n'
    '\tSceneContext* sceneContext = nullptr;\n'
    '\tTransformComponent* transform = nullptr;\n'
    '\tMaterialComponent* material = nullptr;\n'
    '\tTextureComponent* texture = nullptr;\n'
    '\tModelRendererComponent* modelRenderer = nullptr;\n'
    '\tMeshRendererComponent* meshRenderer = nullptr;\n'
    '\tSpriteRendererComponent* spriteRenderer = nullptr;\n'
    '\tBillBoardRendererComponent* billboardRenderer = nullptr;\n'
    '\tParticleComponent* particle = nullptr;\n'
    '\tTerrainComponent* terrain = nullptr;\n'
    '\tWaveComponent* wave = nullptr;\n'
    '\tEffectComponent* effect = nullptr;\n'
    '};',
    "RenderPacket snapshot and bindings",
)
text = replace_once(
    text,
    '\tRenderPacketTransformSnapshot transform;\n};',
    '\tRenderPacketTransformSnapshot transform;\n'
    '\tRenderPacketComponentBindings bindings;\n};',
    "RenderPacket binding member",
)
write(packet_path, text)


# ---------------------------------------------------------------------------
# IRenderable now receives the immutable frame packet directly.
# ---------------------------------------------------------------------------
interface_path = "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/Renderable/IRenderable.h"
text = read(interface_path)
text = replace_once(
    text,
    '#include "Scene/Entity/Entity.h"\n',
    '#include "System/Render/RenderSystem/RenderPacket/RenderPacket.h"\n',
    "IRenderable include",
)
text = re.sub(r'\nstruct SceneContext;\n|\nclass ComponentRegistry;\n', '\n', text)
text = replace_once(
    text,
    '\tvirtual void Execute(\n'
    '\t\tconst RenderPassContext& ctx,\n'
    '\t\tSceneContext* sceneContext,\n'
    '\t\tconst Entity& entity\n'
    '\t) = 0;',
    '\tvirtual void Execute(\n'
    '\t\tconst RenderPassContext& ctx,\n'
    '\t\tconst RenderPacket& packet\n'
    '\t) = 0;',
    "IRenderable Execute signature",
)
write(interface_path, text)


# ---------------------------------------------------------------------------
# RenderSystem packet build captures stable component pointers and world matrix.
# Scheduler hazards keep these bindings valid until MainThread submit completes.
# ---------------------------------------------------------------------------
render_system_path = "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/renderSystem.cpp"
text = read(render_system_path)
text = replace_once(
    text,
    '\t\t\tconst MaterialComponent* materialComponent =\n'
    '\t\t\t\tcomponents->GetComponent<MaterialComponent>(entity);\n'
    '\t\t\tconst bool isEnvironmentMap =\n'
    '\t\t\t\tcomponents->GetComponent<EnvironmentMapComponent>(entity) != nullptr;',
    '\t\t\tMaterialComponent* materialComponent =\n'
    '\t\t\t\tcomponents->GetComponent<MaterialComponent>(entity);\n'
    '\t\t\tTextureComponent* textureComponent =\n'
    '\t\t\t\tcomponents->GetComponent<TextureComponent>(entity);\n'
    '\t\t\tModelRendererComponent* modelRenderer =\n'
    '\t\t\t\tcomponents->GetComponent<ModelRendererComponent>(entity);\n'
    '\t\t\tMeshRendererComponent* meshRenderer =\n'
    '\t\t\t\tcomponents->GetComponent<MeshRendererComponent>(entity);\n'
    '\t\t\tSpriteRendererComponent* spriteRenderer =\n'
    '\t\t\t\tcomponents->GetComponent<SpriteRendererComponent>(entity);\n'
    '\t\t\tBillBoardRendererComponent* billboardRenderer =\n'
    '\t\t\t\tcomponents->GetComponent<BillBoardRendererComponent>(entity);\n'
    '\t\t\tParticleComponent* particle =\n'
    '\t\t\t\tcomponents->GetComponent<ParticleComponent>(entity);\n'
    '\t\t\tTerrainComponent* terrain =\n'
    '\t\t\t\tcomponents->GetComponent<TerrainComponent>(entity);\n'
    '\t\t\tWaveComponent* wave =\n'
    '\t\t\t\tcomponents->GetComponent<WaveComponent>(entity);\n'
    '\t\t\tEffectComponent* effect =\n'
    '\t\t\t\tcomponents->GetComponent<EffectComponent>(entity);\n'
    '\t\t\tconst bool isEnvironmentMap =\n'
    '\t\t\t\tcomponents->GetComponent<EnvironmentMapComponent>(entity) != nullptr;',
    "packet component capture",
)
text = replace_once(
    text,
    '\t\t\tsnapshot.scale[0] = transform->scale.x;\n'
    '\t\t\tsnapshot.scale[1] = transform->scale.y;\n'
    '\t\t\tsnapshot.scale[2] = transform->scale.z;\n',
    '\t\t\tsnapshot.scale[0] = transform->scale.x;\n'
    '\t\t\tsnapshot.scale[1] = transform->scale.y;\n'
    '\t\t\tsnapshot.scale[2] = transform->scale.z;\n'
    '\t\t\tDirectX::XMStoreFloat4x4(\n'
    '\t\t\t\t&snapshot.worldMatrix,\n'
    '\t\t\t\ttransform->CalculateWorldMatrix(transform, components)\n'
    '\t\t\t);\n',
    "packet world matrix snapshot",
)
text = replace_once(
    text,
    '\t\t\t\tpacket.stableSequence = stableSequence++;\n'
    '\t\t\t\tpacket.transform = snapshot;\n'
    '\t\t\t\tworker.Add(std::move(packet));',
    '\t\t\t\tpacket.stableSequence = stableSequence++;\n'
    '\t\t\t\tpacket.transform = snapshot;\n'
    '\t\t\t\tpacket.bindings.sceneContext = sceneEntry.context;\n'
    '\t\t\t\tpacket.bindings.transform = transform;\n'
    '\t\t\t\tpacket.bindings.material = materialComponent;\n'
    '\t\t\t\tpacket.bindings.texture = textureComponent;\n'
    '\t\t\t\tpacket.bindings.modelRenderer = modelRenderer;\n'
    '\t\t\t\tpacket.bindings.meshRenderer = meshRenderer;\n'
    '\t\t\t\tpacket.bindings.spriteRenderer = spriteRenderer;\n'
    '\t\t\t\tpacket.bindings.billboardRenderer = billboardRenderer;\n'
    '\t\t\t\tpacket.bindings.particle = particle;\n'
    '\t\t\t\tpacket.bindings.terrain = terrain;\n'
    '\t\t\t\tpacket.bindings.wave = wave;\n'
    '\t\t\t\tpacket.bindings.effect = effect;\n'
    '\t\t\t\tworker.Add(std::move(packet));',
    "packet binding publication",
)
for component, variable in [
    ("ModelRendererComponent", "modelRenderer"),
    ("MeshRendererComponent", "meshRenderer"),
    ("SpriteRendererComponent", "spriteRenderer"),
    ("BillBoardRendererComponent", "billboardRenderer"),
    ("ParticleComponent", "particle"),
    ("TerrainComponent", "terrain"),
    ("WaveComponent", "wave"),
    ("EffectComponent", "effect"),
]:
    text = text.replace(
        f'if(components->GetComponent<{component}>(entity)){{',
        f'if({variable}){{'
    )
write(render_system_path, text)


# ---------------------------------------------------------------------------
# Passes submit packets directly; scene lookup is no longer part of draw submit.
# ---------------------------------------------------------------------------
pass_files = [
    "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/RenderPass/GBuffer/GBufferPass.cpp",
    "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/RenderPass/Forward/ForwardPass.cpp",
    "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/RenderPass/OverlayUI/OverlayUIPass.cpp",
    "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/RenderPass/ShadowMap/ShadowMapPass.cpp",
]
for path in pass_files:
    text = read(path)
    text = re.sub(
        r'\n\s*SceneContext\* sceneContext\s*=\s*\n?\s*m_context->sceneManager->GetContextFromID\(packet\.sceneContextID\);\s*\n\s*if\(!sceneContext\) continue;\s*\n',
        '\n',
        text,
    )
    text, count = re.subn(
        r'renderable->Execute\(([^;]*?),\s*sceneContext,\s*packet\.entity\);',
        r'renderable->Execute(\1, packet);',
        text,
    )
    if count == 0:
        raise RuntimeError(f"{path}: no packet Execute call migrated")
    write(path, text)


# ---------------------------------------------------------------------------
# All Renderable declarations and definitions consume packet bindings.
# ---------------------------------------------------------------------------
renderable_root = ROOT / "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/Renderable"
headers = [p for p in renderable_root.rglob("*.h") if p.name != "IRenderable.h"]
for file in headers:
    text = file.read_text(encoding="utf-8-sig")
    new_text, count = re.subn(
        r'void Execute\(\s*const RenderPassContext& ctx,\s*SceneContext\* sceneContext,\s*const Entity& entity\s*\) override;',
        'void Execute(\n\t\tconst RenderPassContext& ctx,\n\t\tconst RenderPacket& packet\n\t) override;',
        text,
        flags=re.S,
    )
    if count:
        file.write_text(new_text, encoding="utf-8")

cpp_files = list(renderable_root.rglob("*.cpp"))
component_fields = {
    "TransformComponent": "transform",
    "MaterialComponent": "material",
    "TextureComponent": "texture",
    "ModelRendererComponent": "modelRenderer",
    "MeshRendererComponent": "meshRenderer",
    "SpriteRendererComponent": "spriteRenderer",
    "BillBoardRendererComponent": "billboardRenderer",
    "ParticleComponent": "particle",
    "TerrainComponent": "terrain",
    "WaveComponent": "wave",
    "EffectComponent": "effect",
}
for file in cpp_files:
    text = file.read_text(encoding="utf-8-sig")
    pattern = (
        r'(void\s+\w+::Execute\(\s*const RenderPassContext& ctx,\s*)'
        r'SceneContext\* sceneContext,\s*const Entity& entity\s*\)\s*\{'
    )
    replacement = (
        r'\1const RenderPacket& packet){\n'
        r'\tSceneContext* sceneContext = packet.bindings.sceneContext;\n'
        r'\tconst Entity& entity = packet.entity;\n'
        r'\tif(!sceneContext) return;'
    )
    text, signature_count = re.subn(pattern, replacement, text, flags=re.S)
    if signature_count == 0:
        continue

    for component, field in component_fields.items():
        text = re.sub(
            rf'sceneContext->component->GetComponent<{component}>\(entity\)',
            f'packet.bindings.{field}',
            text,
        )

    text = re.sub(
        r'transform->CalculateWorldMatrix\(\s*transform,\s*sceneContext->component\s*\)',
        'DirectX::XMLoadFloat4x4(&packet.transform.worldMatrix)',
        text,
        flags=re.S,
    )

    file.write_text(text, encoding="utf-8")


# Hard guard: Renderable submit must not perform component lookup anymore.
violations = []
for file in cpp_files:
    text = file.read_text(encoding="utf-8")
    if "sceneContext->component->GetComponent<" in text:
        violations.append(str(file.relative_to(ROOT)))
if violations:
    raise RuntimeError(
        "Renderable ComponentRegistry lookup remains:\n" + "\n".join(violations)
    )

print("Render Packet direct binding migration completed.")
