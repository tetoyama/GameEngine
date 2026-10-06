# Portable rendering

## Scope

既存RHIにSDL GPU Backendを追加し、共通の描画処理をD3D12 / Vulkan / Metalで実行する。
新しいEngine / Scene / Asset管理の抽象階層は追加しない。

`GameEnginePortable` は既存EngineContext、TimeService、EntityRegistry、JobSystem、RenderPacket、RHIService、RenderGraphを
つないだ移植用ランタイム兼描画検証ホスト。既存Win32 Editorの全機能を含む実行ファイルではない。
通常の `GameEngine.sln` のD3D11経路も保持する。

既存Windows EditorのProject Settings → Application → Rendering APIからD3D12 / Vulkanを選択し、
Save Project Settingsして再起動すると、Editor View / Player Viewのシーン描画に選択APIを使う。
現在の実行APIと保存待ちの選択を別に表示する。MetalはmacOSランタイムで使う。

EditorのImGui / Win32 / Direct2D表示はD3D11のまま。選択APIで描いた最終RGBA画像を読み戻し、
D3D11表示Textureへ転送する移行用経路。同期Readbackの遅延があるため性能比較には使わない。
対応範囲は不透明Static Model、BaseColor Texture、既存TextureComponentのUV変換、
既存PBRの金属度 / 粗さ / 発光、Environment Map、Unlitと単一Directional Light。
既存ShaderとShadow / Lighting全体の完全な表現互換性は未達。
描画数、未対応Packet数とAnimation / Geometry未解決数を
Editor Viewに表示し、選択APIが失敗した時はD3D11へ暗黙に切り替えずエラーを表示する。
Editor Viewの旧Object-ID GBufferによるClick選択は無効化し、Hierarchy選択 / Gizmo編集を使う。
既存の全Pass・ゲーム内容の互換性が必要ならD3D11を選ぶ。

## Reused boundaries

- Backend登録とDevice所有: `RenderHardwareInterfaceService` / `BackendRegistry`
- Service所有 / 終了: 既存 `EngineContext`。Windows Builderと共通Lifecycleの実装を分離し、
  HeaderからWin32依存を除去。Renderer破棄後、登録の逆順にServiceを終了・破棄する
- Frame時間: 既存 `TimeService`。Windowsは従来のQPC、macOS / Linuxはmonotonicなsteady_clock。
  RuntimeのCamera操作も既存Delta Timeに従い、Frame Rateによる回転速度の違いを避ける
- ECSからの抽出結果: 既存 `RenderPacket`。API専用型を含まない所有Snapshotへコピー
- Geometry: 既存 `ModelGeometryRuntimeMesh` のVertex / Index Buffer Handleを直接利用
- Pass依存と論理状態遷移: 既存 `RenderGraph`
- GPU資源: 既存世代付きHandle / ResourcePool / Queue / Fence契約

モデルのImport / ReimportとGeometry所有は `ModelGeometryRuntimeStorage` の責務を維持する。
`ConvertRenderPackets` のResolverは既存Runtimeの選択済みSubMeshを返す。
Renderer側で別のModelData CacheやAsset Managerは作らない。
AssimpからCPU Geometryを抽出する処理は `ModelGeometryImport::ExtractMesh` に分離し、
既存ModelLoaderのDX11 Buffer生成とNative Runtimeの両方から使う。
Native Runtimeは既存Engineと同じAssimp 5.2.0を固定Hashで取得してStatic Linkする。
`--model path` で実ファイルのStatic Geometryを追加表示できる。この段階ではGeometryのみで、
ファイル内Material / Texture / Node hierarchy / Animationの完全な読み込みは未移行。
頂点色はChannel 0の各頂点から取得し、Blender変換後の頂点にBoundsを揃える。

Textureも既存ResourceService / TextureDataを再利用する。Windows Editorの移行用経路では、
既に読み込まれた2D SRVを初回だけRGBA8へ読み戻して選択RHIへ転送し、GPU Handleを
そのTextureDataが所有する。SRV置換 / Device置換時に再生成し、Device寿命はweak tokenで検査する。
別のTexture Cache / Loaderは作らない。この読み込みBridge自体はWindows Editor専用で、
API非依存FrameRendererにはTextureViewHandleを渡す。

