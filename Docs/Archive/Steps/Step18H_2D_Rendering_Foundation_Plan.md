# Step 18-H: 2D Rendering Foundation Plan

Status: **Planned — 2026-08-02**

対象:

- `refactor/ecs-scheduler-foundation`
- PR #45

関連文書:

- `Docs/ECS_Scheduler_Migration_Plan.md`
- `Docs/Step18A_RenderWorld_Runtime_Ownership_Progress.md`
- `Docs/RenderPipeline_Graph_Architecture.md`
- `Docs/RenderPipeline_Graph_Integration_Plan.md`
- `Docs/RenderPipeline_Graph_Execution_Order_Amendment.md`

---

# 1. 目的

既存の簡易`SpriteRendererComponent`とOverlay UI描画を、2Dゲームを主用途として選択できる描画基盤へ拡張する。

目標は単にTexture付きQuadを描画することではなく、次をEditorとRuntimeで一貫して扱える状態にすることである。

- Sprite Asset Import / Slice / Pivot / Border
- World-space 2D Sprite
- Sorting Layer / Order in Layer / Sorting Group
- Sprite Atlas / Batching / Culling
- Simple / Sliced / Tiled Sprite
- Flip X / Y
- Sprite Mask
- Tilemap / Tile Palette / Chunk Renderer
- SpriteShape
- 2D Light / Normal Map / Shadow Caster
- Frame Animation / Skeletal Deformation
- Canvas / Layout / Text / Mask / Input
- Sprite Editor / Atlas Preview / Tile Palette / Scene Gizmo

2D描画を3D Rendererの特殊ケースやOverlay UIだけとして扱わない。

---

# 2. Pixel Perfectの責務境界

Pixel Perfect Cameraは純粋なPost Effectではない。

次の責務を分離する。

## 2.1 Camera / View側の責務

- Reference Resolution
- Pixels Per Unit
- Camera位置のPixel Grid量子化
- Orthographic Sizeの整合
- Viewport AspectへのCrop / Expand規則
- World座標からPixel座標への安定した変換
- Camera Cut / Resize時の量子化状態更新

これらは`RenderView`または2D Camera設定の責務とする。

## 2.2 RenderPipeline Graph側の責務

- Reference Resolutionの内部Render Target生成
- PostProcessを内部解像度またはDisplay解像度のどちらで実行するかのPolicy
- Nearest Neighborによる整数倍率拡大
- Letterbox / Pillarbox
- 非整数倍率時のFallback
- Overlay UIを拡大前後のどちらへ注入するかの選択

最終拡大は`FullscreenOperation`へLoweringする`PixelPerfectResolve Node`として表現できる。

```text
2D Camera / RenderView
    Camera量子化、Reference Resolution
            ↓
World 2D Render Target
            ↓
PostProcess Policy
            ↓
PixelPerfectResolve Node
            ↓
Display Resolution
            ↓
Native Resolution Overlay UI（任意）
```

したがって、本StepではPixel PerfectのCamera契約とPipeline接続点を定義するが、最終Resolve実装はRenderPipeline Graph側の工程として扱う。

---

# 3. 描画Domainの分離

2D機能を次のDomainへ分離する。

## 3.1 World 2D

Camera、World Transform、Culling、Sorting、Lighting、PostProcessの対象となる。

対象:

- Sprite
- Tilemap
- SpriteShape
- 2D Particle
- 2D Light
- 2D Shadow Caster
- World-space Canvas

## 3.2 Screen UI

Viewport / Canvas基準で配置し、通常はScene Lightingの対象外とする。

対象:

- Image
- Text
- Panel
- Layout
- Mask
- Scroll / Clip
- Input / Focus
- Screen-space Canvas

World 2DとScreen UIで同じ`SpriteRendererComponent`とTransform契約を共有しない。

既存`SpriteRendererComponent`のAnchor / Pivot方式はScreen UI互換経路として段階移行し、World 2D SpriteはWorld TransformとPixels Per Unitを使用する。

---

# 4. 基本アーキテクチャ

```text
Texture Source
    ↓ Import
Sprite Asset / Sprite Atlas Asset / Tile Asset
    ↓
ECS Components
    SpriteRenderer / SortingGroup / Tilemap / Canvas / Text
    ↓ Extraction
RenderWorld 2D Snapshot
    ↓ Prepare
2D DrawList / Tile Chunk List / Light2D List / UI DrawList
    ↓
RenderPipeline Graph
    World2D Raster / Light2D / Mask / UI / Resolve
    ↓
RenderOperation[]
    ↓
Step 16-F RenderGraph
    ↓
RHI
```

