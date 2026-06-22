from pathlib import Path


def load(path: str) -> str:
    return Path(path).read_text(encoding="utf-8-sig")


def save(path: str, text: str) -> None:
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


path = "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/RenderPacket/RenderPacket.h"
text = load(path)
text = replace_once(
    text,
    '''constexpr bool HasRenderPacketPass(
\tRenderPacketPassMask value,
\tRenderPacketPassMask pass
) noexcept {
\treturn (static_cast<uint8_t>(value) & static_cast<uint8_t>(pass)) != 0;
}

constexpr RenderPacketPassMask RenderPacketPassesForLayer(
''',
    '''constexpr RenderPacketPassMask operator&(
\tRenderPacketPassMask lhs,
\tRenderPacketPassMask rhs
) noexcept {
\treturn static_cast<RenderPacketPassMask>(
\t\tstatic_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs)
\t);
}

constexpr bool HasRenderPacketPass(
\tRenderPacketPassMask value,
\tRenderPacketPassMask pass
) noexcept {
\treturn (static_cast<uint8_t>(value) & static_cast<uint8_t>(pass)) != 0;
}

constexpr RenderPacketPassMask RemoveRenderPacketPass(
\tRenderPacketPassMask value,
\tRenderPacketPassMask pass
) noexcept {
\treturn static_cast<RenderPacketPassMask>(
\t\tstatic_cast<uint8_t>(value) & ~static_cast<uint8_t>(pass)
\t);
}

constexpr RenderPacketPassMask RenderPacketPassesForLayer(
''',
    "Render Packet mask operations",
)
text = replace_once(
    text,
    '''\tswitch(layer){
\t\tcase RenderLayer::Opaque3D:
\t\t\treturn RenderPacketPassMask::Shadow | RenderPacketPassMask::GBuffer;
\t\tcase RenderLayer::Background2D:
\t\tcase RenderLayer::Transparent3D:
\t\tcase RenderLayer::SortTransparent3D:
\t\t\treturn RenderPacketPassMask::Forward;
\t\tcase RenderLayer::OverlayUI:
\t\t\treturn RenderPacketPassMask::Overlay;
\t\tcase RenderLayer::Debug:
\t\t\treturn RenderPacketPassMask::Debug;
\t\tdefault:
\t\t\treturn RenderPacketPassMask::None;
\t}
}

struct RenderPacketTransformSnapshot {
''',
    '''\tswitch(layer){
\t\tcase RenderLayer::Opaque3D:
\t\tcase RenderLayer::Background2D:
\t\t\treturn RenderPacketPassMask::Shadow | RenderPacketPassMask::GBuffer;
\t\tcase RenderLayer::Transparent3D:
\t\tcase RenderLayer::SortTransparent3D:
\t\t\treturn RenderPacketPassMask::Shadow | RenderPacketPassMask::Forward;
\t\tcase RenderLayer::OverlayUI:
\t\t\treturn RenderPacketPassMask::Shadow | RenderPacketPassMask::Overlay;
\t\tcase RenderLayer::Debug:
\t\t\treturn RenderPacketPassMask::Shadow |
\t\t\t\tRenderPacketPassMask::GBuffer |
\t\t\t\tRenderPacketPassMask::Debug;
\t\tdefault:
\t\t\treturn RenderPacketPassMask::None;
\t}
}

constexpr RenderPacketPassMask RenderPacketPassesForKind(
\tRenderPacketKind kind
) noexcept {
\tswitch(kind){
\t\tcase RenderPacketKind::Model:
\t\tcase RenderPacketKind::Billboard:
\t\tcase RenderPacketKind::Terrain:
\t\t\treturn RenderPacketPassMask::Shadow |
\t\t\t\tRenderPacketPassMask::GBuffer |
\t\t\t\tRenderPacketPassMask::Forward |
\t\t\t\tRenderPacketPassMask::Debug;
\t\tcase RenderPacketKind::Wave:
\t\t\treturn RenderPacketPassMask::GBuffer |
\t\t\t\tRenderPacketPassMask::Forward |
\t\t\t\tRenderPacketPassMask::Debug;
\t\tcase RenderPacketKind::Mesh:
\t\tcase RenderPacketKind::Particle:
\t\tcase RenderPacketKind::Effect:
\t\t\treturn RenderPacketPassMask::Forward;
\t\tcase RenderPacketKind::Sprite:
\t\t\treturn RenderPacketPassMask::Forward |
\t\t\t\tRenderPacketPassMask::Overlay;
\t\tdefault:
\t\t\treturn RenderPacketPassMask::None;
\t}
}

constexpr RenderPacketPassMask ResolveRenderPacketPasses(
\tRenderLayer layer,
\tRenderPacketKind kind
) noexcept {
\treturn RenderPacketPassesForLayer(layer) & RenderPacketPassesForKind(kind);
}

struct RenderPacketTransformSnapshot {
''',
    "Render Packet compatibility pass classification",
)
save(path, text)


