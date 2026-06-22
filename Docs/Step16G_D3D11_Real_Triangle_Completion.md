# Step 16-G: D3D11 Real Triangle Completion

## 完了内容

D3D11 RHIの公開契約だけを使ってSwapChainへ三角形を描画し、中央PixelをReadbackして実描画を検証するSmoke Testを追加した。

## 検証経路

1. 非表示Win32 Windowを生成
2. `D3D11RHIBackend`からDevice / SwapChainを生成
3. HLSL Vertex / Pixel Shaderを`D3DCompile`で生成
4. RHI経由でShader、Vertex Buffer、Pipeline、RTVを生成
5. Present -> RenderTarget Barrier
6. RenderPass Clear、Pipeline / Vertex Buffer Bind、Draw(3)
7. RenderTarget -> Present Barrier
8. Graphics QueueへSubmitしてWaitIdle
9. 検証専用Native InteropでSwapChain ImageをStagingへCopy
10. 中央Pixelが赤であることを確認
11. Present

## Software Adapter契約

- RuntimeではSoftware Adapterを引き続き既定で拒否する
- `DeviceCreateDesc::allowSoftwareAdapter`を明示したTest / Validationだけ許可する
- Hardware Adapterが存在する場合はHardwareを優先する
- Headless CIでは列挙されたSoftware Adapterを利用可能にする

## CI方針

- Pull RequestではTestのコンパイルだけを行う
- 実描画を伴うRunは`workflow_dispatch`に限定する
- 今回の導入時には一度だけ専用検証Workflowで実行し、Pixel Readbackまで成功したことを確認する
