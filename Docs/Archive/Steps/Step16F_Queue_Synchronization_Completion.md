# Step 16-F: Queue Synchronization Completion

## 完了内容

RenderGraph PassへGraphics / Compute / CopyのQueue Domainを追加し、Hazard依存からQueue間Fence同期計画を自動生成する仕組みを追加した。

- 同一Queueの依存はQueue内の提出順で保証する
- 異なるQueueへ値を渡すProducer PassはQueue固有Fence ValueをSignalする
- Consumer Passは依存元Queueごとの最新Fence ValueをWaitする
- 複数Queueへ依存するPassは複数Fence Waitを一度のSubmitで宣言できる
- RenderGraphがDeviceからCommand ListとFenceを生成し、Pass単位でQueueへSubmitする
- 既存の単一Command List実行は単一Queue Graphだけに制限する

## Resource Queue Sharing契約

- Logical / Transient Resourceは既定で`Concurrent`
- Descriptorを伴わないImport Resourceは安全側の`Exclusive`
- `BufferDesc` / `TextureDesc`で`ResourceQueueSharingMode::Concurrent`を明示可能
- `Exclusive` Resourceを複数Queue Domainから使用したGraphはCompile時に拒否
- Vulkan Backendは`Concurrent`を必要なQueue FamilyのConcurrent Sharingへ変換する
- D3D11 / D3D12 Backendは同じ公開契約を保持し、API名による上位分岐を行わない

## Fence / Command Lifetime契約

- RenderGraphはQueueごとのTimeline FenceをExecution Stateとして再利用する
- Fence ValueはExecuteを跨いで単調増加する
- `Execute()`ごとのFence新規生成は禁止し、Device Fence Poolの無制限増加を防ぐ
- `ReleaseExecutionState()`は`WaitIdle()`後に`DestroyFence()`で明示解放する
- `Submit()`後のNative Command Allocator / Command List保持はBackendの責務とする

## D3D11契約

D3D11のGraphics / Compute / Copyは論理Queueであり、Immediate Contextへ直列化する。Queue間WaitはCPU側でEVENT Query完了を待ち、SignalはEVENT Queryを挿入する。`supportsMultipleCommandQueues`と`supportsAsyncCompute`はfalseのままで、Native並列実行を偽装しない。

## 将来Backend契約

D3D12 / Vulkan Backendは`QueueSubmitDesc::waits`と`signals`をNative Queue同期へ変換する。FenceはQueueごとに単調増加させるため、異なるQueueで同じValueを使用しても相互に干渉しない。

## 検証方針

通常CIでは次のContract Testをコンパイルする。

- Null Backendによる複数Queue依存
- Exclusive ResourceのCross-Queue拒否
- Concurrent ResourceのCross-Queue許可
- Timeline Fenceの複数Execute再利用
- D3D11 Backend Header / Contract

実行を伴うTestは手動`workflow_dispatch`に限定する。
