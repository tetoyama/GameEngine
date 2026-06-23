from pathlib import Path


def load(path: str) -> str:
    return Path(path).read_text(encoding="utf-8-sig")


def save(path: str, text: str) -> None:
    Path(path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


packet_path = "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/RenderPacket/RenderPacket.h"
text = load(packet_path)
text = replace_once(
    text,
    '#include "System/Render/RenderSystem/renderLayer.h"\n',
    '#include "System/Render/RenderSystem/renderLayer.h"\n\n'
    'struct MaterialComponent;\n'
    'struct TextureComponent;\n'
    'struct ModelRendererComponent;\n'
    'struct MeshRendererComponent;\n'
    'struct SpriteRendererComponent;\n'
    'struct BillBoardRendererComponent;\n'
    'struct ParticleComponent;\n'
    'struct TerrainComponent;\n'
    'struct WaveComponent;\n'
    'struct EffectComponent;\n',
    "packet forward declarations",
)
text = replace_once(
    text,
    """struct RenderPacketTransformSnapshot {
\tfloat position[3] = {0.0f, 0.0f, 0.0f};
\tfloat worldPosition[3] = {0.0f, 0.0f, 0.0f};
\tfloat rotation[4] = {0.0f, 0.0f, 0.0f, 1.0f};
\tfloat scale[3] = {1.0f, 1.0f, 1.0f};
};
""",
    """struct RenderPacketTransformSnapshot {
\tfloat position[3] = {0.0f, 0.0f, 0.0f};
\tfloat worldPosition[3] = {0.0f, 0.0f, 0.0f};
\tfloat rotation[4] = {0.0f, 0.0f, 0.0f, 1.0f};
\tfloat scale[3] = {1.0f, 1.0f, 1.0f};
\tfloat worldMatrix[16] = {
\t\t1.0f, 0.0f, 0.0f, 0.0f,
\t\t0.0f, 1.0f, 0.0f, 0.0f,
\t\t0.0f, 0.0f, 1.0f, 0.0f,
\t\t0.0f, 0.0f, 0.0f, 1.0f
\t};
};

// BuildからSubmitまでのみ有効な非所有Component参照。
// Render Schedule中のStructural Mutationは禁止されているため、Packet世代内では安定する。
struct RenderPacketComponentBindings {
\tconst MaterialComponent* material = nullptr;
\tconst TextureComponent* texture = nullptr;
\tconst ModelRendererComponent* model = nullptr;
\tconst MeshRendererComponent* mesh = nullptr;
\tconst SpriteRendererComponent* sprite = nullptr;
\tconst BillBoardRendererComponent* billboard = nullptr;
\tconst ParticleComponent* particle = nullptr;
\tconst TerrainComponent* terrain = nullptr;
\tconst WaveComponent* wave = nullptr;
\tconst EffectComponent* effect = nullptr;
};
""",
    "packet transform and bindings",
)
text = replace_once(
    text,
    "\tRenderPacketTransformSnapshot transform;\n};",
    "\tRenderPacketTransformSnapshot transform;\n\tRenderPacketComponentBindings components;\n};",
    "packet binding member",
)
save(packet_path, text)


interface_path = "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/Renderable/IRenderable.h"
text = load(interface_path)
text = replace_once(
    text,
    '#include "Scene/Entity/Entity.h"\n',
    '#include "Scene/Entity/Entity.h"\n'
    '#include "System/Render/RenderSystem/RenderPacket/RenderPacket.h"\n',
    "IRenderable packet include",
)
text = replace_once(
    text,
    """\tvirtual void Execute(
\t\tconst RenderPassContext& ctx,
\t\tSceneContext* sceneContext,
\t\tconst Entity& entity
\t) = 0;
""",
    """\tvirtual void Execute(
\t\tconst RenderPassContext& ctx,
\t\tSceneContext* sceneContext,
\t\tconst Entity& entity
\t) = 0;

\t// Packet移行中の互換入口。未移行Renderableは従来Executeへフォールバックする。
\tvirtual void ExecutePacket(
\t\tconst RenderPassContext& ctx,
\t\tSceneContext* sceneContext,
\t\tconst RenderPacket& packet
\t){
\t\tExecute(ctx, sceneContext, packet.entity);
\t}
""",
    "IRenderable packet entry",
)
save(interface_path, text)


model_header_path = "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/Renderable/Model/RenderableModel.h"
text = load(model_header_path)
text = replace_once(
    text,
    """\tvoid Execute(
\t\tconst RenderPassContext& ctx,
\t\tSceneContext* sceneContext,
\t\tconst Entity& entity
\t) override;
""",
    """\tvoid Execute(
\t\tconst RenderPassContext& ctx,
\t\tSceneContext* sceneContext,
\t\tconst Entity& entity
\t) override;

\tvoid ExecutePacket(
\t\tconst RenderPassContext& ctx,
\t\tSceneContext* sceneContext,
\t\tconst RenderPacket& packet
\t) override;
""",
    "model packet override",
)
save(model_header_path, text)


model_cpp_path = "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/Renderable/Model/RenderableModel.cpp"
text = load(model_cpp_path)
text = replace_once(
    text,
    '#include <d3d11.h>\n',
    '#include <d3d11.h>\n#include <cstring>\n',
    "model cstring include",
)
old_top = """void RenderableModel::Execute(
\tconst RenderPassContext& ctx,
\tSceneContext* sceneContext,
\tconst Entity& entity){
\tModelRendererComponent* modelRenderer =
\t\tsceneContext->component->GetComponent<ModelRendererComponent>(entity);

\tTransformComponent* transform =
\t\tsceneContext->component->GetComponent<TransformComponent>(entity);

\tif(!modelRenderer || !transform){
\t\treturn;
\t}

\tModelData* model = modelRenderer->model.get();
"""
new_top = """void RenderableModel::Execute(
\tconst RenderPassContext& ctx,
\tSceneContext* sceneContext,
\tconst Entity& entity){
\tif(!sceneContext || !sceneContext->component){
\t\treturn;
\t}

\tconst ModelRendererComponent* modelRenderer =
\t\tsceneContext->component->GetComponent<ModelRendererComponent>(entity);
\tconst TransformComponent* transform =
\t\tsceneContext->component->GetComponent<TransformComponent>(entity);
\tif(!modelRenderer || !transform){
\t\treturn;
\t}

\tRenderPacket packet;
\tpacket.entity = entity;
\tpacket.kind = RenderPacketKind::Model;
\tpacket.components.model = modelRenderer;
\tpacket.components.material =
\t\tsceneContext->component->GetComponent<MaterialComponent>(entity);
\tpacket.components.texture =
\t\tsceneContext->component->GetComponent<TextureComponent>(entity);

\tconst DirectX::XMMATRIX world =
\t\ttransform->CalculateWorldMatrix(transform, sceneContext->component);
\tDirectX::XMFLOAT4X4 worldData{};
\tDirectX::XMStoreFloat4x4(&worldData, world);
\tstd::memcpy(packet.transform.worldMatrix, &worldData, sizeof(worldData));

\tExecutePacket(ctx, sceneContext, packet);
}

void RenderableModel::ExecutePacket(
\tconst RenderPassContext& ctx,
\tSceneContext* sceneContext,
\tconst RenderPacket& packet){
\tconst ModelRendererComponent* modelRenderer = packet.components.model;
\tif(!sceneContext || !modelRenderer){
\t\treturn;
\t}

\tModelData* model = modelRenderer->model.get();
"""
text = replace_once(text, old_top, new_top, "model execute packet conversion")
text = replace_once(
    text,
    """\tTextureComponent* textureComponent =
\t\tsceneContext->component->GetComponent<TextureComponent>(entity);

\tMaterialComponent* materialComponent =
\t\tsceneContext->component->GetComponent<MaterialComponent>(entity);
""",
    """\tconst TextureComponent* textureComponent = packet.components.texture;
\tconst MaterialComponent* materialComponent = packet.components.material;
""",
    "model bound optional components",
)
text = replace_once(
    text,
    """\tDirectX::XMMATRIX world =
\t\ttransform->CalculateWorldMatrix(
\t\t\ttransform,
\t\t\tsceneContext->component);
\tgraphicsContext->SetWorldMatrix(world);
""",
    """\tDirectX::XMFLOAT4X4 worldData{};
\tstd::memcpy(&worldData, packet.transform.worldMatrix, sizeof(worldData));
\tgraphicsContext->SetWorldMatrix(DirectX::XMLoadFloat4x4(&worldData));
""",
    "model world snapshot",
)
save(model_cpp_path, text)


render_system_path = "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/renderSystem.cpp"
text = load(render_system_path)
text = replace_once(
    text,
    """\t\t\tRenderPacketTransformSnapshot snapshot;
\t\t\tsnapshot.position[0] = transform->position.x;
""",
    """\t\t\tRenderPacketTransformSnapshot snapshot;
\t\t\tsnapshot.position[0] = transform->position.x;
""",
    "snapshot anchor",
)
text = replace_once(
    text,
    """\t\t\tsnapshot.scale[0] = transform->scale.x;
\t\t\tsnapshot.scale[1] = transform->scale.y;
\t\t\tsnapshot.scale[2] = transform->scale.z;

\t\t\tauto appendPacket = [&](RenderPacketKind kind){
""",
    """\t\t\tsnapshot.scale[0] = transform->scale.x;
\t\t\tsnapshot.scale[1] = transform->scale.y;
\t\t\tsnapshot.scale[2] = transform->scale.z;

\t\t\tconst DirectX::XMMATRIX worldMatrix =
\t\t\t\ttransform->CalculateWorldMatrix(transform, components);
\t\t\tDirectX::XMFLOAT4X4 worldMatrixData{};
\t\t\tDirectX::XMStoreFloat4x4(&worldMatrixData, worldMatrix);
\t\t\tstatic_assert(sizeof(snapshot.worldMatrix) == sizeof(worldMatrixData));
\t\t\tstd::memcpy(snapshot.worldMatrix, &worldMatrixData, sizeof(worldMatrixData));

\t\t\tRenderPacketComponentBindings bindings;
\t\t\tbindings.material = materialComponent;
\t\t\tbindings.texture = components->GetComponent<TextureComponent>(entity);
\t\t\tbindings.model = components->GetComponent<ModelRendererComponent>(entity);
\t\t\tbindings.mesh = components->GetComponent<MeshRendererComponent>(entity);
\t\t\tbindings.sprite = components->GetComponent<SpriteRendererComponent>(entity);
\t\t\tbindings.billboard = components->GetComponent<BillBoardRendererComponent>(entity);
\t\t\tbindings.particle = components->GetComponent<ParticleComponent>(entity);
\t\t\tbindings.terrain = components->GetComponent<TerrainComponent>(entity);
\t\t\tbindings.wave = components->GetComponent<WaveComponent>(entity);
\t\t\tbindings.effect = components->GetComponent<EffectComponent>(entity);

\t\t\tauto appendPacket = [&](RenderPacketKind kind){
""",
    "world matrix and component bindings",
)
text = replace_once(
    text,
    "\t\t\t\tpacket.transform = snapshot;\n\t\t\t\tworker.Add(std::move(packet));",
    "\t\t\t\tpacket.transform = snapshot;\n\t\t\t\tpacket.components = bindings;\n\t\t\t\tworker.Add(std::move(packet));",
    "assign packet bindings",
)
for component, binding, kind in [
    ("ModelRendererComponent", "model", "Model"),
    ("MeshRendererComponent", "mesh", "Mesh"),
    ("SpriteRendererComponent", "sprite", "Sprite"),
    ("BillBoardRendererComponent", "billboard", "Billboard"),
    ("ParticleComponent", "particle", "Particle"),
    ("TerrainComponent", "terrain", "Terrain"),
    ("WaveComponent", "wave", "Wave"),
    ("EffectComponent", "effect", "Effect"),
]:
    text = text.replace(
        f"if(components->GetComponent<{component}>(entity)){{\n\t\t\t\tappendPacket(RenderPacketKind::{kind});",
        f"if(bindings.{binding}){{\n\t\t\t\tappendPacket(RenderPacketKind::{kind});",
        1,
    )
if '#include <cstring>' not in text:
    text = text.replace('#include <array>\n', '#include <array>\n#include <cstring>\n', 1)
save(render_system_path, text)


pass_paths = [
    "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/RenderPass/GBuffer/GBufferPass.cpp",
    "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/RenderPass/Forward/ForwardPass.cpp",
    "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/RenderPass/ShadowMap/ShadowMapPass.cpp",
    "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/RenderPass/OverlayUI/OverlayUIPass.cpp",
]
for path in pass_paths:
    text = load(path)
    replacements = [
        ("renderable->Execute(newCtx, sceneContext, packet.entity);", "renderable->ExecutePacket(newCtx, sceneContext, packet);"),
        ("renderable->Execute(ctx, sceneContext, packet.entity);", "renderable->ExecutePacket(ctx, sceneContext, packet);"),
        ("renderable->Execute(newCtx, sceneContext, packet->entity);", "renderable->ExecutePacket(newCtx, sceneContext, *packet);"),
        ("renderable->Execute(ctx, sceneContext, packet->entity);", "renderable->ExecutePacket(ctx, sceneContext, *packet);"),
    ]
    changed = False
    for old, new in replacements:
        if old in text:
            text = text.replace(old, new)
            changed = True
    if not changed:
        raise RuntimeError(f"no Execute call converted in {path}")
    save(path, text)
