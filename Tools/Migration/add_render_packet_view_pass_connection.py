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
    '''struct RenderPacketTransformSnapshot {
\tfloat position[3] = {0.0f, 0.0f, 0.0f};
\tfloat rotation[4] = {0.0f, 0.0f, 0.0f, 1.0f};
\tfloat scale[3] = {1.0f, 1.0f, 1.0f};
};
''',
    '''struct RenderPacketTransformSnapshot {
\tfloat position[3] = {0.0f, 0.0f, 0.0f};
\tfloat worldPosition[3] = {0.0f, 0.0f, 0.0f};
\tfloat rotation[4] = {0.0f, 0.0f, 0.0f, 1.0f};
\tfloat scale[3] = {1.0f, 1.0f, 1.0f};
};
''',
    "Render Packet world position snapshot",
)
text += '''

struct RenderPacketViewItem {
\tconst RenderPacket* packet = nullptr;
\tfloat distanceSq = 0.0f;
};

inline bool RenderPacketBackToFront(
\tconst RenderPacketViewItem& lhs,
\tconst RenderPacketViewItem& rhs
) noexcept {
\tif(lhs.distanceSq != rhs.distanceSq) return lhs.distanceSq > rhs.distanceSq;
\tif(!lhs.packet) return false;
\tif(!rhs.packet) return true;
\treturn RenderPacketLess(*lhs.packet, *rhs.packet);
}

inline bool RenderPacketOverlayOrder(
\tconst RenderPacket* lhs,
\tconst RenderPacket* rhs
) noexcept {
\tif(!lhs) return false;
\tif(!rhs) return true;
\tif(lhs->orderInLayer != rhs->orderInLayer){
\t\treturn lhs->orderInLayer > rhs->orderInLayer;
\t}
\treturn RenderPacketLess(*lhs, *rhs);
}
'''
save(path, text)


path = "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/renderSystem.cpp"
text = load(path)
text = replace_once(
    text,
    '''\t\t\tRenderPacketTransformSnapshot snapshot;
\t\t\tsnapshot.position[0] = transform->position.x;
\t\t\tsnapshot.position[1] = transform->position.y;
\t\t\tsnapshot.position[2] = transform->position.z;
\t\t\tconst DirectX::XMFLOAT4& rotation = transform->GetRotation();
''',
    '''\t\t\tRenderPacketTransformSnapshot snapshot;
\t\t\tsnapshot.position[0] = transform->position.x;
\t\t\tsnapshot.position[1] = transform->position.y;
\t\t\tsnapshot.position[2] = transform->position.z;
\t\t\tconst Vector3 worldPosition = transform->GetWorldPosition(components);
\t\t\tsnapshot.worldPosition[0] = worldPosition.x;
\t\t\tsnapshot.worldPosition[1] = worldPosition.y;
\t\t\tsnapshot.worldPosition[2] = worldPosition.z;
\t\t\tconst DirectX::XMFLOAT4& rotation = transform->GetRotation();
''',
    "Render Packet world position build",
)
save(path, text)


