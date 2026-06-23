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


# -----------------------------------------------------------------------------
# RenderPacket routing
# -----------------------------------------------------------------------------
packet_path = "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/RenderPacket/RenderPacket.h"
text = load(packet_path)
anchor = """constexpr RenderPacketPassMask ResolveRenderPacketPasses(
\tRenderLayer layer,
\tRenderPacketKind kind
) noexcept {
\treturn RenderPacketPassesForLayer(layer) & RenderPacketPassesForKind(kind);
}
"""
replacement = """constexpr RenderLayer ResolveRenderPacketLayer(
\tRenderLayer requestedLayer,
\tRenderPacketKind kind
) noexcept {
\tswitch(kind){
\t\tcase RenderPacketKind::Sprite:
\t\t\t// Spriteは2D描画経路へ提出する。既定Opaqueのままでは
\t\t\t// Forward/Overlayのどちらにも入らないためOverlayへ正規化する。
\t\t\tif(requestedLayer == RenderLayer::Opaque3D ||
\t\t\t   requestedLayer == RenderLayer::Background2D){
\t\t\t\treturn RenderLayer::OverlayUI;
\t\t\t}
\t\t\treturn requestedLayer;

\t\tcase RenderPacketKind::Mesh:
\t\tcase RenderPacketKind::Particle:
\t\tcase RenderPacketKind::Effect:
\t\t\t// Forward専用Renderableは既定OpaqueのままだとPacketが消える。
\t\t\t// 明示的なTransparent指定は保持し、それ以外をTransparentへ正規化する。
\t\t\tif(requestedLayer != RenderLayer::Transparent3D &&
\t\t\t   requestedLayer != RenderLayer::SortTransparent3D){
\t\t\t\treturn RenderLayer::Transparent3D;
\t\t\t}
\t\t\treturn requestedLayer;

\t\tdefault:
\t\t\treturn requestedLayer;
\t}
}

constexpr RenderPacketPassMask ResolveRenderPacketPasses(
\tRenderLayer layer,
\tRenderPacketKind kind
) noexcept {
\tconst RenderLayer effectiveLayer = ResolveRenderPacketLayer(layer, kind);
\treturn RenderPacketPassesForLayer(effectiveLayer) & RenderPacketPassesForKind(kind);
}
"""
text = replace_once(text, anchor, replacement, "render packet effective layer")
save(packet_path, text)


render_system_path = "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/renderSystem.cpp"
text = load(render_system_path)
old = """\t\t\tauto appendPacket = [&](RenderPacketKind kind){
\t\t\t\tRenderPacket packet;
\t\t\t\tpacket.sceneContextID = sceneEntry.contextID;
\t\t\t\tpacket.entity = entity;
\t\t\t\tpacket.kind = kind;
\t\t\t\tpacket.layer = layer;
\t\t\t\tpacket.passMask = ResolveRenderPacketPasses(layer, kind);
"""
new = """\t\t\tauto appendPacket = [&](RenderPacketKind kind){
\t\t\t\tRenderPacket packet;
\t\t\t\tpacket.sceneContextID = sceneEntry.contextID;
\t\t\t\tpacket.entity = entity;
\t\t\t\tpacket.kind = kind;
\t\t\t\tconst RenderLayer effectiveLayer = ResolveRenderPacketLayer(layer, kind);
\t\t\t\tpacket.layer = effectiveLayer;
\t\t\t\tpacket.passMask = ResolveRenderPacketPasses(effectiveLayer, kind);
"""
text = replace_once(text, old, new, "packet effective layer assignment")
text = replace_once(
    text,
    """\t\t\t\tpacket.sortKey = MakeRenderPacketSortKey(
\t\t\t\t\tlayer,
\t\t\t\t\tkind,
""",
    """\t\t\t\tpacket.sortKey = MakeRenderPacketSortKey(
\t\t\t\t\teffectiveLayer,
\t\t\t\t\tkind,
""",
    "packet effective sort layer",
)
save(render_system_path, text)


