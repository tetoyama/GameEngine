# Step 19-A.2 Lighting Bottleneck Diagnosis

## 状態

**最優先・実装中**

2026-06-30の実機計測:

| Scope | GPU Time | GPU Frame比 |
|---|---:|---:|
| GPU Frame | 61.6301ms | 100% |
| Player Lighting | 39.6063ms | 約64.3% |
| Player Post Effect | 17.5012ms | 約28.4% |
| Player Shadow Map生成 | 2.1187ms | 約3.4% |
| Player GBuffer | 0.8786ms | 約1.4% |
| ImGui | 0.0512ms | 約0.1% |
| Unaccounted GPU | 1.4742ms | 約2.4% |

LightingとPost EffectだけでGPU Frameの約92.7%を占める。最初にLighting 39.6msを分解し、その後Post Effectへ進む。

診断UI:

```text
Project Settings
    -> Lighting
```

診断設定はRuntime専用であり、Scene YAMLまたはProject YAMLへ保存しない。

---

## 1. 禁止事項

### 1.1 `switch(materialID)`統合Deferred Shaderを再導入しない

過去の実機検証で、Material IDによる統合Shader分岐は軽量化しなかった。

同一Wave内に複数Materialが混在すると分岐先の実行が直列化され、重いMaterial経路を含む全経路の負荷を受ける可能性がある。したがって次を禁止する。

- `switch(materialID)`で全Material関数を一つのPixel Shaderへ統合
- 動的`if / else if`で全Material関数を選択
- `[branch]`指定だけを根拠に統合経路を再採用
- Material統合とShadow最適化を同時に変更

現行のMaterial別Deferred Pixel Shaderは診断中は維持する。

### 1.2 診断ToggleへMaterial分岐を追加しない

診断ToggleはDraw全体で同じ値となる定数バッファだけを使用する。

許可するToggle:

- Shadow評価の全体無効化
- PCF Kernel Radiusの全体Override
- Environment Reflectionの全体無効化
- 評価する最大Light数の全体制限

これらはPixelごとのMaterial IDではなく、Draw単位で均一な値として扱う。

---

## 2. 現行Lighting構造

現行Lighting Passは登録済みDeferred Pixel ShaderごとにFull Screen Quadを描画し、各Shaderが`GParam.materialID`を確認して担当外Pixelを`discard`する。

この構造には次の負荷がある。

- Deferred Material数分のFull Screen Draw
- 担当外PixelでもMaterial ID取得まではPixel Shaderが起動
- 各Material Shaderが独立してGBufferを参照

ただし、過去に失敗したMaterial統合へ戻さず、まず各Material Shader内部の共通Lighting負荷を分解する。

Shadow Map生成時間は`Player Shadow`へ計上されるが、Lighting Shader内のPCF Sample時間は`Player Lighting`へ計上される。Shadow Map生成が2.1msでも、Shadow参照がLighting 39.6msの主要部分である可能性がある。

CSMは最大4 Cascadeを評価し、既定PCF半径2では1 Cascadeあたり25比較Sampleとなる。Fallback経路では1 Pixelあたり最大100比較Sampleへ達する可能性があるため、No ShadowとPCF Overrideを最初に比較する。

---

## 3. Lighting診断用定数バッファ

`b3: CbLightingDebug`を使用する。

```text
Flags
    Disable Shadows
    Disable Environment Reflection
PCF Mode
    Material Default
    1x1
    3x3
    5x5
Max Active Lights
    0 = 全Light
    1..LIGHT_MAX_COUNT = 先頭N Lightのみ
Reserved
```

契約:

- 16 Byte境界を維持
- 全フィールド0を通常描画とする
- Player / Editorへ同じ設定を適用
- Debug診断専用でScene YAMLへ保存しない
- Runtime中に変更してもShader再Compileを要求しない
- Material ID分岐には使用しない

実装:

- PCF ModeとEnvironment Flagは`b3`をPBR ShaderへBindする
- Shadow無効と最大Light数は`CbPerFrame`の一時コピーへ適用する
- Lighting Draw後に元の`CbPerFrame`を即時復元する
- Shadow無効時もShadow Map生成Passは実行し、生成時間と参照時間を分離する

---

## 4. A/B測定順序

同一Scene、同一Camera、同一解像度、VSync OFFで測る。

Warm-up 60 Frame後、120 Frame以上の平均とP95を記録する。

### 4.1 Baseline

```text
Shadows: ON
PCF: Material Default
Environment: ON
Max Lights: 0 (All)
```

記録:

- GPU Frame
- Player Lighting
- Player Shadow
- Player Post Effect
- Active Light Count
- Shadow Cast Light Count
- Player Resolution / Pixel Count

### 4.2 No Shadow

```text
Disable Shadows: ON
```

`Baseline Lighting - No Shadow Lighting`を、Lighting Shader内Shadow評価コストとする。