path = "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/RenderPass/Forward/ForwardPass.cpp"
text = load(path)
old = '''\tfor(int i = 0; i < (int)RenderLayer::MaxRenderLayer; i++){

\t\tif(!ctx.renderLayerVisibility[i]){
\t\t\tcontinue;
\t\t}

\t\tif(i != (int)RenderLayer::SortTransparent3D &&
\t\t   i != (int)RenderLayer::Transparent3D){
\t\t\tcontinue;
\t\t}

\t\tstd::vector<TransparentDrawItem> transparentList;

\t\tfor(auto& [name, scene] : m_context->sceneManager->GetActiveScenes()){

\t\t\tauto context = scene->GetSceneContext();
\t\t\tauto entities = context->component->FindEntitiesWithComponent<TransformComponent>();
\t\t\tif(entities.empty()){
\t\t\t\tcontinue;
\t\t\t}

\t\t\tfor(Entity entity : entities){

\t\t\t\tRenderLayer layer = scene->GetRenderLayerFromEntity(entity);
\t\t\t\tif((int)layer != i){
\t\t\t\t\tcontinue;
\t\t\t\t}

\t\t\t\tif(layer == RenderLayer::SortTransparent3D){

\t\t\t\t\tauto transform = context->component->GetComponent<TransformComponent>(entity);
\t\t\t\t\tif(!transform){
\t\t\t\t\t\tcontinue;
\t\t\t\t\t}

\t\t\t\t\tVector3 worldPos = transform->GetWorldPosition(context->component);
\t\t\t\t\tVector3 diff = worldPos - Vector3(ctx.CameraPosition.x, ctx.CameraPosition.y, ctx.CameraPosition.z);

\t\t\t\t\tTransparentDrawItem item;
\t\t\t\t\titem.ref = EntityRef(entity, context);
\t\t\t\t\titem.distanceSq = diff.dot(diff);
\t\t\t\t\ttransparentList.push_back(item);

\t\t\t\t} else{
\t\t\t\t\tfor(auto renderable : renderables){
\t\t\t\t\t\tint materialID = 0;
\t\t\t\t\t\tauto material = context->component->GetComponent<MaterialComponent>(entity);
\t\t\t\t\t\tif(material){
\t\t\t\t\t\t\tmaterialID = material->ShaderID;
\t\t\t\t\t\t}

\t\t\t\t\t\tbindForwardPS(materialID);

\t\t\t\t\t\tObjectInfo info;
\t\t\t\t\t\tinfo.SceneID = m_context->sceneManager->GetIDFromContext(context);
\t\t\t\t\t\tinfo.ObjectID = entity;
\t\t\t\t\t\tinfo.ShaderID = materialID;
\t\t\t\t\t\tm_context->graphics->SetObjectInfo(info);
\t\t\t\t\t\trenderable->Execute(ctx, context, entity);
\t\t\t\t\t}
\t\t\t\t}
\t\t\t}
\t\t}

\t\tif(!transparentList.empty()){

\t\t\tstd::sort(
\t\t\t\ttransparentList.begin(), transparentList.end(),
\t\t\t\t[](const TransparentDrawItem& a, const TransparentDrawItem& b){
\t\t\t\t\treturn a.distanceSq > b.distanceSq;
\t\t\t\t}
\t\t\t);

\t\t\tfor(auto& item : transparentList){

\t\t\t\tif(!item.ref.IsValid()) continue;
\t\t\t\tEntity       entity = item.ref.GetEntityID();
\t\t\t\tSceneContext* itemCtx = item.ref.GetScene();

\t\t\t\tfor(auto renderable : renderables){

\t\t\t\t\tint materialID = 0;
\t\t\t\t\tauto material = itemCtx->component->GetComponent<MaterialComponent>(entity);
\t\t\t\t\tif(material){
\t\t\t\t\t\tmaterialID = material->ShaderID;
\t\t\t\t\t}

\t\t\t\t\tbindForwardPS(materialID);

\t\t\t\t\tObjectInfo info;
\t\t\t\t\tinfo.SceneID = m_context->sceneManager->GetIDFromContext(itemCtx);
\t\t\t\t\tinfo.ObjectID = entity;
\t\t\t\t\tinfo.ShaderID = materialID;
\t\t\t\t\tm_context->graphics->SetObjectInfo(info);

\t\t\t\t\trenderable->Execute(ctx, itemCtx, entity);
\t\t\t\t}
\t\t\t}
\t\t}
\t}
'''
new = '''\tauto drawPacket = [&](const RenderPacket& packet){
\t\tSceneContext* sceneContext =
\t\t\tm_context->sceneManager->GetContextFromID(packet.sceneContextID);
\t\tif(!sceneContext) return;

\t\tIRenderable* renderable =
\t\t\tm_renderSystem->GetRenderableForPacketKind(packet.kind);
\t\tif(!renderable) return;

\t\tconst int materialID = static_cast<int>(packet.materialKey);
\t\tbindForwardPS(materialID);

\t\tObjectInfo info;
\t\tinfo.SceneID = packet.sceneContextID;
\t\tinfo.ObjectID = packet.entity;
\t\tinfo.ShaderID = materialID;
\t\tm_context->graphics->SetObjectInfo(info);
\t\trenderable->Execute(ctx, sceneContext, packet.entity);
\t};

\tconst RenderPacketFrameBuffer& packetBuffer =
\t\tm_renderSystem->GetRenderPacketBuffer();
\tif(packetBuffer.IsReady()){
\t\tfor(int layerIndex = 0;
\t\t\tlayerIndex < static_cast<int>(RenderLayer::MaxRenderLayer);
\t\t\t++layerIndex){
\t\t\tif(!ctx.renderLayerVisibility[layerIndex]) continue;
\t\t\tif(layerIndex != static_cast<int>(RenderLayer::Transparent3D) &&
\t\t\t   layerIndex != static_cast<int>(RenderLayer::SortTransparent3D)){
\t\t\t\tcontinue;
\t\t\t}

\t\t\tstd::vector<RenderPacketViewItem> sortedPackets;
\t\t\tfor(const RenderPacket& packet : packetBuffer.Packets()){
\t\t\t\tif(static_cast<int>(packet.layer) != layerIndex) continue;
\t\t\t\tif(!HasRenderPacketPass(packet.passMask, RenderPacketPassMask::Forward)){
\t\t\t\t\tcontinue;
\t\t\t\t}

\t\t\t\tif(packet.layer != RenderLayer::SortTransparent3D){
\t\t\t\t\tdrawPacket(packet);
\t\t\t\t\tcontinue;
\t\t\t\t}

\t\t\t\tconst float dx = packet.transform.worldPosition[0] - ctx.CameraPosition.x;
\t\t\t\tconst float dy = packet.transform.worldPosition[1] - ctx.CameraPosition.y;
\t\t\t\tconst float dz = packet.transform.worldPosition[2] - ctx.CameraPosition.z;
\t\t\t\tsortedPackets.push_back({&packet, dx * dx + dy * dy + dz * dz});
\t\t\t}

\t\t\tstd::sort(
\t\t\t\tsortedPackets.begin(),
\t\t\t\tsortedPackets.end(),
\t\t\t\tRenderPacketBackToFront
\t\t\t);
\t\t\tfor(const RenderPacketViewItem& item : sortedPackets){
\t\t\t\tif(item.packet) drawPacket(*item.packet);
\t\t\t}
\t\t}
\t}
'''
text = replace_once(text, old, new, "Forward Render Packet connection")
save(path, text)