原則:

- ComponentはNative GPU Resourceを所有しない
- Import済みAssetとRuntime GPU Resourceを分離する
- RenderWorld抽出後の描画経路はComponentを直接参照しない
- Sprite / Tilemap / UIのDraw DataはFrame-local SnapshotまたはHandleで保持する
- D3D11専用型を2D上位層へ露出しない
- 2D描画もRenderGraphのHazard / Lifetime / Barrier管理下へ置く

---

# 5. Asset契約

## 5.1 Sprite Asset

Texture全体ではなく、Texture内の矩形領域を独立Assetとして扱う。

```cpp
struct SpriteAssetDesc
{
    TextureAssetHandle sourceTexture;
    RectInt sourceRectPixels;

    Vector2 pivotNormalized;
    Border4 borderPixels;

    float pixelsPerUnit = 100.0f;

    SpriteMeshType meshType = SpriteMeshType::FullRect;
    SpritePackingRotation packingRotation = SpritePackingRotation::None;

    bool alphaIsTransparency = true;
    bool generatePhysicsShape = false;
};
```

必要項目:

- Single / Multiple Import Mode
- Grid / Cell Size / Cell Count Slice
- Automatic Slice
- Sprite名
- Pivot Preset / Custom Pivot
- Border
- Pixels Per Unit
- Full Rect / Tight Mesh
- Filter Mode
- Wrap Mode
- Mipmap Policy
- sRGB / Linear
- Premultiplied Alpha Policy
- Normal Map / Mask Map関連付け

Texture Componentの`UV_Slice_X / UV_Slice_Y / AnimationNum`は互換ロード対象とし、Sprite Asset参照へMigration可能にする。

## 5.2 Sprite Atlas Asset

- 複数Sprite AssetのPacking
- Padding
- Extrude
- Rotation可否
- Tight Packing可否
- Max Atlas Size
- Platform / Backend別Override余地
- Atlas Revision
- Runtime Page Handle
- Repack時の参照安定性

Sprite EntityはAtlas内UVを直接永続保存せず、`SpriteAssetHandle`を保持する。

## 5.3 Tile Asset

- Sprite参照
- Color
- Transform
- Collider Shape参照
- Animation Frame列
- Animation Rate
- User Data / Tag

Rule TileやTerrain Ruleは後段拡張とするが、Tile Asset IDとResolver境界は初期から固定する。

---

# 6. Runtime Component契約

## 6.1 SpriteRendererComponent 2D

既存Componentとは段階移行し、World 2D用の純データComponentを定義する。

```cpp
struct SpriteRenderer2DComponent
{
    SpriteAssetHandle sprite;
    Color4 color = Color4::White;

    SpriteDrawMode drawMode = SpriteDrawMode::Simple;
    Vector2 size;

    bool flipX = false;
    bool flipY = false;

    SortingLayerId sortingLayer = DefaultSortingLayer;
    int32_t orderInLayer = 0;

    MaterialAssetHandle material;
    SpriteMaskInteraction maskInteraction;
};
```

`drawMode`:

- Simple
- Sliced
- Tiled

## 6.2 SortingGroupComponent

子階層を一つのSorting単位として扱う。

- Sorting Layer Override
- Order in Layer Override
- Child Local Order
- Nested Group
- Group単位Distance / Axis値
- Stable Tie Breaker

## 6.3 TilemapComponent

Componentには巨大なCell配列を直接保持せず、Tilemap Asset / Runtime StorageへのHandleを保持する。

- Grid Layout
- Cell Size
- Cell Gap
- Orientation
- Tilemap Asset Handle
- Sorting情報
- Color
- Chunk Size

## 6.4 SpriteShapeComponent

- Spline Point列
- Closed / Open
- Fill Material
- Edge Sprite Profile
- Corner Profile
- Collider生成用Outline出力

## 6.5 Canvas / UI Components

World 2Dとは別契約とする。

- Canvas
- RectTransform
- CanvasGroup
- UIImage
- UIText
- UIMask
- LayoutGroup
- ContentSizeFitter相当
- Selectable / Input Target

既存Anchor / Pivot実装はRectTransform Migrationの入力として利用する。

---

# 7. Sorting契約

