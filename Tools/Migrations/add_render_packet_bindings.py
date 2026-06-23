from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8-sig")


def write(path: str, text: str) -> None:
    target = ROOT / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(text, encoding="utf-8")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one match, found {count}")
    return text.replace(old, new, 1)


PACKET_HEADER = "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/RenderPacket/RenderPacket.h"
PACKET_DX11 = "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/RenderPacket/RenderPacketTransformDX11.h"
IRENDERABLE = "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/Renderable/IRenderable.h"
RENDER_SYSTEM = "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/renderSystem.cpp"


# ---------------------------------------------------------------------------
# API-independent packet matrix. DirectX conversion remains outside the packet.
# ---------------------------------------------------------------------------
text = read(PACKET_HEADER)
text = replace_once(
    text,
    '#include "System/Render/RenderSystem/renderLayer.h"\n',
    '#include "System/Render/RenderSystem/renderLayer.h"\n\n'
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
    "RenderPacket declarations",
)
text = replace_once(
    text,
    'struct RenderPacketTransformSnapshot {\n'
    '\tfloat position[3] = {0.0f, 0.0f, 0.0f};\n'
    '\tfloat worldPosition[3] = {0.0f, 0.0f, 0.0f};\n'
    '\tfloat rotation[4] = {0.0f, 0.0f, 0.0f, 1.0f};\n'
    '\tfloat scale[3] = {1.0f, 1.0f, 1.0f};\n'
    '};',
    'struct RenderPacketMatrix4x4 {\n'
    '\tfloat values[16] = {\n'
    '\t\t1.0f, 0.0f, 0.0f, 0.0f,\n'
    '\t\t0.0f, 1.0f, 0.0f, 0.0f,\n'
    '\t\t0.0f, 0.0f, 1.0f, 0.0f,\n'
    '\t\t0.0f, 0.0f, 0.0f, 1.0f\n'
    '\t};\n'
    '};\n\n'
    'struct RenderPacketTransformSnapshot {\n'
    '\tfloat position[3] = {0.0f, 0.0f, 0.0f};\n'
    '\tfloat worldPosition[3] = {0.0f, 0.0f, 0.0f};\n'
    '\tfloat rotation[4] = {0.0f, 0.0f, 0.0f, 1.0f};\n'
    '\tfloat scale[3] = {1.0f, 1.0f, 1.0f};\n'
    '\tRenderPacketMatrix4x4 worldMatrix;\n'
    '\tRenderPacketMatrix4x4 parentWorldMatrix;\n'
    '\tbool hasParentWorld = false;\n'
    '};\n\n'
    '// Frame-local, non-owning bindings. Scheduler read hazards and the\n'
    '// MainThread submit barrier keep these addresses stable until submit ends.\n'
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
    "RenderPacket snapshot",
)
text = replace_once(
    text,
    '\tRenderPacketTransformSnapshot transform;\n};',
    '\tRenderPacketTransformSnapshot transform;\n'
    '\tRenderPacketComponentBindings bindings;\n};',
    "RenderPacket bindings member",
)
write(PACKET_HEADER, text)

write(
    PACKET_DX11,
    '''#pragma once

#include <DirectXMath.h>

#include "RenderPacket.h"

inline void StoreRenderPacketMatrix(
    RenderPacketMatrix4x4& destination,
    DirectX::FXMMATRIX matrix
) noexcept {
    DirectX::XMFLOAT4X4 value{};
    DirectX::XMStoreFloat4x4(&value, matrix);
    const float source[16] = {
        value._11, value._12, value._13, value._14,
        value._21, value._22, value._23, value._24,
        value._31, value._32, value._33, value._34,
        value._41, value._42, value._43, value._44
    };
    for(size_t index = 0; index < 16; ++index){
        destination.values[index] = source[index];
    }
}

inline DirectX::XMMATRIX LoadRenderPacketMatrix(
    const RenderPacketMatrix4x4& source
) noexcept {
    return DirectX::XMMatrixSet(
        source.values[0], source.values[1], source.values[2], source.values[3],
        source.values[4], source.values[5], source.values[6], source.values[7],
        source.values[8], source.values[9], source.values[10], source.values[11],
        source.values[12], source.values[13], source.values[14], source.values[15]
    );
}
'''
)


