# Step 19-A GPU Pixel Cost Optimization

## 背景

Player ViewまたはWindowの表示サイズを大きくすると、描画オブジェクト数を変えていなくてもFrame Rateが大きく低下する。

この挙動は、CPUのEntity走査やDraw Call生成よりも、次の解像度依存処理が支配的である可能性が高い。

- GBufferへのMultiple Render Target書き込み
- Deferred LightingでのGBuffer読み込み
- Shadow Filter
- SSAO / SSR / Bloom等のFull Screen処理
- Editor ViewとPlayer Viewの二重描画
- Post EffectごとのRender Target読み書き
- Render TargetのFormatと総Bytes Per Pixel

現行`GBufferPS.hlsl`は次の6 Targetを出力する。

```text
Target 0: Albedo
Target 1: World Normal
Target 2: World Position
Target 3: Material
Target 4: Emissive
Target 5: uint4 Parameter
```

World PositionはDepthから復元できる可能性があり、`uint4` Parameterは用途に対してFormatが過大である可能性がある。

ただし、Format変更を先に行うとLighting、SSR、SSAO、Picking、Material選択のどこで回帰したか判別できない。最初にGPU Pass時間とBytes Per Pixelを可視化し、変更を一つずつ比較する。

## 優先度

状態: **最優先・未着手**

Static BatchはDraw CallとCPU Submit負荷には有効だが、画面サイズに比例するPixel Costは直接削減しない。

以後の性能作業は次の順序に切り替える。

1. GPU Pass単位Timestamp
2. 固定Render Scale
3. GBuffer Format / Bytes Per Pixel台帳
4. World Position Target削除
5. Parameter Target縮小・Player Picking分離
6. SSAO / SSR / Bloom等の低解像度化
7. Full Screen Pass統合
8. RenderDocによるBefore / After比較
9. 描画並列化の再検討

Compile Error、描画欠落、Resource Lifetime等のCorrectness修正は引き続き最優先とする。

---

# 1. 基本契約

## 1.1 計測なしにFormatを変更しない

各最適化は次を記録してから行う。

- GPU Frame Time
- Pass別GPU Time
- Internal Render Width / Height
- Pixel Count
- Render Target Format
- Bytes Per Pixel
- Targetごとの推定Frame Write量
- Targetごとの推定Frame Read量
- Draw Call数
- Editor View / Player Viewの有効状態
- Post Effect有効状態
- Shadow設定

## 1.2 CPU時間とGPU時間を混同しない

次はGPU Pass時間として扱わない。

- CPU側の`Present()`呼び出し時間
- Queue Submit関数のCPU時間
- Command生成時間
- Query結果回収待機時間

D3D11 Timestamp Queryは2～4 Frame遅延で回収し、同一Frameで待機しない。

## 1.3 一度に一つの帯域変更だけを行う

次を同一Commitで変更しない。

- World Position削除
- Parameter Packing
- Half Resolution化
- Post Effect統合

各変更にFeature Toggleまたは旧経路との比較手段を用意し、GPU時間と描画結果を個別に検証する。

## 1.4 Window SizeとInternal Render Sizeを分離する

Window、ImGui Panel、SwapChainのサイズを、そのまま全Render Targetの解像度として使用しない。

```text
Display Size
    -> Render Scale
    -> Internal Render Size
    -> GBuffer / Lighting / Post Effect
    -> Final Upscale
    -> Display Target
```

Picking、Mouse座標、Viewport、ScissorはDisplay座標とInternal座標を明示的に変換する。

---

# 2. Step 19-A.1 GPU Pass Timestamp

## 目的

GPU Frame全体だけでなく、どのPassが解像度増加に比例しているかを特定する。

## 計測対象

最低限、次を別Scopeとして計測する。

- Shadow Map
- Player GBuffer
- Player Deferred Lighting
- Player Post Effect合計
- Editor GBuffer
- Editor Deferred Lighting
- Editor Post Effect合計
- SSAO
- SSR
- Bloom Downsample
- Bloom Blur
- Bloom Composite
- Debug / Overlay
- ImGui Main Viewport
- ImGui Platform Windows
- Final Composite / Upscale
- GPU Frame Total

存在しないPassは0msとして表示せず、`Not Executed`として扱う。

## 実装契約