path = "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/renderSystem.h"
text = load(path)
text = replace_once(
    text,
    '''\tstd::shared_ptr<TextureData> GetEnvironmentMap() const;
\tID3D11SamplerState* GetEnvMapSampler() const;

\tconst RenderPacketFrameBuffer& GetRenderPacketBuffer() const noexcept {
''',
    '''\tstd::shared_ptr<TextureData> GetEnvironmentMap() const;
\tID3D11SamplerState* GetEnvMapSampler() const;
\tIRenderable* GetRenderableForPacketKind(RenderPacketKind kind);

\tconst RenderPacketFrameBuffer& GetRenderPacketBuffer() const noexcept {
''',
    "Render Packet renderable resolver declaration",
)
save(path, text)


path = "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/renderSystem.cpp"
text = load(path)
resolver = r'''
IRenderable* RenderSystem::GetRenderableForPacketKind(RenderPacketKind kind){
	switch(kind){
		case RenderPacketKind::Model:
			return GetRenderable<RenderableModel>();
		case RenderPacketKind::Mesh:
			return GetRenderable<RenderableMesh>();
		case RenderPacketKind::Sprite:
			return GetRenderable<RenderableSprite>();
		case RenderPacketKind::Billboard:
			return GetRenderable<RenderableBillBoard>();
		case RenderPacketKind::Particle:
			return GetRenderable<RenderableParticle>();
		case RenderPacketKind::Terrain:
			return GetRenderable<RenderableTerrain>();
		case RenderPacketKind::Wave:
			return GetRenderable<RenderableWave>();
		case RenderPacketKind::Effect:
			return GetRenderable<RenderableEffect>();
		default:
			return nullptr;
	}
}

'''
text = replace_once(
    text,
    "void RenderSystem::BuildRenderPackets(){\n",
    resolver + "void RenderSystem::BuildRenderPackets(){\n",
    "Render Packet renderable resolver implementation",
)
text = replace_once(
    text,
    '''\t\t\tconst MaterialComponent* materialComponent =
\t\t\t\tcomponents->GetComponent<MaterialComponent>(entity);

\t\t\tconst RenderLayer layer = layerComponent
''',
    '''\t\t\tconst MaterialComponent* materialComponent =
\t\t\t\tcomponents->GetComponent<MaterialComponent>(entity);
\t\t\tconst bool isEnvironmentMap =
\t\t\t\tcomponents->GetComponent<EnvironmentMapComponent>(entity) != nullptr;

\t\t\tconst RenderLayer layer = layerComponent
''',
    "Environment map packet classification input",
)
text = replace_once(
    text,
    '''\t\t\t\tpacket.kind = kind;
\t\t\t\tpacket.layer = layer;
\t\t\t\tpacket.passMask = RenderPacketPassesForLayer(layer);
\t\t\t\tpacket.materialKey = materialKey;
''',
    '''\t\t\t\tpacket.kind = kind;
\t\t\t\tpacket.layer = layer;
\t\t\t\tpacket.passMask = ResolveRenderPacketPasses(layer, kind);
\t\t\t\tif(isEnvironmentMap){
\t\t\t\t\tpacket.passMask = RemoveRenderPacketPass(
\t\t\t\t\t\tpacket.passMask,
\t\t\t\t\t\tRenderPacketPassMask::Shadow
\t\t\t\t\t);
\t\t\t\t}
\t\t\t\tpacket.materialKey = materialKey;
''',
    "Render Packet resolved pass mask",
)
text = replace_once(
    text,
    '''\t\t.ReadComponent<EffectComponent>()
\t\t.ReadResource<SceneManager>()
''',
    '''\t\t.ReadComponent<EffectComponent>()
\t\t.ReadComponent<EnvironmentMapComponent>()
\t\t.ReadResource<SceneManager>()
''',
    "Environment map packet access declaration",
)
save(path, text)


