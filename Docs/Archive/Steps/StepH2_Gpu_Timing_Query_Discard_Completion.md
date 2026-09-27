# H2 Device Lost / GPU Timing Query Discard Completion

## 状態

**Phase 2-A コード完了（2026-07-10）**

H2 Device Lost完全復帰の前段として、Step 19-A.1で要求していたResize / Device Lost時のPending GPU Timestamp Query破棄を描画Lifecycleへ接続した。

## 対応内容

### Resize

`MainRenderer::OnResize`でSwapChain / D2D Resourceを変更する前に`GpuPassTimingProfiler::Reset()`を呼ぶ。

これにより次を保証する。

- 旧Render Target構成に属するPending Queryを次Frameへ持ち越さない
- `GetData`完了待ちを行わない
- Query Ring上書きやResize前後の計測混在を防ぐ
- 次Frameの`BeginFrame`で現在Device向けQuery Poolを再生成する

### ResizeBuffers Device Lost

`GraphicsContext::Resize`後に`IsDeviceLost()`を確認する。

Device Lost時は次を行う。

- D2D Resourceを失われたDevice上で再生成しない
- `PostQuitMessage(-1)`で同じ`PollEvents`ループへ`WM_QUIT`を投入する
- `Engine::Run`が次の描画へ入る前に終了条件へ到達する

### Present Device Lost

`MainRenderer::EndFrame`で`Present`後に`IsDeviceLost()`を確認する。

Device Lost時は`GpuPassTimingProfiler::Reset()`を呼び、現在Frameを含む未回収Queryを解放する。

`Engine::Run`既存のH2判定がそのFrame末尾でGraceful終了する。

## 変更ファイル

- `Source/GameApplication/Service/Graphics/mainRenderer.h`
- `Source/GameApplication/Service/Graphics/mainRenderer.cpp`

## 契約

```text
Resize requested
    -> discard pending GPU timing queries
    -> release D2D resize resources
    -> ResizeBuffers
    -> if device lost:
           do not recreate D2D resources
           enqueue WM_QUIT
           stop before next render
       else:
           recreate D2D resources

Present
    -> if device lost:
           discard pending GPU timing queries
           Engine graceful exit
```

## 残作業

H2 Phase 2-Bとして次が残る。

- Device / Immediate Context再生成
- SwapChain再生成
- RTV / DSV / SRV / UAV再生成
- D2D / ImGui / Renderer Resource再生成
- Timestamp Query Pool再生成後の計測再開
- RHI D3D11 Device再Binding
- Scene / ResourceService所有GPU Resourceの再Upload
- TDR実機試験

本対応は完全復帰ではなく、Device Lost時のQuery Lifetime破損とResize直後の無効Device参照を防ぐGraceful終了強化である。