# ---------------------------------------------------------------------------
# Submit interface consumes a complete packet, not an Entity lookup key.
# ---------------------------------------------------------------------------
text = read(IRENDERABLE)
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
    "IRenderable signature",
)
write(IRENDERABLE, text)


# ---------------------------------------------------------------------------
# Packet extraction captures all bindings and final hierarchy matrices once.
# ---------------------------------------------------------------------------
text = read(RENDER_SYSTEM)
text = replace_once(
    text,
    '#include "System/Render/RenderSystem/renderLayer.h"\n',
    '#include "System/Render/RenderSystem/renderLayer.h"\n'
    '#include "System/Render/RenderSystem/RenderPacket/RenderPacketTransformDX11.h"\n',
    "RenderSystem matrix adapter include",
)
text = replace_once(
    text,
    '\t\t\tconst TransformComponent* transform =\n'
    '\t\t\t\tcomponents->GetComponent<TransformComponent>(entity);',
    '\t\t\tTransformComponent* transform =\n'
    '\t\t\t\tcomponents->GetComponent<TransformComponent>(entity);',
    "mutable frame binding",
)
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
    '\t\t\tStoreRenderPacketMatrix(\n'
    '\t\t\t\tsnapshot.worldMatrix,\n'
    '\t\t\t\ttransform->CalculateWorldMatrix(transform, components)\n'
    '\t\t\t);\n'
    '\t\t\tif(transform->parent){\n'
    '\t\t\t\tif(TransformComponent* parentTransform =\n'
    '\t\t\t\t\tcomponents->GetComponent<TransformComponent>(transform->parent)){\n'
    '\t\t\t\t\tStoreRenderPacketMatrix(\n'
    '\t\t\t\t\t\tsnapshot.parentWorldMatrix,\n'
    '\t\t\t\t\t\tparentTransform->CalculateWorldMatrix(parentTransform, components)\n'
    '\t\t\t\t\t);\n'
    '\t\t\t\t\tsnapshot.hasParentWorld = true;\n'
    '\t\t\t\t}\n'
    '\t\t\t}\n',
    "packet matrix capture",
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
    "packet binding publish",
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
text = replace_once(
    text,
    '\t\t.ReadComponent<MaterialComponent>()\n',
    '\t\t.ReadComponent<MaterialComponent>()\n'
    '\t\t.ReadComponent<TextureComponent>()\n',
    "Texture read access",
)
write(RENDER_SYSTEM, text)


# ---------------------------------------------------------------------------
# Passes no longer resolve SceneContext during command submission.
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
        r'\n\s*SceneContext\* sceneContext\s*=\s*\n?\s*m_context->sceneManager->GetContextFromID\((?:packet\.|packet->)sceneContextID\);\s*\n\s*if\(!sceneContext\) (?:continue|return);\s*\n',
        '\n',
        text,
    )
    replacements = {
        'renderable->Execute(newCtx, sceneContext, packet.entity);':
            'renderable->Execute(newCtx, packet);',
        'renderable->Execute(ctx, sceneContext, packet.entity);':
            'renderable->Execute(ctx, packet);',
        'renderable->Execute(newContext, sceneContext, packet.entity);':
            'renderable->Execute(newContext, packet);',
        'renderable->Execute(ctx, sceneContext, packet->entity);':
            'renderable->Execute(ctx, *packet);',
    }
    replaced = 0
    for old, new in replacements.items():
        if old in text:
            text = text.replace(old, new)
            replaced += 1
    if replaced == 0:
        raise RuntimeError(f"{path}: packet submit call was not found")
    write(path, text)