path = "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/RenderPass/GBuffer/GBufferPass.cpp"
text = load(path)
old = '''\t// ----- Draw -----
\tfor (int layer = 0; layer < (int)RenderLayer::MaxRenderLayer; layer++) {

\t\tif (!newCtx.renderLayerVisibility[layer]) continue;

\t\tfor (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {

\t\t\tauto sctx = scene->GetSceneContext();
\t\t\tauto entities = sctx->component->FindEntitiesWithComponent<TransformComponent>();
\t\t\tif (entities.empty()) continue;

\t\t\tfor (Entity ent : entities) {

\t\t\t\tif ((int)scene->GetRenderLayerFromEntity(ent) != layer) continue;

\t\t\t\tfor (auto r : renderables) {

\t\t\t\t\tint materialID = 0;
\t\t\t\t\tMaterialComponent* material =
\t\t\t\t\t\tsctx->component->GetComponent<MaterialComponent>(ent);
\t\t\t\t\tif(material){
\t\t\t\t\t\tmaterialID = material->ShaderID;
\t\t\t\t\t}

\t\t\t\t\tObjectInfo info;
\t\t\t\t\tinfo.SceneID = m_context->sceneManager->GetIDFromContext(sctx);
\t\t\t\t\tinfo.ObjectID = ent;
\t\t\t\t\tinfo.ShaderID = materialID;
\t\t\t\t\tm_context->graphics->SetObjectInfo(info);

\t\t\t\t\tr->Execute(newCtx, sctx, ent);
\t\t\t\t}
\t\t\t}
\t\t}
\t}
'''
new = '''\t// ----- Draw from completed Render Packets -----
\tconst RenderPacketFrameBuffer& packetBuffer =
\t\tm_renderSystem->GetRenderPacketBuffer();
\tif(packetBuffer.IsReady()){
\t\tfor(const RenderPacket& packet : packetBuffer.Packets()){
\t\t\tif(!HasRenderPacketPass(packet.passMask, RenderPacketPassMask::GBuffer)){
\t\t\t\tcontinue;
\t\t\t}

\t\t\tconst int layerIndex = static_cast<int>(packet.layer);
\t\t\tif(static_cast<unsigned>(layerIndex) >=
\t\t\t\tstatic_cast<unsigned>(RenderLayer::MaxRenderLayer)){
\t\t\t\tcontinue;
\t\t\t}
\t\t\tif(!newCtx.renderLayerVisibility[layerIndex]) continue;

\t\t\tSceneContext* sceneContext =
\t\t\t\tm_context->sceneManager->GetContextFromID(packet.sceneContextID);
\t\t\tif(!sceneContext) continue;

\t\t\tIRenderable* renderable =
\t\t\t\tm_renderSystem->GetRenderableForPacketKind(packet.kind);
\t\t\tif(!renderable) continue;

\t\t\tObjectInfo info;
\t\t\tinfo.SceneID = packet.sceneContextID;
\t\t\tinfo.ObjectID = packet.entity;
\t\t\tinfo.ShaderID = static_cast<int>(packet.materialKey);
\t\t\tm_context->graphics->SetObjectInfo(info);

\t\t\trenderable->Execute(newCtx, sceneContext, packet.entity);
\t\t}
\t}
'''
text = replace_once(text, old, new, "GBuffer Render Packet connection")
save(path, text)