path = "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/RenderPass/OverlayUI/OverlayUIPass.cpp"
text = load(path)
old = '''\t// 透明・UIレイヤーのみ描画
\tfor (int i = 0; i < (int)RenderLayer::MaxRenderLayer; i++) {

\t\tif (!ctx.renderLayerVisibility[i]) {
\t\t\tcontinue;
\t\t}

\t\tif (i != (int)RenderLayer::OverlayUI) {
\t\t\tcontinue;
\t\t}

\t\tstd::vector<SpriteDrawItem>      spriteList;

\t\tfor (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {

\t\t\tauto context = scene->GetSceneContext();
\t\t\tauto entities = context->component->FindEntitiesWithComponent<TransformComponent>();
\t\t\tif (entities.empty()) {
\t\t\t\tcontinue;
\t\t\t}

\t\t\tfor (Entity entity : entities) {

\t\t\t\tRenderLayer layer = scene->GetRenderLayerFromEntity(entity);
\t\t\t\tif ((int)layer != i) {
\t\t\t\t\tcontinue;
\t\t\t\t}

\t\t\t\tif (layer == RenderLayer::OverlayUI) {

\t\t\t\t\tauto transform = context->component->GetComponent<TransformComponent>(entity);
\t\t\t\t\tif (!transform) {
\t\t\t\t\t\tcontinue;
\t\t\t\t\t}

\t\t\t\t\tSpriteDrawItem item;
\t\t\t\t\titem.ref = EntityRef(entity, context);
\t\t\t\t\titem.orderInLayer = 0;
\t\t\t\t\tOrderInLayerComponent* layerComp = context->component->GetComponent<OrderInLayerComponent>(entity);
\t\t\t\t\tif (layerComp) {
\t\t\t\t\t\titem.orderInLayer = layerComp->order;
\t\t\t\t\t}
\t\t\t\t\tspriteList.push_back(item);

\t\t\t\t} else {

\t\t\t\t\tfor (auto renderable : renderables) {

\t\t\t\t\t\tint materialID = 0;
\t\t\t\t\t\tauto material = context->component->GetComponent<MaterialComponent>(entity);
\t\t\t\t\t\tif (material) {
\t\t\t\t\t\t\tmaterialID = material->ShaderID;
\t\t\t\t\t\t}

\t\t\t\t\t\tObjectInfo info;
\t\t\t\t\t\tinfo.SceneID = m_context->sceneManager->GetIDFromContext(context);
\t\t\t\t\t\tinfo.ObjectID = entity;
\t\t\t\t\t\tinfo.ShaderID = materialID;
\t\t\t\t\t\tm_context->graphics->SetObjectInfo(info);

\t\t\t\t\t\trenderable->Execute(ctx, context, entity);
\t\t\t\t\t}
\t\t\t\t}
\t\t\t}
\t\t}

\t\t// オーダーソートしてスプライト描画
\t\tif (!spriteList.empty()) {

\t\t\tstd::sort(
\t\t\t\tspriteList.begin(), spriteList.end(),
\t\t\t\t[](const SpriteDrawItem& a, const SpriteDrawItem& b) {
\t\t\t\treturn a.orderInLayer > b.orderInLayer;
\t\t\t}
\t\t\t);

\t\t\tfor (auto& item : spriteList) {

\t\t\t\tif (!item.ref.IsValid()) continue;
\t\t\t\tEntity       entity = item.ref.GetEntityID();
\t\t\t\tSceneContext* itemCtx = item.ref.GetScene();

\t\t\t\tfor (auto renderable : renderables) {

\t\t\t\t\tint materialID = 0;
\t\t\t\t\tauto material = itemCtx->component->GetComponent<MaterialComponent>(entity);
\t\t\t\t\tif (material) {
\t\t\t\t\t\tmaterialID = material->ShaderID;
\t\t\t\t\t}

\t\t\t\t\tObjectInfo info;
\t\t\t\t\tinfo.SceneID = m_context->sceneManager->GetIDFromContext(itemCtx);
\t\t\t\t\tinfo.ObjectID = entity;
\t\t\t\t\tinfo.ShaderID = materialID;
\t\t\t\t\tm_context->graphics->SetObjectInfo(info);

\t\t\t\t\trenderable->Execute(ctx, itemCtx, entity);
\t\t\t\t}
\t\t\t}
\t\t}
\t}
'''
new = '''\tif(ctx.renderLayerVisibility[RenderLayer::OverlayUI]){
\t\tstd::vector<const RenderPacket*> overlayPackets;
\t\tconst RenderPacketFrameBuffer& packetBuffer =
\t\t\tm_renderSystem->GetRenderPacketBuffer();

\t\tif(packetBuffer.IsReady()){
\t\t\tfor(const RenderPacket& packet : packetBuffer.Packets()){
\t\t\t\tif(packet.layer != RenderLayer::OverlayUI) continue;
\t\t\t\tif(!HasRenderPacketPass(packet.passMask, RenderPacketPassMask::Overlay)){
\t\t\t\t\tcontinue;
\t\t\t\t}
\t\t\t\toverlayPackets.push_back(&packet);
\t\t\t}
\t\t}

\t\tstd::sort(
\t\t\toverlayPackets.begin(),
\t\t\toverlayPackets.end(),
\t\t\tRenderPacketOverlayOrder
\t\t);

\t\tfor(const RenderPacket* packet : overlayPackets){
\t\t\tif(!packet) continue;
\t\t\tSceneContext* sceneContext =
\t\t\t\tm_context->sceneManager->GetContextFromID(packet->sceneContextID);
\t\t\tif(!sceneContext) continue;

\t\t\tIRenderable* renderable =
\t\t\t\tm_renderSystem->GetRenderableForPacketKind(packet->kind);
\t\t\tif(!renderable) continue;

\t\t\tObjectInfo info;
\t\t\tinfo.SceneID = packet->sceneContextID;
\t\t\tinfo.ObjectID = packet->entity;
\t\t\tinfo.ShaderID = static_cast<int>(packet->materialKey);
\t\t\tm_context->graphics->SetObjectInfo(info);
\t\t\trenderable->Execute(ctx, sceneContext, packet->entity);
\t\t}
\t}
'''
text = replace_once(text, old, new, "Overlay Render Packet connection")
save(path, text)