現行`Background2D / OverlayUI`だけではUnity相当の2D Sortingを表現できないため、Sorting Layer Assetを追加する。

```cpp
struct SortingLayerEntry
{
    SortingLayerId id;
    std::string name;
    int32_t order;
};
```

World 2Dの基本Sort Key:

```text
Pipeline Domain
Sorting Layer Order
Sorting Group Order
Order In Layer
Transparency Sort Axis / Distance
Material / Atlas Page（Batch可能範囲のみ）
Stable Entity Sequence
```

注意:

- Batch効率のために視覚順序を変更しない
- 同一Sorting範囲内だけMaterial / Atlasでまとめる
- Transparent Sort ModeをCamera単位で選択可能にする
- Custom Axisを2D Camera設定へ持たせる
- Sorting Group内の相対順を保持する

---

# 8. RenderWorld 2D Snapshot

既存`RenderPacketKind::Sprite`を発展させる。

初期Snapshot:

```cpp
struct SpriteRenderSnapshot
{
    Entity entity;
    Transform2D worldTransform;

    SpriteAssetHandle sprite;
    MaterialAssetHandle material;

    Color4 color;
    Vector2 size;

    SortingKey2D sorting;
    SpriteDrawMode drawMode;
    SpriteMaskInteraction maskInteraction;

    bool flipX;
    bool flipY;
};
```

追加Snapshot:

- TilemapChunkRenderSnapshot
- SpriteShapeRenderSnapshot
- Light2DRenderSnapshot
- ShadowCaster2DRenderSnapshot
- CanvasRenderSnapshot
- TextRenderSnapshot

Extraction後のRuntime / Rendererが、`SpriteRenderer2DComponent`やTexture Componentを直接includeしない構造にする。

---

# 9. RHI Sprite Renderer

既存`RenderableSprite`のDirect3D 11直接描画を段階廃止する。

必要機能:

- RHI Pipeline State
- RHI Texture / Sampler Handle
- Shared Quad GeometryまたはVertex ID生成
- Instance Buffer
- Atlas UV
- Color
- Transform2D
- Flip
- Sliced / Tiled Parameter
- Mask Reference
- Per Draw Material Parameter

標準経路:

```text
Sprite Snapshot[]
    ↓ Stable Sort
Sprite Batch Builder
    ↓
Sprite Instance Buffer
    ↓
RasterOperation
    ↓
RenderGraph
    ↓
RHI DrawInstanced
```

最初から一Entity一Drawを標準構造にしない。

---

# 10. Sprite Atlas / Batching / Culling

## 10.1 Batching Key

- Pipeline / Material Pass
- Atlas Page
- Sampler
- Blend Mode
- Mask State
- Sorting連続範囲
- Render Target / View

## 10.2 Culling

- World 2D AABB
- Camera Orthographic Bounds
- Tilemap Chunk Bounds
- SpriteShape Bounds
- Canvas Clip Rect

## 10.3 Instance Data

- 2D Affine Transform
- UV Rect
- Color
- Size
- Border / Tiling
- Flags
- Mask ID

GPU Instance BufferはFrame Runtime Storageが所有し、Componentへ戻さない。

---

# 11. Sprite Mask

Sprite Maskは単なるTexture Alpha切抜きではなく、Sorting範囲と連動する。

必要契約:

- Mask Shape Sprite
- Front / Back Sorting Layer範囲
- Front / Back Order範囲
- Visible Inside / Outside
- Nested Mask Policy
- Mask ID / Stencil Bit管理
- Overflow時のFallback

Backend実装はStencilを第一候補とし、複雑なNested / Offscreen MaskはMask TextureへLowering可能にする。

Screen UI Maskとは契約を共有しすぎない。

---

# 12. Tilemap

## 12.1 Grid

- Rectangle
- Isometric
- Isometric Z as Y
- Hexagonalは後段

## 12.2 Chunk Runtime

- Dirty Cell範囲追跡
- Chunk単位Geometry / Instance再構築
- Chunk Bounds
- Atlas Page別Batch
- Animated Tile更新
- Scene Reload / Asset Revision対応

## 12.3 Editor

- Tile Palette
- Brush / Box / Fill / Erase / Picker
- Grid Overlay
- Layer選択
- Tilemap Scene View Preview
- Undo / Redo
- Prefab / Scene保存

Tilemapの全Cellを毎Frame RenderPacket化しない。

---

# 13. SpriteShape