path = "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/RenderPass/ShadowMap/ShadowMapPass.cpp"
text = load(path)
old = '''\t\tfor(const auto& [name, scene] : activeScenes){

\t\t\tauto sctx = scene->GetSceneContext();
\t\t\tauto& component = *sctx->component;

\t\t\tconst auto entities = component.FindEntitiesWithComponent<TransformComponent>();
\t\t\tif(entities.empty()){
\t\t\t\tcontinue;
\t\t\t}

\t\t\tfor(Entity ent : entities){

\t\t\t\tif(component.GetComponent<EnvironmentMapComponent>(ent)){
\t\t\t\t\tcontinue;
\t\t\t\t}

\t\t\t\tRenderLayer layer = scene->GetRenderLayerFromEntity(ent);
\t\t\t\tconst int layerIndex = (int)layer;
\t\t\t\tif((unsigned)layerIndex >= (unsigned)maxLayer){
\t\t\t\t\tcontinue;
\t\t\t\t}

\t\t\t\tif(!newContext.renderLayerVisibility[layerIndex]){
\t\t\t\t\tcontinue;
\t\t\t\t}

\t\t\t\tfor(auto renderable : renderables){
\t\t\t\t\trenderable->Execute(newContext, sctx, ent);
\t\t\t\t}
\t\t\t}
\t\t}
'''
new = '''\t\tconst RenderPacketFrameBuffer& packetBuffer =
\t\t\tm_renderSystem->GetRenderPacketBuffer();
\t\tif(packetBuffer.IsReady()){
\t\t\tfor(const RenderPacket& packet : packetBuffer.Packets()){
\t\t\t\tif(!HasRenderPacketPass(packet.passMask, RenderPacketPassMask::Shadow)){
\t\t\t\t\tcontinue;
\t\t\t\t}

\t\t\t\tconst int layerIndex = static_cast<int>(packet.layer);
\t\t\t\tif(static_cast<unsigned>(layerIndex) >=
\t\t\t\t\tstatic_cast<unsigned>(maxLayer)){
\t\t\t\t\tcontinue;
\t\t\t\t}
\t\t\t\tif(!newContext.renderLayerVisibility[layerIndex]) continue;

\t\t\t\tSceneContext* sceneContext =
\t\t\t\t\tm_context->sceneManager->GetContextFromID(packet.sceneContextID);
\t\t\t\tif(!sceneContext) continue;

\t\t\t\tIRenderable* renderable =
\t\t\t\t\tm_renderSystem->GetRenderableForPacketKind(packet.kind);
\t\t\t\tif(!renderable) continue;

\t\t\t\trenderable->Execute(newContext, sceneContext, packet.entity);
\t\t\t}
\t\t}
'''
text = replace_once(text, old, new, "Shadow Render Packet connection")
save(path, text)