# ---------------------------------------------------------------------------
# All concrete Renderables consume packet bindings.
# ---------------------------------------------------------------------------
renderable_root = ROOT / "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/Renderable"
headers = [path for path in renderable_root.rglob("*.h") if path.name != "IRenderable.h"]
header_count = 0
for file in headers:
    text = file.read_text(encoding="utf-8-sig")
    text, count = re.subn(
        r'void Execute\(\s*const RenderPassContext& ctx,\s*SceneContext\* sceneContext,\s*const Entity& entity\s*\) override;',
        'void Execute(\n\t\tconst RenderPassContext& ctx,\n\t\tconst RenderPacket& packet\n\t) override;',
        text,
        flags=re.S,
    )
    if count:
        header_count += count
        file.write_text(text, encoding="utf-8")
if header_count != 8:
    raise RuntimeError(f"expected 8 Renderable headers, migrated {header_count}")

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
cpp_files = list(renderable_root.rglob("*.cpp"))
cpp_count = 0
for file in cpp_files:
    text = file.read_text(encoding="utf-8-sig")
    text, count = re.subn(
        r'(void\s+\w+::Execute\(\s*const RenderPassContext& ctx,\s*)SceneContext\* sceneContext,\s*const Entity& entity\s*\)\s*\{',
        r'\1const RenderPacket& packet){\n\tSceneContext* sceneContext = packet.bindings.sceneContext;\n\tconst Entity& entity = packet.entity;\n\tif(!sceneContext) return;',
        text,
        flags=re.S,
    )
    if count == 0:
        continue
    cpp_count += count

    include_anchor = '#include "../../RenderPass/RenderPassContext.h"\n'
    if include_anchor in text and "RenderPacketTransformDX11.h" not in text:
        text = text.replace(
            include_anchor,
            include_anchor + '#include "../../RenderPacket/RenderPacketTransformDX11.h"\n',
            1,
        )

    for component, field in component_fields.items():
        text = re.sub(
            rf'sceneContext->component->GetComponent<{component}>\(entity\)',
            f'packet.bindings.{field}',
            text,
        )
        text = re.sub(
            rf'componentRegistry->GetComponent<{component}>\(entity\)',
            f'packet.bindings.{field}',
            text,
        )

    file.write_text(text, encoding="utf-8")

if cpp_count != 8:
    raise RuntimeError(f"expected 8 Renderable definitions, migrated {cpp_count}")


def patch_file(path: str, replacements: list[tuple[str, str]]) -> None:
    text = read(path)
    for index, (old, new) in enumerate(replacements):
        text = replace_once(text, old, new, f"{path} patch {index}")
    write(path, text)


patch_file(
    "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/Renderable/Model/RenderableModel.cpp",
    [
        (
            '\tDirectX::XMMATRIX world =\n'
            '\t\ttransform->CalculateWorldMatrix(\n'
            '\t\t\ttransform,\n'
            '\t\t\tsceneContext->component);',
            '\tDirectX::XMMATRIX world =\n'
            '\t\tLoadRenderPacketMatrix(packet.transform.worldMatrix);'
        ),
    ],
)

patch_file(
    "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/Renderable/Mesh/RenderableMesh.cpp",
    [
        ('\tComponentRegistry* componentRegistry = sceneContext->component;\n', ''),
        (
            '\tDirectX::XMMATRIX World = transform->CalculateWorldMatrix(transform, componentRegistry);',
            '\tDirectX::XMMATRIX World =\n'
            '\t\tLoadRenderPacketMatrix(packet.transform.worldMatrix);'
        ),
    ],
)

patch_file(
    "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/Renderable/Sprite/RenderableSprite.cpp",
    [
        ('\tComponentRegistry* componentRegistry = sceneContext->component;\n', ''),
        (
            '\tDirectX::XMMATRIX World = newTransform.CalculateWorldMatrix(&newTransform, componentRegistry);',
            '\tDirectX::XMMATRIX World = TransformMath::CalculateLocalMatrix(newTransform);\n'
            '\tif(packet.transform.hasParentWorld){\n'
            '\t\tWorld = World * LoadRenderPacketMatrix(packet.transform.parentWorldMatrix);\n'
            '\t}'
        ),
    ],
)