Environment Mapは既存RenderSystemが選択したTextureDataを同じBridgeで渡す。
BRDFの純粋な計算は `Source/Shader/Material/BRDF.hlsli` を既存HLSLとPortable GLSLで共有する。
材質Snapshotには既存MaterialDescriptorの金属度 / 粗さ / AO / 発光 / ShadowとEnvironmentのFlagsをコピーする。
AOは既存PBR式と同様、現段階では照明計算に適用しない。

## Implemented frame

1. フレーム定数とインスタンスのUpload
2. Directional shadow (D32 depth、比較Sampler、5x5 PCF)
3. 5-target RGBA16F GBuffer (albedo / normal / world position / material / emissive) + depth
4. Deferred PBR directional lighting / Environment reflection / Emissive / Unlit
   （SceneのDirectional Lightの向き・色・Ambient・Shadow有効性を利用）
5. 最終出力変換。診断RuntimeはHDR tone mapping + gamma、既存Editor互換経路はLinear UNORM
6. Swapchain合成。最小化 / 非表示で取得画像がない場合は表示を省略

GPUへの定数PushはCommandごとの値を保存する。Instance Bufferの更新は描画と同じ
ordered queue上のCopyとして記録する。更新同期 / Buffer cyclingはBackendが所有し、
Renderer側には別の固定Frame Slot数や毎FrameのFence待機を置かない。
Instance Buffer拡張 / Resize / Geometry破棄 / Capture時は必要な完了を待つ。

既存のMaximum Frame Latency設定はWindows EditorのDXGI Swapchainの表示待ちFrame数を制限する。
今回この設定を新設・置換してはいない。SDLのWindow付き移植用Runtimeの表示待ちはSDL側が管理する。
Windows EditorのDX12 / Vulkan互換表示は毎Frame Captureするため、現時点でCPUとGPUの
Frame間並列性が改善したとは言えない。

DeviceはRendererより長く生存させる。資源作成・更新・Command記録はRender Threadで行う。
CommandBufferは取得したThreadで記録・Submit・破棄する。

Matrix契約はcolumn-major / column-vector、左手系、NDC depth [0,1]、Viewportは左上原点。
DirectXのrow-major / row-vector Snapshotは同じ16floatをコピーして変換できる。
法線はinverse-transposeで変換するため、非一様Scaleにも対応する。可逆なWorld Matrixを渡す。
VulkanのAPI固有座標差はSDL GPUが処理する。

## Build and run

CMake 3.24以上とC++20コンパイラが必要。SDL3 3.4以上を `find_package` で利用し、
未導入なら固定版3.4.18をSHA256検証付きで取得しStatic Linkする。
macOSではXcode Command Line Toolsを利用する。

Windows EditorのVisual Studioビルドは固定版SDL VC SDKをSHA256検証付きで取得し、
SDL3.dllを実行ファイル横へコピーする。取得先は無視対象の `build-portable-dependencies`。

```sh
cmake --preset portable
cmake --build --preset portable --parallel 4
ctest --preset portable
```

### Windows / macOS build target

Project Settings → BuildでWindows / macOSを選択できる。選択は既存EngineConfig.yamlの
`Build.Target`に保存する。Windowsでは同じ画面のBuild package on this computerで
別Processによるビルド・テスト・配布フォルダ生成を実行でき、ログは `Logs/Build/Windows.log`。
CMakeがPATHにない場合は同画面で実行ファイルの場所を指定する。

Native Host上の一括ビルド:

```sh
# Windows (Windows上)
cmake -DTARGET_PLATFORM=Windows -P cmake/BuildPortable.cmake
# macOS (Mac上)
cmake -DTARGET_PLATFORM=macOS -P cmake/BuildPortable.cmake
```

成果物は `portable-runtime/<Windows|macOS>/Release/bin`。
OSごとのConfigure / Build / Test Presetも `windows` / `macos` として用意する。
Hostと対象OSが異なる場合は生成前に失敗させる。WindowsからMac用Binaryを直接生成する
Cross Compile Toolchainを追加したという意味ではない。