Shadow Map生成Passは引き続き実行されるため、`Player Shadow`は変化しなくてもよい。初期診断では生成と参照を分離する。

### 4.3 PCF比較

```text
1x1: Kernel Radius 0
3x3: Kernel Radius 1
5x5: Kernel Radius 2
```

順番:

1. No Shadow
2. PCF 1x1
3. PCF 3x3
4. PCF 5x5
5. Material Default

PCF Sample数の目安:

| Mode | Samples / Shadow evaluation |
|---|---:|
| 1x1 | 1 |
| 3x3 | 9 |
| 5x5 | 25 |

CSM fallbackやPoint Face選択により、実際のSample数はこの値より増える可能性がある。

### 4.4 No Environment

```text
Disable Environment Reflection: ON
```

現行PBRはEnvironment Reflection有効Pixelで8 Sampleを行う。Baselineとの差をEnvironment Reflectionコストとして記録する。

### 4.5 Light Count制限

```text
Max Lights: 1
Max Lights: 2
Max Lights: 4
Max Lights: 8
Max Lights: All
```

`Player Lighting`がLight数にほぼ比例する場合、全画面Light Loopが主要因である。

---

## 5. 判定と次工程

### 5.1 Shadow参照が支配的

条件:

- No ShadowでLighting時間が大幅に低下
- 1x1 / 3x3 / 5x5で時間がSample数に応じて増加

次工程:

1. 通常品質を3x3へ変更
2. Low品質を1x1へ変更
3. CSM Cascade選択を先に行い、全Cascade fallback探索を通常経路から除去
4. Cascade境界だけ隣接Cascadeを評価
5. Shadow Cast Lightの画面影響範囲を絞る
6. LightごとのShadow更新頻度とStatic Shadow Cacheを検討

### 5.2 Light Loopが支配的

条件:

- Max Lights制限でLighting時間が大幅に低下

次工程:

1. Tiled / Clustered Light Cullingを設計
2. Point / Spot Lightを画面Tile単位で絞る
3. Directional LightはGlobal Listに分離
4. Dummy CSM / Point Face Entryを実Light Loopから分離
5. Shadow Atlas EntryとLogical Lightを別Bufferへ分離

### 5.3 Environment Reflectionが支配的

条件:

- No EnvironmentでLighting時間が大幅に低下

次工程:

1. 8 SampleのRuntime Blurを廃止
2. Prefiltered Environment Mapを生成
3. RoughnessからMip Levelを選択
4. 原則1 Sampleへ変更

### 5.4 Material別Full Screen Drawが支配的

Shadow、Environment、Light数を減らしても、Material数増加に応じてLighting時間が増える場合に判定する。

`switch(materialID)`統合には戻さない。

候補:

1. Depth / Stencil PrepassまたはGBuffer StencilへMaterial Classを書き込む
2. Material別Lighting DrawをStencil TestでPixel Shader起動前に制限
3. 標準PBR / Unlitの少数Classへ整理
4. 特殊MaterialはForwardまたは専用Passへ分離
5. Material Class数がStencil Bit数を超える場合は別方式を再設計

Stencil方式は次を検証してから採用する。

- GBuffer DepthのStencilをLighting Passで再利用可能か
- Editor PickingとStencil用途が競合しないか
- Material Class数の上限
- Full Screen Draw数とPixel Shader InvocationのRenderDoc比較

---

## 6. 実装状況

- [x] GPU Pass単位Timestamp
- [x] Player Lighting / Shadow / Post Effect分離
- [x] 2026-06-30 Baseline取得
- [x] `CbLightingDebug`追加
- [x] Shadow Disable Toggle
- [x] PCF 1x1 / 3x3 / 5x5 / Default Toggle
- [x] Environment Disable Toggle
- [x] Max Active Lights Toggle
- [x] Project SettingsへLighting診断UI追加
- [x] Settings Contract Smoke
- [x] PBR Shader Compile Smoke
- [x] 専用GitHub Actions Workflow
- [ ] Windows Build確認
- [ ] Lighting Diagnostic Contract確認
- [ ] Default設定の描画一致確認
- [ ] No Shadow測定
- [ ] PCF比較
- [ ] No Environment測定
- [ ] Light Count比較
- [ ] 支配要因を確定
- [ ] 支配要因に対応する最適化を実装
- [ ] Post Effect 17.5msの内訳計測へ移行

---

## 7. 完了条件

- `switch(materialID)`統合を使用していない
- 同一Shader Binaryのまま診断Toggleを変更できる
- Shadow参照、PCF、Environment、Light LoopのGPU時間差を個別に測定できる
- Player / Editorで同じ診断設定が適用される
- Default設定で変更前と同じ描画結果になる
- 診断設定がScene保存データを汚染しない
- Lighting 39.6msの主要因を数値で特定する
- 次の恒久最適化を測定結果に基づいて一つだけ選択する