path = "Tests/RenderPacketBufferSmokeTest.cpp"
text = load(path)
text = replace_once(
    text,
    '''\tassert(packets[2].kind == RenderPacketKind::Wave);
\tassert(packets[3].kind == RenderPacketKind::Sprite);
\treturn 0;
}
''',
    '''\tassert(packets[2].kind == RenderPacketKind::Wave);
\tassert(packets[3].kind == RenderPacketKind::Sprite);

\tRenderPacket nearPacket = MakePacket(
\t\t1, Entity(1, 0), RenderPacketKind::Model,
\t\tRenderLayer::SortTransparent3D, 0, 0, 0
\t);
\tRenderPacket farPacket = MakePacket(
\t\t1, Entity(2, 0), RenderPacketKind::Model,
\t\tRenderLayer::SortTransparent3D, 0, 0, 1
\t);
\tstd::vector<RenderPacketViewItem> transparentView{
\t\t{&nearPacket, 1.0f},
\t\t{&farPacket, 9.0f}
\t};
\tstd::sort(transparentView.begin(), transparentView.end(), RenderPacketBackToFront);
\tassert(transparentView.front().packet == &farPacket);

\tRenderPacket lowOrder = MakePacket(
\t\t1, Entity(3, 0), RenderPacketKind::Sprite,
\t\tRenderLayer::OverlayUI, 0, -2, 0
\t);
\tRenderPacket highOrder = MakePacket(
\t\t1, Entity(4, 0), RenderPacketKind::Sprite,
\t\tRenderLayer::OverlayUI, 0, 4, 1
\t);
\tstd::vector<const RenderPacket*> overlayView{&lowOrder, &highOrder};
\tstd::sort(overlayView.begin(), overlayView.end(), RenderPacketOverlayOrder);
\tassert(overlayView.front() == &highOrder);
\treturn 0;
}
''',
    "Render Packet view sort tests",
)
save(path, text)


