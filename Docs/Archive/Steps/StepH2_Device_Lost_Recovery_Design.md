# H2 Phase 2: Device Lost Recovery Design

## 状態

**設計（2026-07-11）。実装は工程2a-1のシミュレーションフックのみ着手済み**

`Docs/ECS_Scheduler_Migration_Plan.md` §2.5 H2の残工程
「Device / SwapChain / 全View / Query Pool完全再生成による復帰」の設計文書。

## 前提（Phase 1 実装済み）

- `Present` / `ResizeBuffers`のHRESULTを`HandleDeviceLostHResult`で判定
- `DXGI_ERROR_DEVICE_REMOVED / RESET`検出時に`GetDeviceRemovedReason`をログし
  `m_DeviceLost`を立てる
- `Engine::Run`が`IsDeviceLost()`でメインループをbreakしGraceful終了

## D3D11の制約

`DEVICE_REMOVED`後、旧`ID3D11Device`と**全ての子リソースは恒久的に無効**。
復帰 = 新Device生成 + 全GPUリソースの再生成。CPU側状態（ECS / Scene / 物理 /
Script）は保持できる。よって問題の本質は「GPUリソース保持箇所の網羅」にある。

## 再生成対象インベントリ（2026-07-11 コード全域grep）

### A. コアコンテキスト
- `Service/Graphics/graphicsContext.*` — Device / DeviceContext / SwapChain /
  BackBuffer RTV・DSV / CB b0-b2 / Blend・Depth・Rasterizer States / Sampler /
  FullScreen VB・IB / GPU Timing Query Set / 内部Buffer・RTV・SRV / csSkinning
- `Service/Graphics/D2DRenderer.*` — D2D1Factory / DWrite / D3D11 Interop
- `Service/DebugTools/ImGuiSystem.cpp` — `ImGui_ImplDX11`バックエンド
- `Service/Graphics/GpuPassTimingProfiler.h` — Pass別Timestamp Query Pool

### B. RHI層（ResourcePool中央管理）
- `Service/Graphics/RHI/D3D11/` — `D3D11RHIDevice`のResourcePool配下:
  Buffer / BufferView / Texture / TextureView / Sampler / Shader / Pipeline /
  SwapChain(Image)Runtime / GraphicsContextInterop

### C. RenderPass / RenderTarget / StaticBatch
- `RenderTarget/renderTarget.*` — 全RenderTarget（tex / rtv / srv / dsv）
- `GBufferPass` / `LightingPass` / `ShadowMapPass` / `PhysXDebugPass` —
  Pass固有RT・State・Shader
- `StaticBatch/StaticBatchGpuInstanceBuffer.h` / `StaticBatchD3D11GBufferTargetBinding.h` /
  `StaticBatchShadowPixelState.h` — Instance Buffer / Target Binding / Pixel State

### D. アセット / Component保持（※Step 18-Aで除去予定の箇所を含む）
- `Resources/Loader/textureLoader.h`（TextureData） /
  `modelLoader.h`・`modelData.cpp`（Mesh VB・IB） /
  `shaderLoader.h`・`vertexShaderData.h`・`pixelShaderData.h`（Shader + InputLayout）
- `Component/cameraComponent.h` / `meshRendererComponent.h` /
  `Operations/CameraPostEffectRuntime.h` / `Operations/ModelRendererRuntime.h`
- `Terrain/terrainSystem.h` / `waveSystem.h`（動的VB）
- `Renderable/Sprite・BillBoard・Particle`（内部バッファ）

計 約35ファイル・5レイヤーに分散。

## 設計方針の比較

- **案1: In-place個別Recreate** — 全保持箇所へRecreate APIを追加。
  分散が大きく網羅漏れ＝Crashのリスクが高い。Component内保持（Step 18-Aで
  除去予定）へAPIを生やすのは二重投資。**不採用**
