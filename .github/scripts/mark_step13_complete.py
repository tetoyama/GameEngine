from pathlib import Path

path = Path("Docs/ECS_Scheduler_Migration_Plan.md")
text = path.read_text(encoding="utf-8-sig")

old = """### Step 13: Job System

- [ ] 常駐Worker
- [ ] Work Stealing
- [ ] Job Counter / Fence
- [ ] ParallelFor
- [ ] Thread-local Command Buffer
- [ ] Thread-local Scratch Allocator

"""

new = """### Step 13: Job System

状態: **完了**

- [x] 常駐Worker
- [x] WorkerごとのDequeとWork Stealing
- [x] Job Counter / Fence
- [x] Nested Wait中のJob実行支援
- [x] ParallelFor
- [x] Worker-local Command Buffer
- [x] Marker / Rewind対応Scratch Allocator
- [x] Job例外のFence / WaitIdle伝播
- [x] SystemRegistryによる起動・停止の寿命管理
- [x] 停止時に全JobをDrainしてからWorkerをJoin
- [x] JobSystem Smoke TestをWindows CIへ追加

Schedule自体はStep 14まで直列実行を維持する。Worker-local Command BufferからECS EntityCommandBufferへの統合はParallel Executor側で行う。

検証: Windows Build run #383でJob System Smoke Test、Script / GameEngineのDebug・Release x64が成功。

"""

if text.count(old) != 1:
    raise RuntimeError("Step 13 block mismatch")

path.write_text(text.replace(old, new, 1), encoding="utf-8")