SpriteShapeはSplineからEdge / Fill Geometryを生成する。

- CPU Tessellation
- Corner Sprite
- Edge Sprite Repeat / Stretch
- Fill UV
- Geometry Revision
- Dirty Segment更新
- RHI Geometry Runtime
- Collider用Outline出力境界

初期実装はStatic Shapeを対象とし、毎Frame変形は後段最適化とする。

---

# 14. 2D Lighting

2D Renderer専用のLight Accumulationを用意する。

Light種別候補:

- Global
- Point
- Freeform
- Sprite
- Parametric

必要入力:

- Color
- Intensity
- Blend Style
- Radius / Falloff
- Cookie / Light Sprite
- Target Sorting Layer範囲
- Volumetric Policy

Sprite Material入力:

- Albedo
- Normal Map
- Mask Map
- Emission
- Receive Light Flag

Pipeline例:

```text
Light2D Culling
    ↓
Light2D Accumulation per Blend Style
    ↓
Lit Sprite Raster
    ↓
World2D Color
```

単一巨大Pixel Shaderで全Lightを走査する方式を固定しない。

---

# 15. 2D Shadow Caster

- Polygon Shape
- Sprite Alpha Outlineからの生成
- Self Shadow Policy
- Target Sorting Layer範囲
- Light別Caster Culling
- Geometry Revision
- Editor Shape編集

2D Shadowは3D Shadow Atlas契約へ無理に統合せず、2D Light Pipeline固有Resourceとして扱う。

---

# 16. Sprite Animation

## 16.1 Frame Animation

- Sprite Asset Frame列
- Duration / FPS
- Loop Mode
- Event Marker
- Animator State連携
- Atlas Page跨ぎ対応

既存`TextureComponent::AnimationNum`は互換入力としてMigrationする。

## 16.2 Skeletal 2D

後段工程:

- Bone Hierarchy
- Sprite Skin Mesh
- Weight
- Deform Buffer
- CPU / GPU Skinning選択
- IK / Constraintとの境界
- Editor Bone / Weight Tool

Skeletal実装前でもAsset SchemaがFrame Animationと衝突しないようにする。

---

# 17. Canvas / Layout / Text / Input

Unity相当の2D制作を目標とする場合、描画だけでなくScreen UI制作基盤が必要である。

## 17.1 Canvas

- Screen Space Overlay
- Screen Space Camera
- World Space
- Reference Resolution
- Scale With Screen Size
- Match Width / Height
- Render Order

## 17.2 RectTransform

- Anchor Min / Max
- Pivot
- Anchored Position
- Size Delta
- Offset Min / Max
- Parent Rect依存Layout

現行の単一Anchorと正規化ScaleだけではStretchを表現できないため、互換Migrationを行う。

## 17.3 Layout

- Horizontal / Vertical Layout
- Grid Layout
- Padding / Spacing
- Min / Preferred / Flexible Size
- Content Size Fitter
- Aspect Ratio Fitter

## 17.4 Text

- Font Asset
- Glyph Atlas
- UTF-8 / Unicode
- Kerning
- Line Break
- Alignment
- Rich Textの段階対応
- SDF Text
- Fallback Font

## 17.5 Input / Focus

- Hit Test
- Pointer Enter / Exit
- Press / Click / Drag
- Keyboard / Gamepad Navigation
- Focus
- Raycast Blocking
- Canvas Sortとの整合

Inputは描画処理ではないが、CanvasのSort / Clip / Transformと同じSnapshotを参照するため、UI工程内で契約を固定する。

---

# 18. RenderPipeline Graph統合

2D専用Pipelineを別Rendererとして孤立させず、RenderPipeline Graph上の標準Nodeとして扱う。

Node候補:

- BuildWorld2DDrawList
- BuildTilemapDrawList
- BuildLight2DList
- World2DUnlitRaster
- Light2DAccumulation
- World2DLitRaster
- SpriteMask
- WorldSpaceCanvas
- PixelPerfectResolve
- ScreenSpaceCanvas
- Present

標準Semantic候補:

- World2DColor
- World2DNormal
- World2DMask
- Light2DAccumulation
- UIOverlayColor
- PixelPerfectColor

代表Pipeline:

```text
World2D Clear
    ↓
Tilemap / SpriteShape
    ↓
Light2D Accumulation
    ↓
Lit / Unlit Sprite
    ↓
World-space Canvas
    ↓
2D PostProcess
    ↓
PixelPerfectResolve（任意）
    ↓
Screen-space Canvas
    ↓
Present
```