path = "Tests/RenderPacketBufferSmokeTest.cpp"
text = load(path)
text = replace_once(
    text,
    '''\tstatic_assert(HasRenderPacketPass(
\t\tRenderPacketPassesForLayer(RenderLayer::Opaque3D),
\t\tRenderPacketPassMask::GBuffer
\t));

\tRenderPacketWorkerBuffer worker0(0);
''',
    '''\tstatic_assert(HasRenderPacketPass(
\t\tRenderPacketPassesForLayer(RenderLayer::Opaque3D),
\t\tRenderPacketPassMask::GBuffer
\t));
\tstatic_assert(HasRenderPacketPass(
\t\tResolveRenderPacketPasses(
\t\t\tRenderLayer::Transparent3D,
\t\t\tRenderPacketKind::Model
\t\t),
\t\tRenderPacketPassMask::Shadow
\t));
\tstatic_assert(!HasRenderPacketPass(
\t\tResolveRenderPacketPasses(
\t\t\tRenderLayer::Opaque3D,
\t\t\tRenderPacketKind::Mesh
\t\t),
\t\tRenderPacketPassMask::GBuffer
\t));
\tstatic_assert(HasRenderPacketPass(
\t\tResolveRenderPacketPasses(
\t\t\tRenderLayer::Debug,
\t\t\tRenderPacketKind::Model
\t\t),
\t\tRenderPacketPassMask::GBuffer
\t));
\tstatic_assert(!HasRenderPacketPass(
\t\tRemoveRenderPacketPass(
\t\t\tRenderPacketPassMask::Shadow | RenderPacketPassMask::GBuffer,
\t\t\tRenderPacketPassMask::Shadow
\t\t),
\t\tRenderPacketPassMask::Shadow
\t));

\tRenderPacketWorkerBuffer worker0(0);
''',
    "Render Packet pass compatibility tests",
)
save(path, text)


path = "Docs/Step17_Task_Naming_And_Render_Task_Decomposition_Plan.md"
text = load(path)
text = replace_once(
    text,
    "- [ ] Model / Mesh / Sprite / Particle / Terrain / Wave Packet Builder\n",
    "- [x] Model / Mesh / Sprite / Particle / Terrain / Wave Packet Builder\n",
    "Render Packet builder checklist",
)
text = replace_once(
    text,
    "- `Docs/Step17B_Render_Packet_Foundation.md`\n",
    "- `Docs/Step17B_Render_Packet_Foundation.md`\n"
    "- `Docs/Step17B_Render_Packet_Opaque_Pass_Connection.md`\n",
    "Opaque pass connection document link",
)
save(path, text)


path = "Docs/Step17B_Render_Packet_Foundation.md"
text = load(path)
text += '''

## Opaque Pass接続

GBufferとShadowのジオメトリ提出は、Scene / Transform Entityの再走査から完成済みRender Packet列の走査へ移行した。Light収集、RenderTarget設定、Camera設定、実GPU Drawは引き続きMainThread Pass側が担当する。

既存挙動維持のため、LayerとPacket Kindの両方からPass Maskを解決する。Environment Map EntityはShadow Maskを除外する。
'''
save(path, text)


doc = '''# Step 17-B: Render Packet Opaque Pass Connection

## 完了範囲

- GBuffer geometry submissionをRender Packet入力へ変更
- Shadow caster submissionをRender Packet入力へ変更
- Scene Context IDから非所有SceneContextを解決
- PacketのMaterial KeyからObjectInfoを設定
- Packet Kindから既存IRenderableを解決
- Environment Map EntityをShadow対象外としてPacket Build時に分類

## 互換Pass分類

既存描画結果を変えないため、LayerだけでなくRenderable Kindも含めてPass Maskを決定する。

- Model / Billboard / Terrain: Shadow, GBuffer, Forward
- Wave: GBuffer, Forward
- Mesh / Particle / Effect: Forward
- Sprite: Forward, Overlay
- Debug LayerのModel系: GBufferとShadowを維持
- Transparent LayerのModel系: Forwardに加えてShadow casterを維持

## 未移行

- Forward透明距離Sort
- Overlay UI order sort
- Renderable内部のComponentRegistry参照
- Transform Snapshotの直接利用

次工程はForward / OverlayをPacket入力へ接続し、View依存SortをPacket Viewへ分離する。
'''
save("Docs/Step17B_Render_Packet_Opaque_Pass_Connection.md", doc)

print("Render Packet GBuffer and Shadow connection migration applied")