- **案2: Renderer層の完全再起動（推奨 / Phase 2a）** — 既存の
  Finalize / Initialize経路とResource ServiceのUnloadを再利用し、
  「Rendererを丸ごと落として立ち上げ直す」。CPU側Scene状態は維持。
  新規APIが最小で、既存の終了経路の品質をそのまま網羅性として使える
- **案3: ResourcePool中心の再生成（Phase 2b / 最終形）** — Step 18-A
  RenderWorld + RHI Handle移行後、GPUリソースがResourcePoolへ集約された
  時点でPool一括再生成へ置き換える。案2はその踏み台になる

## Phase 2a 復帰シーケンス（案2）

`Engine::Run`のDevice Lost検出点（現在はbreak）で、終了ではなく
`RendererRestartOrchestrator`（新設）を呼ぶ:

1. Frame / Render Scheduleの実行完了を待つ（Schedule外の安全点へ）
2. Play中なら`SceneManagerState::Stopped`へ遷移（初期仕様。復帰後は停止状態）
3. ImGui `ImGui_ImplDX11_Shutdown` / D2D Interop解放
4. 全RenderPass / RenderTarget / StaticBatch GPU資源のFinalize
5. Resource ServiceのGPU依存アセット（Texture / Model / Shader）を全Unload
   し、Component側の生ポインタ参照を無効化（`TextureComponent::m_TextureData`等は
   shared_ptr再Loadで解決。生ID3D11ポインタ保持箇所はnullptr化を明示確認）
6. RHI ResourcePool Release / GpuPassTimingProfiler Query Pool破棄
   （Step 19-A.1のPending Query破棄契約と統合）
7. `GraphicsContext`再生成: Device / SwapChain / States / CB
   （`kConstantBufferUploadStrategy`と同一ポリシーで生成）
8. RHI再初期化 → Pass Initialize → ImGui / D2D再初期化
9. アセットは参照時の遅延Loadに任せる（即時全Reloadはしない）
10. `m_DeviceLost`クリア、メインループ再開

**フォールバック**: 復帰試行が連続N回（初期値2）失敗、または再生成中に再度
DEVICE_REMOVEDが発生した場合はPhase 1のGraceful終了へ落とす。

## 検証計画

- **シミュレーションフック**（工程2a-1・実装済み）:
  `GraphicsContext::MarkDeviceLostForTest()`が`HandleDeviceLostHResult`を
  `DXGI_ERROR_DEVICE_REMOVED`で呼ぶ。実デバイスは無効化されないため、
  検出経路とオーケストレーション順序の検証に限定して使用する
- **実TDR**: `dxcap -forcetdr`（Windows SDK）でGPUリセットを強制発火。
  リソース無効化を伴う本番検証はこちらでのみ可能
- シナリオ回帰: ドライバ更新 / RDP切替 / ノートPCのGPU切替

## 工程分割

- [x] 2a-1 シミュレーションフック（`MarkDeviceLostForTest`）
- [ ] 2a-2 Pass / RT / StaticBatchのFinalize→Initialize往復の冪等性監査
      （二重Finalize安全・再Initialize時のリーク無しをフックで確認）
- [ ] 2a-3 Resource ServiceのGPUアセットUnload→遅延再Load経路
- [ ] 2a-4 `RendererRestartOrchestrator`実装と`Engine::Run`への組込み
- [ ] 2a-5 ImGui / D2D再初期化
- [ ] 2a-6 `dxcap -forcetdr`実機回帰（TDR中Play / Stopped両状態）
- [ ] 2b ResourcePool中心再生成へ移行（Step 18-A完了後）

## 備考

- Component内のGPU資源保持（インベントリD群）はStep 18-A「Camera /
  ModelRendererからNative描画資源所有権を分離」で根治される。2aでは
  Unload時の参照無効化で暫定対応し、2bで再生成対象から消える
- 復帰後の見た目は「アセット遅延Load により数フレームかけて戻る」を許容する