billboard_path = "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/Renderable/BillBoard/RenderableBillBoard.cpp"
text = read(billboard_path)
text = replace_once(text, '\tComponentRegistry* componentRegistry = sceneContext->component;\n', '', "billboard registry")
parent_pattern = re.compile(
    r'\n\tif\(transform->parent != 0\)\{.*?\n\t\}',
    flags=re.S,
)
parent_replacement = '''
	if(packet.transform.hasParentWorld){
		const DirectX::XMMATRIX parentWorld =
			LoadRenderPacketMatrix(packet.transform.parentWorldMatrix);
		DirectX::XMVECTOR parentScale, parentRotation, parentTranslation;
		DirectX::XMMatrixDecompose(
			&parentScale,
			&parentRotation,
			&parentTranslation,
			parentWorld
		);
		WorldMatrix = LocalMatrix *
			DirectX::XMMatrixTranslationFromVector(parentTranslation);
	}'''
text, count = parent_pattern.subn(parent_replacement, text, count=1)
if count != 1:
    raise RuntimeError("billboard parent transform block was not found")
write(billboard_path, text)

patch_file(
    "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/Renderable/Terrain/RenderableTerrain.cpp",
    [
        (
            '\tDirectX::XMMATRIX World = transform->CalculateWorldMatrix(transform, sceneContext->component);',
            '\tDirectX::XMMATRIX World =\n'
            '\t\tLoadRenderPacketMatrix(packet.transform.worldMatrix);'
        ),
    ],
)

wave_path = "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/Renderable/Wave/RenderableWave.cpp"
text = read(wave_path)
text = replace_once(text, '\tauto componentRegistry = sceneContext->component;\n', '', "wave registry")
text = text.replace(
    '\tauto pTransform = componentRegistry->GetComponent<TransformComponent>(entity);',
    '\tauto pTransform = packet.bindings.transform;'
)
text = text.replace(
    '\tauto pWave = componentRegistry->GetComponent<WaveComponent>(entity);',
    '\tauto pWave = packet.bindings.wave;'
)
text = replace_once(
    text,
    '\tDirectX::XMMATRIX World = transform->CalculateWorldMatrix(transform, componentRegistry);',
    '\tDirectX::XMMATRIX World =\n'
    '\t\tLoadRenderPacketMatrix(packet.transform.worldMatrix);',
    "wave world matrix",
)
write(wave_path, text)

# Correct the known UV cell-size inversion while these submitters are migrated.
for path in [
    "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/Renderable/BillBoard/RenderableBillBoard.cpp",
    "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/Renderable/Particle/RenderableParticle.cpp",
    "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/Renderable/Terrain/RenderableTerrain.cpp",
]:
    text = read(path)
    text = text.replace(
        'uv.UVEnd.x = uv.UVStart.x + 1.0f / pTexture->UV_Slice_X;',
        'uv.UVEnd.x = uv.UVStart.x + pTexture->UV_Slice_X;'
    )
    text = text.replace(
        'uv.UVEnd.y = uv.UVStart.y + 1.0f / pTexture->UV_Slice_Y;',
        'uv.UVEnd.y = uv.UVStart.y + pTexture->UV_Slice_Y;'
    )
    write(path, text)

# Hard guards: command submit and Renderables may not look up drawable components.
violations: list[str] = []
for file in cpp_files:
    text = file.read_text(encoding="utf-8")
    if "sceneContext->component->GetComponent<" in text:
        violations.append(str(file.relative_to(ROOT)))
    if "componentRegistry->GetComponent<" in text:
        violations.append(str(file.relative_to(ROOT)))
    if "CalculateWorldMatrix(" in text and file.name != "RenderableParticle.cpp":
        violations.append(str(file.relative_to(ROOT)) + " (world lookup remains)")

for path in pass_files:
    text = read(path)
    if "GetContextFromID(packet" in text:
        violations.append(path + " (SceneContext lookup remains)")

if violations:
    raise RuntimeError("Render Packet migration incomplete:\n" + "\n".join(sorted(set(violations))))

print("Render Packet direct binding migration completed.")