GitHub ActionsのWindows Buildを手動起動し、`target_platform`をWindows / macOS / Linux / allから
選ぶと、対象OSのRunnerでビルドし `portable-runtime-<OS>` Artifactを生成する。
macOS選択時に `portable_gpu=true` とするとMetal実行・GPU読み戻しも検証する。

このビルド対象は移植用描画ランタイム。既存プロジェクトのScene / Script / PhysX / Assetを
自動で梱包するゲームExporterや、Mac版Editorのビルドではない。

Windowsでは `build-portable/Release/GameEnginePortable.exe`、macOS / Linuxでは
`build-portable/GameEnginePortable` が生成される。

```sh
# Windows
GameEnginePortable.exe --backend d3d12
GameEnginePortable.exe --backend vulkan
# macOS: Metalを既定選択
./GameEnginePortable --backend metal
./GameEnginePortable --backend metal --model /path/to/cube.obj
# Linux
./GameEnginePortable --backend vulkan
```

左右ArrowでCameraを回転し、Escapeで終了する。既定の診断Sceneは複数Blockで構成した機体。

`--frames N`、`--width W --height H`、`--capture output.ppm` を診断に使える。
Captureは実際の最終Render Textureから読み戻す。SDLのWindowサイズとGPU画像サイズは分け、
High-DPI表示では取得した画像のPixel寸法を使う。

## Shaders

`Asset/Shader/Portable/` のGLSLと既存Shader側の共有BRDF Headerを手書きSourceとする。
glslcでSPIR-Vを生成し、SPIRV-CrossでHLSL / MSLへ変換し、DXCでDXILを生成する。
生成物を同梱するため、通常ビルドやMac起動時にDXC / Vulkan SDKは不要。
Metalでは同梱MSLをDevice作成時にCompileする。

Shader Toolが検出される環境では次のTargetで全形式を更新できる。

```sh
cmake --build build-portable --target PortableShaders
```

GLSLのdescriptor set / bindingとSDLのstage別Binding規約を合わせる。
MSLは `--msl-decoration-binding` で同じSlotを維持する。Resource数は `ShaderDesc` に明示する。
Graphics uniform setはVertex=1、Fragment=3。Sampler setはVertex=0、Fragment=2。
ComputeはReadonly=0、ReadWrite=1、Uniform=2。MetalのEntry Pointは `main0`。

## Validation

CPU契約は通常CTestで検証する。実GPU検証は明示的に有効化する。
Service重複登録時の所有権、依存Serviceを参照できる逆順Shutdown / 破棄、二重Shutdown、
時計の進行、Fixed Update、Draw区間計測と再初期化を共通Lifecycle Testで検査する。
Model Importは頂点色Channel、UV、Blender変換、三角形 / Index範囲、失敗時のSnapshot保持、
Assimp Scene解放後のGeometry所有と既存cube.objの読み込みをCPU Testで検査する。

```sh
cmake -S . -B build-portable -DGAMEENGINE_GPU_TESTS=ON
cmake --build build-portable --config Debug --parallel 4
ctest --test-dir build-portable -C Debug --output-on-failure
```

GPU検証は、画像の変化を使ってGeometry、Material、Shadow、Resize、同一Frameの再利用を検査する。
Texture、UV変換、金属度、粗さ、Environment Map、発光、Unlit切り替えも実際のGPU画像の変化で検査する。
Linear出力はUnlitの既知色（0.25 / 0.5 / 0.75）がUNORMの64 / 128 / 191として
出力されることを検査し、意図しないTone Mapping / Gamma変換を検出する。
Compute書き込み / 非整列Texture幅のReadback、Shader破棄後のPipeline利用、
Submit後のCommand wrapper破棄、Fence / Device寿命、古いHandleと異なるThreadの拒否も検査する。