- `D3D11_QUERY_TIMESTAMP_DISJOINT`はFrame単位
- Pass Begin / Endに`D3D11_QUERY_TIMESTAMP`
- Query ObjectはFrame Ringで再利用
- 未回収Queryを上書きしない
- `GetData`でFlushを要求しない
- Disjoint時は該当Frame全体を無効化
- Resize / Device Lost時はPending Queryを破棄
- Scope名は固定IDとし、毎Frame文字列Allocationしない
- CPU Schedule Captureとは別Telemetryとして保持

## Editor表示

Performance Monitorへ次を追加する。

```text
GPU Frame
Pixel Count
Resolution
Render Scale
Shadow
Player GBuffer
Player Lighting
Player Post
Editor GBuffer
Editor Lighting
Editor Post
ImGui
Unaccounted GPU
```

`Unaccounted GPU`はGPU Frame Totalから有効Pass合計を引いた値とする。負値はTimer重複またはScope設計不良として警告する。

## 完了条件

- Pass合計とGPU Frame Totalの差が安定している
- Query回収にCPU Stallを発生させない
- Player / Editor二重描画を別々に確認できる
- Window拡大時に増加するPassを特定できる

---

# 3. Step 19-A.2 Baseline Matrix

## 固定条件

- VSync OFF
- 同一Scene
- 同一Camera
- 同一Object数
- 同一Light数
- 同一Shadow設定
- 同一Post Effect設定
- 60 Frame以上Warm-up
- 120 Frame以上の平均とP95を記録

## 解像度

最低限、次を比較する。

```text
1280 x 720
1920 x 1080
2560 x 1440
```

可能なら3840 x 2160も測定するが、VRAM不足やTDRの危険がある場合は必須としない。

## 描画構成

次を個別に測る。

1. Player Viewのみ
2. Editor Viewのみ
3. Player + Editor
4. GBuffer + Lightingのみ
5. Post Effect全停止
6. Shadow停止
7. SSAOのみ停止
8. SSRのみ停止
9. Bloomのみ停止
10. Render Scale 1.00 / 0.75 / 0.50

## Pixel Bound判定

解像度のPixel数増加に近い比率でGPU時間が増え、Render Scale低下で大きくGPU時間が下がる場合、Pixel / Bandwidth Boundと判定する。

Draw Call数を減らしてもGPU時間がほぼ変化しない場合、Static Batchを主要対策として扱わない。

---

# 4. Step 19-A.3 Fixed Render Scale

## 設定

Project Settingsへ次を追加する。

```text
Render Scale: 0.50 ～ 1.00
Player Render Scale
Editor Render Scale
Post Effect Scale Override
```

初期実装は固定値だけとし、Dynamic Resolutionは導入しない。

## Internal Size

```text
internalWidth  = max(MinWidth,  floor(displayWidth  * renderScale))
internalHeight = max(MinHeight, floor(displayHeight * renderScale))
```

- 最低サイズを持つ
- 0除算を禁止
- 奇数サイズを許容するか2 Pixel単位へ丸めるかを固定
- Panel Resize中に毎Frame Resourceを再生成しない
- Size確定までHysteresisまたはResize Debounceを使用
- PlayerとEditorは個別Sizeを持てる

## Final Upscale

初期版はBilinear Upscaleでよい。

品質改善は次段階とする。

- Sharpen
- FSR 1相当Spatial Upscale
- Temporal Upscale
- Dynamic Resolution

## Picking座標

```text
internalX = displayX * internalWidth  / displayWidth
internalY = displayY * internalHeight / displayHeight
```

Clamp後にInteger Pixelへ変換する。Editor ViewのObject Picking回帰を必須とする。

## 完了条件

- Display Sizeを変えてもRender Scaleに応じたInternal Sizeになる
- Scale 0.50でGBuffer / Lighting / PostのGPU時間が明確に低下する
- Picking座標が一致する
- Resize中にResource再生成が連続しない

---

# 5. Step 19-A.4 GBuffer Format Budget

## 現行台帳

実装開始時にRender Target生成箇所から、実際のDXGI / RHI Formatを取得して表へ記録する。