3Dとの混在時は、World 2DをBackground / Transparent / Overlayのどこへ注入するかをPipeline Assetで選択可能にする。

---

# 19. Editor Tooling

## 19.1 Sprite Editor

- Slice
- Pivot
- Border
- Outline
- Physics Shape Preview
- Normal / Mask Map関連付け
- Frame Animation Preview

## 19.2 Atlas Inspector

- Packed Page Preview
- Padding / Bleed確認
- Sprite検索
- Repack
- Revision表示
- Batch Break理由表示

## 19.3 Scene View

- 2D Mode
- Orthographic固定
- Grid / Pixel Grid
- Sorting Layer可視化
- Mask範囲表示
- Light2D範囲表示
- Shadow Caster Shape編集
- Tile Palette
- Canvas Rect / Anchor Gizmo

## 19.4 Preview

Sprite、Tile、Tilemap Chunk、SpriteShape、2D Material、Light2D、Canvasを、Production Pipeline定義またはBuiltin 2D Preview Pipelineで描画する。

---

# 20. 実装工程

## 2D-0: Domain / Coordinate / Asset Contract

- [ ] World 2DとScreen UIの責務分離
- [ ] Pixels Per Unit契約
- [ ] Transform2D / RectTransform境界
- [ ] Sprite Asset / Atlas / Tile Asset Handle
- [ ] Sorting Layer Asset
- [ ] Pixel Perfect Camera / Resolve責務分離
- [ ] Legacy Sprite / Texture Slice Migration方針

完了条件:

- 2D World SpriteがViewport正規化Transformへ依存しない
- Screen UIがWorld 2D Sortingへ混入しない
- Pixel Perfectを単一Post Effectとして誤実装しない

## 2D-1: Sprite Import / Sprite Editor Foundation

- [ ] Single / Multiple Import
- [ ] Grid / Automatic Slice
- [ ] Pivot / Border / PPU
- [ ] Sprite Asset YAML
- [ ] Sprite Preview
- [ ] Legacy Texture Slice Migration

## 2D-2: RHI Sprite Renderer

- [ ] Sprite Render Snapshot
- [ ] Component Pointer非依存Extraction
- [ ] RHI Pipeline / Texture / Sampler
- [ ] Simple Sprite
- [ ] Flip X / Y
- [ ] Material Color
- [ ] World 2D Camera
- [ ] Existing Sprite Renderer互換経路

## 2D-3: Sorting Layer / Sorting Group

- [ ] Sorting Layer Asset
- [ ] Order in Layer
- [ ] Sorting Group
- [ ] Custom Axis
- [ ] Stable Sort
- [ ] Nested Group Test

## 2D-4: Atlas / Batching / Culling

- [ ] Atlas Packing
- [ ] Atlas Revision
- [ ] Instance Buffer
- [ ] Draw Instancing
- [ ] Sortingを壊さないBatch Builder
- [ ] World 2D AABB Culling
- [ ] Batch Break Telemetry

この段階を最初の実用可能ラインとする。

## 2D-5: Sliced / Tiled / Mask

- [ ] 9-Slice
- [ ] Tiled Sprite
- [ ] Sprite Mask
- [ ] Nested / Sorting Range
- [ ] Stencil / Mask Texture Lowering

## 2D-6: Tilemap

- [ ] Grid
- [ ] Tile Asset
- [ ] Tilemap Asset / Runtime Storage
- [ ] Chunk Renderer
- [ ] Animated Tile
- [ ] Tile Palette
- [ ] Undo / Redo

## 2D-7: SpriteShape

- [ ] Spline Asset / Component
- [ ] Tessellation
- [ ] Edge / Fill
- [ ] Geometry Revision
- [ ] Editor Tool

## 2D-8: 2D Lighting / Shadow

- [ ] Light2D Snapshot
- [ ] Blend Style
- [ ] Normal / Mask Map
- [ ] Light Culling
- [ ] Shadow Caster 2D
- [ ] Lit Sprite Material
- [ ] Pipeline Graph Node

## 2D-9: Animation

- [ ] Frame Animation Asset
- [ ] Animator連携
- [ ] Event Marker
- [ ] Skeletal 2D Asset Contract
- [ ] Skinning / Deform Runtime
- [ ] Bone / Weight Editor

## 2D-10: Canvas / Layout / Text / Input