path = "Docs/Step17_Task_Naming_And_Render_Task_Decomposition_Plan.md"
text = load(path)
text = replace_once(
    text,
    "状態: **基盤実装中**\n",
    "状態: **主要RenderPass接続完了・計測待ち**\n",
    "Step 17-B view connection state",
)
text = replace_once(
    text,
    "- [ ] 既存RenderPassとの接続\n",
    "- [x] 既存RenderPassとの接続\n",
    "Render Pass connection checklist",
)
text = replace_once(
    text,
    "- `Docs/Step17B_Render_Packet_Opaque_Pass_Connection.md`\n",
    "- `Docs/Step17B_Render_Packet_Opaque_Pass_Connection.md`\n"
    "- `Docs/Step17B_Render_Packet_View_Pass_Connection.md`\n",
    "View pass connection document link",
)
save(path, text)


path = "Docs/ECS_Scheduler_Migration_Plan.md"
text = load(path)
text = replace_once(
    text,
    '''1. Step 17-B Render Packet既存RenderPass接続
2. Step 17-C Animation CPU Build / GPU Upload分離
3. Step 17-D Terrain CPU Mesh Build / GPU Upload分離
4. Step 17-E Wave CPU Vertex Build / GPU Upload分離
''',
    '''1. Step 17-B CaptureでPacket Build / Command Submit分離確認
2. Step 17-C Animation CPU Build / GPU Upload分離
3. Step 17-D Terrain CPU Mesh Build / GPU Upload分離
4. Step 17-E Wave CPU Vertex Build / GPU Upload分離
''',
    "Current position after Render Pass connection",
)
save(path, text)


foundation_path = "Docs/Step17B_Render_Packet_Foundation.md"
foundation = load(foundation_path)
foundation += '''

## View Pass接続

ForwardとOverlayもRender Packet入力へ移行した。SortTransparent3DはPacketに保存したWorld Position Snapshotからカメラ距離を計算し、Back-to-Frontで提出する。Overlay UIはPacketのOrderInLayerを降順Sortして提出する。
'''
save(foundation_path, foundation)


doc = '''# Step 17-B: Render Packet View Pass Connection

## 完了範囲

- Forward PassのScene / Transform Entity再走査を除去
- Transparent3Dを完成済みPacket順で提出
- SortTransparent3DをWorld Position SnapshotからBack-to-Front Sort
- Overlay UIのScene / Entity再走査を除去
- Overlay UIをOrderInLayer降順でSort
- Material Key、Scene Context ID、Entity、Packet KindをPacketから使用

## MainThreadに残す処理

- Shader / Pipeline bind
- RenderTarget / Depth state設定
- Object constant設定
- IRenderableによる実Draw
- Camera依存のView Sort

## Workerへ移した処理

- Renderable Component検出
- RenderLayer / Material / Order分類
- Local / World Transform Snapshot
- Pass Mask分類
- 決定的Packet Merge

## 残作業

- Schedule Captureで`RenderSystem.Packet.Build`と`RenderSystem.Command.Submit`の分離確認
- IRenderable内部のComponentRegistry参照除去
- PacketへDraw Resource Handle / Geometry Rangeを格納

Capture確認完了後、Step 17-C Animation CPU Build / GPU Upload分離へ進む。
'''
save("Docs/Step17B_Render_Packet_View_Pass_Connection.md", doc)

print("Render Packet Forward and Overlay connection migration applied")