WindowsでD3D12 / Vulkanの描画とCompute契約が通過。
既存Editorの非表示起動でも両APIの選択とEditor Viewへの描画を確認した。
Editor表示用のD3D11 Texture転送は、画像変化とResizeを使うGPU Testでも通過。
Window付き表示とOffscreen経路を検証し、同じSceneの画像差も比較する。
macOS-14 Runnerでビルド / 共通処理テスト / 梱包が成功。
2026-10-06の[配布物からのMetal実行検証](https://github.com/tetoyama/GameEngine/actions/runs/37405549367)で、
共通描画・GPU読み戻し・Shadow / Geometry / Material / Texture / UV / Roughness / Metallic /
Environment / Emissive / Unlit / Linear出力・Resizeの検査が通過した。
OffscreenとWindow付きの両方で実行し、最後のScene画像は全Byte一致した。
Window付き実行では最終FrameのPass数が4から5へ増え、Swapchainへの合成・提出も実行されている。
これはMac CI Runner上の検証。手元の物理Macでの操作や既存Editor全体の移植を確認した結果ではない。
同じ診断SceneをWindows D3D12 / VulkanとMac Metalで比較したところ、
D3D12 / Metalの平均絶対RGB差は0.03882 / 255（480×320、差が2を超えるPixelは2）だった。
診断SceneでのPixel完全一致は成立していない。これは `_scene.scene` の比較とは別の検証。
再検証はWorkflowの手動入力 `portable_gpu`、またはMacで上のGPU Testを実行する。
PBR接続のCode Commit `18f34613` の[各OS / 既存Windows Editor CI](https://github.com/tetoyama/GameEngine/actions/runs/37405524694)
と[Core Smoke](https://github.com/tetoyama/GameEngine/actions/runs/37405524962)も成功した。

## Current limits

2026-10-06に同じ `_scene.scene` Snapshot、Camera、1280×720、Stopped / dt=0でEditor Viewを比較した。
D3D12とVulkanの平均絶対RGB差は0.00004087 / 255で、チャンネル差が2を超えるPixelは2 / 921600。
各APIの1 Frame目と3 Frame目は一致した。
D3D11との平均絶対RGB差は5.18 / 255で、一致していない。チャンネル差が2を超えるPixelは23.09%。
PBR / Environment接続前の26.76 / 255から減少し、共有BRDFへの変更前後のD3D11画像は全Pixel一致した。
既存Editor CameraはPost Effectが空のため、共通経路でもTone Mapping / Gamma変換を適用しない。
床と空のTexture、既存PBRのスカラー材質 / Environment Mapが表示されるが、
単一Shadow Mapと既存CSMとの差、未対応Toon / Custom Material等の差が残る。
未対応Materialも14 Packet残る。Drawsは36、未解決Geometry / Animationは0。
この比較はMac上で既存Sceneを描いた結果ではない。

- 既存Editor / Win32 Engine entry / Direct2D runtime text / PhysX / EffekseerのMac移植は完了していない
- 共通FrameRendererはOpaque Model Geometry、BaseColor Texture、UV変換、既存PBRのスカラー材質、Environment Map、発光、Unlitを扱う。
  Normal / Metallic / Roughness等のTexture、Toon / Custom Shader、Skinning、Billboard、
  Terrain / Wave / Particle、Transparency、既存CSM / Local Light / Post Effect Nodeの全機能は未移行
- SceneのDirectional Lightを1つ使用する。Directional CSMも照明方向は利用するがShadow Mapは1枚で、Cascade設定の移植は未完了
- 未対応Packet / Materialは変換結果の件数で明示し、別の描画で代用しない
- SDL GPUの単一ordered queueを使う。Async Compute / 複数Native Queue / GPU Timeline同期は広告しない
- Graphics Storage Texture、Textureの一部範囲SRV、MSAA、Mip生成は現在拒否する
- SDLの既定Adapterを使う。Native Adapterの独自列挙は追加しない
- Swapchain画像はCommandBufferごとに明示取得し、Submit時にSDLがPresentする

この段階で成立するのは「API非依存の描画と既存Coreを含むRuntimeを複数OSへBuildできる基盤」。
既存Editor全体がMacで動くという意味ではない。

## Dependency

SDL3: https://github.com/libsdl-org/SDL (zlib license、`ThirdParty/SDL3-LICENSE.txt`)

GPUの公式契約: https://wiki.libsdl.org/SDL3/CategoryGPU