# -----------------------------------------------------------------------------
# Sprite draw path: use the actual pass resolution and deterministic UV defaults.
# -----------------------------------------------------------------------------
sprite_path = "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/Renderable/Sprite/RenderableSprite.cpp"
text = load(sprite_path)
text = replace_once(
    text,
    """\tVector2 viewportSize = Vector2(
\t\t(float)sceneContext->manager->graphics->m_width,
\t\t(float)sceneContext->manager->graphics->m_height
\t);
""",
    """\tconst Vector2 viewportSize = ctx.screenSize;
""",
    "sprite pass viewport",
)
text = text.replace("\t\tUVMatrixBuffer uv;", "\t\tUVMatrixBuffer uv{};")
text = text.replace("\t\t\tUVMatrixBuffer uv;", "\t\t\tUVMatrixBuffer uv{};")
text = replace_once(
    text,
    """\t\tif (pTexture->UV_Slice_X > 0.0f && pTexture->UV_Slice_Y > 0.0f) {

\t\t\tint column = (int)(pTexture->UV_Slice_X);
\t\t\tif(column <= 0){
\t\t\t\tcolumn = 1;
\t\t\t}

\t\t\tuv.UVStart.x = (pTexture->AnimationNum % column) * (1.0f / pTexture->UV_Slice_X);
\t\t\tuv.UVStart.y = (pTexture->AnimationNum / column) * (1.0f / pTexture->UV_Slice_Y);

\t\t\t// 1 セルの UV サイズ: 1/スライス数
\t\t\tuv.UVEnd.x = uv.UVStart.x + 1.0f / pTexture->UV_Slice_X;  // セルの右端 UV
\t\t\tuv.UVEnd.y = uv.UVStart.y + 1.0f / pTexture->UV_Slice_Y;  // セルの下端 UV
\t\t}
""",
    """\t\tuv.UVStart = float2(0.0f, 0.0f);
\t\tuv.UVEnd = float2(1.0f, 1.0f);
\t\tif(pTexture->UV_Slice_X > 0.0f && pTexture->UV_Slice_Y > 0.0f){
\t\t\tconst int columnCount = (std::max)(1, static_cast<int>(1.0f / pTexture->UV_Slice_X));
\t\t\tuv.UVStart.x = (pTexture->AnimationNum % columnCount) * pTexture->UV_Slice_X;
\t\t\tuv.UVStart.y = (pTexture->AnimationNum / columnCount) * pTexture->UV_Slice_Y;
\t\t\tuv.UVEnd.x = uv.UVStart.x + pTexture->UV_Slice_X;
\t\t\tuv.UVEnd.y = uv.UVStart.y + pTexture->UV_Slice_Y;
\t\t}
""",
    "sprite uv calculation",
)
if "#include <algorithm>" not in text:
    text = text.replace("#include <d3d11.h>\n", "#include <d3d11.h>\n#include <algorithm>\n", 1)
save(sprite_path, text)


# -----------------------------------------------------------------------------
# Particle draw path: explicitly restore its own IA/shader state.
# -----------------------------------------------------------------------------
particle_path = "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/Renderable/Particle/RenderableParticle.cpp"
text = load(particle_path)
text = replace_once(
    text,
    """\tgraphicsContext->SetBlendMode(BlendMode::Additive);
\t//graphicsContext->SetDepthMode(DepthMode::Disable);

\tfor(int i = 0; i < MAXPARTICLE; i++){
""",
    """\tgraphicsContext->SetBlendMode(BlendMode::Additive);
\tgraphicsContext->SetDepthMode(DepthMode::ReadOnly);

\tdeviceContext->IASetInputLayout(m_billBoardMesh->mesh.m_VertexLayout.Get());
\tdeviceContext->VSSetShader(m_billBoardMesh->mesh.m_VertexShader.Get(), nullptr, 0);
\tdeviceContext->PSSetShader(m_billBoardMesh->mesh.m_PixelShader.Get(), nullptr, 0);

\tfor(int i = 0; i < MAXPARTICLE; i++){
""",
    "particle shader state",
)
text = text.replace("\t\t\t\tUVMatrixBuffer uv;", "\t\t\t\tUVMatrixBuffer uv{};")
text = text.replace("\t\t\tUVMatrixBuffer uv;", "\t\t\tUVMatrixBuffer uv{};")
text = replace_once(
    text,
    """\tgraphicsContext->SetBlendMode(BlendMode::Alpha);
\t//graphicsContext->SetDepthMode(DepthMode::Write);
""",
    """\tgraphicsContext->SetBlendMode(BlendMode::Alpha);
\tgraphicsContext->SetDepthMode(DepthMode::Write);
""",
    "particle state restore",
)
save(particle_path, text)