| Target | Semantic | Current Format | Bytes / Pixel | Producer | Consumers | Player必須 | Editor必須 |
|---|---|---:|---:|---|---|---|---|
| 0 | Albedo | 調査 | 調査 | GBuffer | Lighting / Post | Yes | Yes |
| 1 | World Normal | 調査 | 調査 | GBuffer | Lighting / SSAO / SSR | Yes | Yes |
| 2 | World Position | 調査 | 調査 | GBuffer | Lighting / SSR / Post | 要削除検討 | 要削除検討 |
| 3 | Material | 調査 | 調査 | GBuffer | Lighting | Yes | Yes |
| 4 | Emissive | 調査 | 調査 | GBuffer | Lighting / Bloom | 条件付き | 条件付き |
| 5 | Scene/Object/Shader/Flags | 調査 | 調査 | GBuffer | Material選択 / Picking | 要分離 | Yes |
| D | Depth | 調査 | 調査 | GBuffer | Depth Test / Position復元 | Yes | Yes |

## 推定帯域

Targetごとに次を表示する。

```text
Write MiB / Frame = width * height * bytesPerPixel / 1024 / 1024
Read  MiB / Frame = Write MiB * estimatedReadCount
MiB / Second      = MiB / Frame * FPS
```

MSAAを使用する場合はSample Countを掛ける。

## 完了条件

- 全TargetのFormatと用途が記録される
- 未使用Channelが特定される
- Playerに不要なEditor専用Targetが特定される
- Format変更前後のBytes Per Pixel差を計算できる

---

# 6. Step 19-A.5 World Position Target Removal

## 方針

World PositionはDepthとInverse View Projectionから復元する。

```text
Screen UV
    -> NDC
    -> Depth
    -> Inverse Projection / Inverse ViewProjection
    -> View Position / World Position
```

共通のShader Helperへ実装し、Lighting、SSAO、SSR、Fog、God Ray等が別々の復元式を持たないようにする。

## 前提確認

- Depth TextureをShader Resourceとして参照可能
- D3D11 Typeless Texture / DSV / SRV Format契約が正しい
- Reversed-Z使用有無を統一
- NDC Z RangeをBackendごとに吸収
- Projection Matrixの行列規約を統一
- Editor / Player CameraのInverse Matrixが有効

## 移行順

1. DepthからWorld Positionを復元するHelperを追加
2. Debug ViewでGPositionとの差分を可視化
3. Lightingだけを復元経路へ変更
4. SSAO / SSR / Post Effectを順番に変更
5. 全Consumer移行後にTarget 2生成を停止
6. PipelineのColor Attachment数を更新
7. Static Batch Pipeline契約を更新
8. Shader Compile / D3D11 Pipeline Smokeを更新

## A/B検証

旧GPositionと復元Positionの差を表示するDebug Modeを用意する。

```text
Absolute Position Error
View Space Z Error
Lighting Difference
Shadow Receiver Difference
SSR Hit Difference
```

## Fallback

Depth SRVまたはInverse Matrixが無効な場合、黒画面やNaNを出さない。

移行期間中は旧GPosition経路へFallbackできるToggleを保持する。

## 完了条件

- GPositionを読んでいる全Shaderが0件になる
- GBuffer Color Attachmentが1枚減る
- Lighting / Shadow / SSRに許容外差分がない
- GPU GBuffer時間またはLighting帯域が改善する

---

# 7. Step 19-A.6 Parameter Target Reduction

## 現行用途

現行`uint4`は次を格納する。

```text
x: Scene ID
y: Object ID
z: Shader / Material ID
w: Material Flags
```

各値の最大範囲、0の意味、Invalid値を調査してからPackingする。

## 方針候補

### Player View

PlayerでObject Pickingを行わない場合、Scene / Object ID Targetを生成しない。

Lightingに必要なMaterial / Shader分類だけを小さいTargetへ保持する。

### Editor View

Picking用IDを`R32_UINT`へ分離する。

Material分類とFlagsは別の`R32_UINT`または`R16G16_UINT`へPackする。

### Packing例

実際のID上限を確認後にBit数を決定する。

```text
Material ID
Shader ID
Material Flags
Reserved
```

Scene IDとObject IDはPicking用ID Tableで間接化する案も検討する。

## 禁止事項

- 上限未確認のままBitを切り詰めない
- EditorとPlayerの用途を一つのFormatへ無理に統合しない
- Signed / Unsigned変換を暗黙にしない
- Clear値とInvalid IDを同じ意味にしない

## 完了条件

- Player Viewで不要なPicking書き込みが発生しない
- Editor Pickingが維持される
- Parameter系Bytes Per Pixelが縮小する
- Material別Lightingの早期判定が維持される

---

