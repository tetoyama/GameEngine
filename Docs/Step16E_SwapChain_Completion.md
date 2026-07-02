# Step 16-E SwapChain Completion

## 状態

**完了**

## 目的

SwapChainをNative API Objectとして直接扱わず、RenderGraphとRendererが世代付きTexture Handle、Queue、同期契約だけでBack Bufferを扱えるようにする。

## 公開契約

`IRHISwapChain`は次を公開する。

- SwapChain Image数
- Current Image Index
- Image Indexから`TextureHandle`を取得
- Current Imageの`TextureHandle`を取得
- Presentを担当するQueue種別
- Resize
- SwapChain Descriptor

`IRHIDevice`は次の安全な標準経路を提供する。

- `PresentSwapChain()`
- `ResizeSwapChain()`

## Present契約

- Presentは`IRHICommandQueue::Present()`を経由する
- SwapChainが要求するQueue種別と異なるQueueからのPresentを拒否する
- D3D11はGraphics論理QueueからNative SwapChainへPresentする
- Null Backendも同じ契約を実装する
- Renderer上位層はNative Present APIを呼ばない

## Image Handle契約

- SwapChain Imageは通常Textureと同じ世代付き`TextureHandle`で公開する
- SwapChain所有Textureを外部から`DestroyTexture()`できない
- Texture Viewは通常Resourceと同じ経路で生成する
- Resize成功後、旧Image Handleはgeneration不一致により失効する
- Resize後は新しいImage HandleをSwapChainから取得し直す

## Resize同期契約

1. `IRHIDevice::ResizeSwapChain()`がDevice Idleを待つ
2. SwapChain Imageに生存中のViewがある場合はResizeを拒否する
3. D3D11 Immediate ContextのRender Target参照を解除する
4. 旧Image HandleとNative参照を破棄する
5. Native SwapChainをResizeする
6. 新しいBack BufferをTexture PoolへImportする
7. 新しいgenerationを持つImage Handleを公開する

D3D11 SwapChain側でもIdle待機を行い、直接`IRHISwapChain::Resize()`が呼ばれた場合にもGPU使用中のBufferを解放しない。

## D3D11 Backend

- Blt Modelは公開Image数を1とする
- Flip ModelはNative Buffer数分のImage Handleを公開する
- `IDXGISwapChain3`が利用可能な場合はCurrent Back Buffer Indexを取得する
- Import Textureの初期Stateは`ResourceState::Present`
- Import Textureは`swapChainOwned=true`として外部破棄を拒否する

## Null Backend

- Resource Pool上にSwapChain Imageを生成する
- Resize時に旧Handleを破棄して新Handleを生成する
- View生存中のResize拒否をD3D11と同じ規則で検証する
- Graphics Queue以外からのPresentを拒否する

## 検証

`Tests/RHISwapChainContractSmokeTest.cpp`で次を検証する。

- Null / D3D11 WARPでImage Handleを取得できる
- SwapChain Imageを外部破棄できない
- View生存中はResizeできない
- View破棄後はResizeできる
- Resize後に旧Handleが失効する
- 新HandleのWidth / Heightが更新される
- Compute QueueからPresentできない
- Graphics Queue経由Presentが成功する

検証結果:

- Migration Plan Validation run #195: success
- RHI Smoke Test run #244: success
- Windows Build run #764: success

## 完了条件

- SwapChain ImageがAPI非依存Handleで参照できる
- PresentがQueue契約へ統合される
- Resize前のGPU同期とResource寿命が明文化・実装される
- D3D11とNull Backendが同一契約を満たす
- Debug / Release x64回帰がない
