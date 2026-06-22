from queue_sync_v2_common import load, replace_once, save

plan_path = "Docs/ECS_Scheduler_Migration_Plan.md"
plan = load(plan_path)
legacy_result = """1. Step 16-FのPass Culling
2. D3D11最小実描画Triangle Test
3. 既存Player View実機回帰
4. Step 16完了報告
5. Step 17 RenderWorld移行
"""
current_result = """1. Step 16-FのPass Culling
2. D3D11最小実描画Triangle Test
3. 既存Player View実機回帰
4. Step 17-B Render Packet基盤
5. Step 17-C Animation CPU Build / GPU Upload分離
6. Step 17-D Terrain CPU Mesh Build / GPU Upload分離
7. Step 17-E Wave CPU Vertex Build / GPU Upload分離
"""
plan = replace_once(
    plan,
    legacy_result,
    current_result,
    "restore current work position after base migration",
)
plan = replace_once(
    plan,
    "- `Docs/Step16F_Transient_Resource_Lifetime_Completion.md`\n",
    "- `Docs/Step16F_Transient_Resource_Lifetime_Completion.md`\n"
    "- `Docs/Step16F_Queue_Synchronization_Completion.md`\n",
    "queue synchronization completion document link",
)
save(plan_path, plan)

path = "Docs/Step16F_Queue_Synchronization_Completion.md"
text = load(path)
text = replace_once(
    text,
    "## D3D11契約\n",
    """## Resource Queue Sharing契約

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
""",
    "queue sharing and execution lifetime completion sections",
)
text = replace_once(
    text,
    "通常CIではNull Backendを使う複数Queue依存Contract Testをコンパイルする。実行を伴うTestは手動`workflow_dispatch`に限定する。\n",
    """通常CIでは次のContract Testをコンパイルする。

- Null Backendによる複数Queue依存
- Exclusive ResourceのCross-Queue拒否
- Concurrent ResourceのCross-Queue許可
- Timeline Fenceの複数Execute再利用
- D3D11 Backend Header / Contract

実行を伴うTestは手動`workflow_dispatch`に限定する。
""",
    "queue synchronization verification contract",
)
save(path, text)

path = "Docs/Step16_RHI_MultiBackend_Architecture.md"
text = load(path)
text = replace_once(
    text,
    """D3D11 Backendは内部でImmediate Contextへ変換する。
D3D12 / Vulkan BackendはNative Command Queue / Command Bufferへ変換する。
""",
    """D3D11 Backendは内部でImmediate Contextへ変換する。
D3D12 / Vulkan BackendはNative Command Queue / Command Bufferへ変換する。

Queue間依存はTimeline FenceのWait / Signalとして表現する。複数Queueから参照するResourceは`ResourceQueueSharingMode::Concurrent`を明示し、Vulkan Backendは必要なQueue FamilyのConcurrent Sharingへ変換する。`Exclusive` ResourceのCross-Queue利用はRenderGraph Compile時に拒否する。

`IRHICommandQueue::Submit()`後、呼び出し側はCommand List Wrapperを破棄できる。GPU完了まで必要なNative Command Allocator / Command Listの寿命保持はBackendの責務とする。RenderGraphのQueue FenceはExecute間で再利用し、明示的なExecution State解放時だけWaitIdle後に破棄する。
""",
    "multi-backend queue synchronization architecture",
)
save(path, text)

print("Queue sync documentation updated")
