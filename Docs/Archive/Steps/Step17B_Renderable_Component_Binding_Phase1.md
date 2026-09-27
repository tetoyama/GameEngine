# Step 17-B Renderable Component Binding

## 目的

Renderable内部で同じEntityのComponentを`ComponentRegistry`から再取得する経路を除去し、Render Packet BuildとSubmitを分離する。

## 完了済み

- [x] RenderPacketへ型付き非所有Component Bindingを追加
- [x] Transform World / Parent WorldをPacket Build時にSnapshot
- [x] 全主要RenderPassを`IRenderable::Execute(RenderPacket)`へ統一
- [x] Model / Mesh / Sprite / Billboard / Particle / Terrain / Wave / EffectをPacket Bindingへ移行
- [x] 全主要Renderable Headerから`ComponentRegistry`宣言を除去
- [x] Mesh / Terrain / Wave / Effectの不要なRegistry includeを除去
- [x] Scene Stop / TempLoad前のPacket無効化契約を追加

主要RenderableのSubmit処理は`ComponentRegistry::GetComponent`を呼ばない。

## 寿命契約

Component Bindingは所有権を持たない。
Render Domain中はStructural Mutationを行わず、同じPacket世代のBuild完了からSubmit完了までのみ参照する。
Scene Stop / TempLoad / Shutdown前には公開済みPacketを破棄する。

## 同時修正

- Spriteは現在のRender TargetサイズでRectTransformを計算
- Mesh / Particle / Effectの既定LayerをForward対応Layerへ正規化
- Terrain / WaveのMaterialを既定白で初期化
- TerrainのUV Bufferをゼロ初期化し、既定範囲を`0..1`へ設定
- Waveの不要なTransform参照を除去

## 残作業

- [ ] Model / Sprite / Billboard / Particleの未使用include整理
- [ ] Sprite / Particle / Effect / Terrain / Waveの実機描画確認
- [ ] Sprite Gizmo位置確認
- [ ] Scene Stop / TempLoad直前のPacket破棄を実機確認