# 8. Step 19-A.7 Half / Quarter Resolution Effects

## 初期対象

- SSAO: Half Resolution
- SSR: Half Resolution、Low品質ではQuarter Resolution
- Bloom: Half Resolution開始のMip Chain
- God Ray / Volumetric系: HalfまたはQuarter Resolution
- Blur系: Quality設定によりHalf Resolution

## Upsample

単純Bilinearだけで輪郭漏れが目立つEffectは、Depth / Normal aware Upsampleを使用する。

```text
Low Resolution Color
Low Resolution Depth
Full Resolution Depth
Full Resolution Normal
    -> Bilateral Upsample
```

## Quality設定

```text
Off
Low
Medium
High
Ultra
```

内部Scale、Sample数、Step数、Blur RadiusをQualityから一意に決定する。

## 完了条件

- 各Effectを個別にFull / Half比較できる
- GPU時間とVRAM差を表示できる
- Depth Edgeで重大なHaloがない
- Effect無効時に関連Transient Targetを生成しない

---

# 9. Step 19-A.8 Full Screen Pass Fusion

## 対象

連続する単純なColor変換を一つのShaderへ統合する。

- Color Correction
- Grayscale
- Posterize
- Vignette
- Chromatic Aberration
- Fog Composite
- Bloom Composite

Blur、SSR Ray March、SSAO等の専用Passは無理に統合しない。

## Node式Post Effect

Node順序から次を分類する。

```text
Fusible Color Operations
Neighborhood Operations
Depth / Normal Operations
History Operations
```

連続するFusible Operationを一つのGenerated Shaderまたは固定Permutationへまとめる。

初期版は全組合せの動的Shader生成ではなく、代表的な固定Permutationでもよい。

## 完了条件

- Full Screen Draw回数が減る
- 中間Render Targetの読み書きが減る
- Node順序の結果が変化しない
- Shader Permutation数が管理可能である

---

# 10. 実装順序

## Phase 1: 観測

- [ ] GPU Pass Timestamp Query Pool
- [ ] Pass Scope ID
- [ ] Performance Monitor表示
- [ ] Baseline Matrix記録
- [ ] GBuffer Format / Bytes Per Pixel台帳

## Phase 2: 解像度制御

- [ ] Player / Editor固定Render Scale
- [ ] Internal Render Size
- [ ] Final Upscale
- [ ] Picking座標変換
- [ ] Resize Debounce

## Phase 3: GBuffer縮小

- [ ] Depth Position Reconstruction Helper
- [ ] Position Difference Debug View
- [ ] Lighting移行
- [ ] SSAO / SSR / Post Consumer移行
- [ ] World Position Target削除
- [ ] Player / Editor Parameter Target分離
- [ ] Parameter Packing

## Phase 4: Post Effect帯域削減

- [ ] SSAO Half Resolution
- [ ] SSR Half / Quarter Resolution
- [ ] Bloom Mip Chain
- [ ] Depth-aware Upsample
- [ ] Full Screen Pass Fusion

## Phase 5: 検証

- [ ] 720p / 1080p / 1440p比較
- [ ] Render Scale 1.00 / 0.75 / 0.50比較
- [ ] Player / Editor / 両方表示比較
- [ ] RenderDoc Resource / Event比較
- [ ] VRAM比較
- [ ] Debug / Release x64
- [ ] Picking / Shadow / SSAO / SSR / Bloom回帰

---

# 11. 完了条件

- GPU Frame TimeをPass単位で説明できる
- 解像度増加時にどのPassが支配するか表示できる
- Window SizeとInternal Render Sizeが分離される
- World Position TargetがDepth Reconstructionへ置換される
- Player ViewでEditor専用Picking Targetを書かない
- Parameter TargetのBytes Per Pixelが縮小される
- 重いPost Effectが品質設定に応じて低解像度実行される
- Full Screen Passと中間Target数が削減される
- 720p / 1080p / 1440pでBefore / After GPU時間を記録する
- 画面サイズ拡大時のFrame Rate低下が、変更前より明確に緩和される

## 非目標

初期工程では次を行わない。

- DLSS / FSR 2 / XeSS
- Temporal Upscaling
- Dynamic Resolution
- Mesh Shader
- Variable Rate Shading
- D3D12 / Vulkan専用Async Compute最適化

これらは固定Render Scale、GBuffer縮小、Pass計測が安定した後に再評価する。