# -----------------------------------------------------------------------------
# Sprite gizmo: use the same RectTransform -> World matrix path as rendering.
# Do not apply 3D parent inverse a second time for the 2D gizmo result.
# -----------------------------------------------------------------------------
view_path = "Source/GameApplication/Engine/Editor/UI/ViewWindow.cpp"
text = load(view_path)
old = """\t\t\t\tauto* sprite = registry->GetComponent<SpriteRendererComponent>(selectedEntity);
\t\t\t\tif(sprite){
\t\t\t\t\tTransformComponent temp = transform->CalculateRectTransform(Vector2(avail.x, avail.y), *sprite, *transform);
\t\t\t\t\tDirectX::XMVECTOR scaling = DirectX::XMVectorSet(temp.scale.x, temp.scale.y, 1.0f, 0.0f);
\t\t\t\t\tDirectX::XMVECTOR rotationOrigin = DirectX::XMVectorSet(sprite->pivot.x, sprite->pivot.y, 0.0f, 0.0f);
\t\t\t\t\tfloat rotationZ = temp.GetRotationEuler().z;
\t\t\t\t\tDirectX::XMVECTOR translation = DirectX::XMVectorSet(temp.position.x, temp.position.y, temp.position.z, 0.0f);

\t\t\t\t\tDirectX::XMMATRIX model2D = DirectX::XMMatrixAffineTransformation2D(scaling, rotationOrigin, rotationZ, translation);
\t\t\t\t\tWorld = model2D;
\t\t\t\t\tmodelMatrix = m_editor->sceneManager->GetContext()->imgui->RenderGizmo2D(World, DirectX::XMFLOAT2(avail.x, avail.y));
\t\t\t\t} else{
\t\t\t\t\tmodelMatrix = m_editor->sceneManager->GetContext()->imgui->RenderGizmo(World);
\t\t\t\t}

\t\t\t\t// 親の逆行列を適用してローカル座標系に戻す
\t\t\t\tEntity Parent = transform->parent;
\t\t\t\twhile(Parent != 0){
\t\t\t\t\tauto* ParentTransform = registry->GetComponent<TransformComponent>(Parent);
\t\t\t\t\tif(ParentTransform){
\t\t\t\t\t\tDirectX::XMMATRIX ParentWorld = ParentTransform->CalculateWorldMatrix(ParentTransform, registry);
\t\t\t\t\t\tmodelMatrix = modelMatrix * DirectX::XMMatrixInverse(nullptr, ParentWorld);
\t\t\t\t\t\tParent = ParentTransform->parent;
\t\t\t\t\t} else{
\t\t\t\t\t\tParent = 0;
\t\t\t\t\t}
\t\t\t\t}
"""
new = """\t\t\t\tauto* sprite = registry->GetComponent<SpriteRendererComponent>(selectedEntity);
\t\t\t\tif(sprite){
\t\t\t\t\tconst Vector2 editorViewport(avail.x, avail.y);
\t\t\t\t\tTransformComponent rectTransform =
\t\t\t\t\t\ttransform->CalculateRectTransform(editorViewport, *sprite, *transform);
\t\t\t\t\tWorld = rectTransform.CalculateWorldMatrix(&rectTransform, registry);
\t\t\t\t\tmodelMatrix = m_editor->sceneManager->GetContext()->imgui->RenderGizmo2D(
\t\t\t\t\t\tWorld,
\t\t\t\t\t\tDirectX::XMFLOAT2(editorViewport.x, editorViewport.y)
\t\t\t\t\t);
\t\t\t\t} else{
\t\t\t\t\tmodelMatrix = m_editor->sceneManager->GetContext()->imgui->RenderGizmo(World);

\t\t\t\t\t// 3D Transformのみ親Worldを除去してローカルへ戻す。
\t\t\t\t\tEntity Parent = transform->parent;
\t\t\t\t\twhile(Parent != 0){
\t\t\t\t\t\tauto* ParentTransform = registry->GetComponent<TransformComponent>(Parent);
\t\t\t\t\t\tif(ParentTransform){
\t\t\t\t\t\t\tDirectX::XMMATRIX ParentWorld = ParentTransform->CalculateWorldMatrix(ParentTransform, registry);
\t\t\t\t\t\t\tmodelMatrix = modelMatrix * DirectX::XMMatrixInverse(nullptr, ParentWorld);
\t\t\t\t\t\t\tParent = ParentTransform->parent;
\t\t\t\t\t\t} else{
\t\t\t\t\t\t\tParent = 0;
\t\t\t\t\t\t}
\t\t\t\t\t}
\t\t\t\t}
"""
text = replace_once(text, old, new, "sprite gizmo world path")
save(view_path, text)


# -----------------------------------------------------------------------------
# Documentation
# -----------------------------------------------------------------------------
doc_path = "Docs/Step17B_Renderable_Component_Binding_Phase1.md"
text = load(doc_path)
text += """

## 緊急回帰修正

- Sprite / Particle / Effectの既定Opaque Layerを有効な提出Layerへ正規化
- Sprite描画のViewportをRenderPassContextと統一
- ParticleがForwardPassのShader Stateを引き継がないよう明示Bind
- Spriteギズモを描画と同一のRectTransform / World Matrix経路へ統一
- Spriteギズモに3D親逆行列を二重適用しない
"""
save(doc_path, text)