- [ ] Canvas Modes
- [ ] Full RectTransform
- [ ] Layout System
- [ ] SDF Text
- [ ] Mask / Clip
- [ ] Hit Test / Focus / Navigation
- [ ] Existing HUD Migration

## 2D-11: Editor / Preview / Observability

- [ ] 2D Scene Mode
- [ ] Pixel Grid
- [ ] Sorting Debugger
- [ ] Sprite / Atlas / Tile Preview
- [ ] Tile Palette
- [ ] Light / Shadow Gizmo
- [ ] Draw Count / Batch Count / Overdraw計測
- [ ] Headless Capture対象追加

---

# 21. RenderPipeline Graphとの実装順

Step 18-HはRenderPipeline Graph完成後まで待たせない。

```text
Step 18-A Ownership / RenderWorld安定化
    ↓
RPG-0 Contract Alignment
    ↓
2D-0 Domain / Asset Contract
    ↓
RPG-1 Asset / Registry
    ↓
RPG-2 Compiler
    ↓
RPG-3 Operation Lowering
    ↓
2D-1 Sprite Import
2D-2 RHI Sprite Renderer
2D-3 Sorting
2D-4 Atlas / Batching / Culling
    ↓
RPG-7A Minimal Pipeline Instance
    ↓
Pixel Perfect Camera接続 / Resolve Node
    ↓
RPG-5 Legacy Adapter
    ↓
2D-5 Mask
2D-6 Tilemap
2D-7 SpriteShape
2D-8 Lighting / Shadow
    ↓
RPG-8 Editor Integration
    ↓
2D-9 Animation
2D-10 UI
2D-11 Editor / Preview
```

2D-0〜2D-4は、完全なRenderPipeline Graph Editorを待たず、Builtin 2D Pipelineと既存Pipeline Adapter上で実装可能にする。

---

# 22. CI / Smoke Test

追加する契約テスト:

- Sprite Asset Slice / Pivot / Border
- Legacy Texture Slice Migration
- Sorting Layer Stable Order
- Sorting Group Nested Order
- Sprite Batch Boundary
- Atlas Revision / Repack
- Sprite Instance Buffer
- Sprite Mask Range
- Tilemap Chunk Dirty Update
- Tilemap Chunk Culling
- SpriteShape Geometry Revision
- Light2D Culling / Blend Style
- Shadow Caster 2D Shape Revision
- Canvas RectTransform
- Layout Determinism
- Text Glyph Fallback
- Pixel Perfect Reference Resolution Contract
- Pixel Perfect Camera Quantization
- Pixel Perfect Resolve Integer Scale
- Resize / Scene Reload / Asset Reload Lifetime

実機確認:

- 16:9 / 16:10 / 4:3 / Ultrawide
- Window Resize
- Integer / Non-integer Scale
- Nearest FilterでのPixel Bleed
- Atlas境界Bleed
- Sorting Group Nested
- Mask境界
- Tilemap大規模Scene
- Light2D多数
- Screen UIとWorld 2D混在
- 3D Pipelineとの混在

---

# 23. 完了定義

最低限のUnity相当2D制作基盤として、次を満たす。

- [ ] Textureから複数SpriteをEditorで切り出せる
- [ ] Pivot / Border / PPUを保存できる
- [ ] World-space SpriteをRHI経由で描画できる
- [ ] Sorting Layer / Order / Sorting Groupが安定動作する
- [ ] AtlasとInstancingで大量SpriteをBatchできる
- [ ] Simple / Sliced / Tiled / Flipを使用できる
- [ ] Sprite Maskを使用できる
- [ ] TilemapをPaletteから編集しChunk描画できる
- [ ] SpriteShapeを編集・描画できる
- [ ] 2D Light / Normal / Shadowを使用できる
- [ ] Frame AnimationをAssetとして再生できる
- [ ] Canvas / RectTransform / Layout / Text / Inputを使用できる
- [ ] Editor View / Game View / Previewで同じAsset契約を利用する
- [ ] Pixel Perfect Cameraと最終Resolveの責務が分離される
- [ ] 2D RendererがNative D3D11型やComponent Pointerへ依存しない
- [ ] Resize / Reload / Pipeline切替でRuntime Resourceがリークしない

Skeletal 2D、高度なRule Tile、PSD / Aseprite Importは、この基盤上の追加Packageまたは後続工程として実装可能な状態を完了条件とする。
