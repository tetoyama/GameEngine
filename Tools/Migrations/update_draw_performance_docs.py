from pathlib import Path


def load(path: str) -> str:
    return Path(path).read_text(encoding="utf-8-sig")


def save(path: str, text: str) -> None:
    Path(path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


main_plan_path = "Docs/ECS_Scheduler_Migration_Plan.md"
main_plan = load(main_plan_path)
main_plan = replace_once(
    main_plan,
    """優先順:

1. SystemTask命名規則の統一
2. Render Packet基盤
3. Animation CPU Build / GPU Upload分離
4. Terrain CPU Mesh Build / GPU Upload分離
5. Wave CPU Vertex Build / GPU Upload分離
6. Physics Begin–Fetch待機隠蔽の再評価
""",
    """優先順:

1. SystemTask命名規則の統一
2. Render Packet基盤
3. Draw Performance計測内訳化
4. IRenderable内部のComponentRegistry参照除去
5. Animation CPU Build / GPU Upload分離
6. Terrain CPU Mesh Build / GPU Upload分離
7. Wave CPU Vertex Build / GPU Upload分離
8. Physics Begin–Fetch待機隠蔽の再評価
""",
    "main Step 17 priority",
)
main_plan = replace_once(
    main_plan,
    """- `Docs/Step17_Task_Naming_And_Render_Task_Decomposition_Plan.md`
- `Docs/Step17A_Task_Naming_Completion.md`
""",
    """- `Docs/Step17_Task_Naming_And_Render_Task_Decomposition_Plan.md`
- `Docs/Step17A_Task_Naming_Completion.md`
- `Docs/Step17B_Draw_Performance_Breakdown.md`
""",
    "main Step 17 details",
)
main_plan = replace_once(
    main_plan,
    """# 3. 現在の作業位置

1. Step 17-B CaptureでPacket Build / Command Submit分離確認
2. Step 17-C Animation CPU Build / GPU Upload分離
3. Step 17-D Terrain CPU Mesh Build / GPU Upload分離
4. Step 17-E Wave CPU Vertex Build / GPU Upload分離

Step 17-AのTask命名統一は完了。以降のCapture、Profiler、YAML Export、依存解析では統一後のTask名を基準とする。
Step 17-B以降は、既存Player View回帰を維持しながら段階的に進める。
""",
    """# 3. 現在の作業位置

1. Step 17-B.1 Performance MonitorでDraw CPU内訳を実機確認
2. VSync ON / OFF比較とGPU Frame Time計測
3. IRenderable内部のComponentRegistry参照除去
4. Step 17-C Animation CPU Build / GPU Upload分離
5. Step 17-D Terrain CPU Mesh Build / GPU Upload分離
6. Step 17-E Wave CPU Vertex Build / GPU Upload分離

Step 17-AのTask命名統一は完了。以降のCapture、Profiler、YAML Export、依存解析では統一後のTask名を基準とする。
Step 17-BのPacket Build / Command Submit分離はSchedule Captureで確認済み。Performance MonitorのDraw全体とRender Scheduleの差をCPU区間、Present待機、GPU時間へ分解してから次の最適化対象を決定する。
""",
    "main current position",
)
save(main_plan_path, main_plan)


step_plan_path = "Docs/Step17_Task_Naming_And_Render_Task_Decomposition_Plan.md"
step_plan = load(step_plan_path)
step_plan = replace_once(
    step_plan,
    "`RenderSystem.Frame.Submit`",
    "`RenderSystem.Command.Submit`",
    "submit task name",
)
step_plan = replace_once(
    step_plan,
    """- `Docs/Step17B_Render_Packet_View_Pass_Connection.md`
""",
    """- `Docs/Step17B_Render_Packet_View_Pass_Connection.md`
- `Docs/Step17B_Draw_Performance_Breakdown.md`
""",
    "Step 17 completion documents",
)
step_plan = replace_once(
    step_plan,
    "状態: **主要RenderPass接続完了・計測待ち**",
    "状態: **主要RenderPass接続・Schedule Capture確認完了 / IRenderable参照除去待ち**",
    "Step 17-B status",
)
step_plan = replace_once(
    step_plan,
    "- [ ] CaptureでBuildとSubmitの分離を確認",
    "- [x] CaptureでBuildとSubmitの分離を確認",
    "Step 17-B capture checkbox",
)
step_plan = replace_once(
    step_plan,
    """---

## Step 17-C: Animation CPU Build / GPU Upload分離
""",
    """---

## Step 17-B.1: Draw Performance計測内訳化

優先度: **高・Step 17-Cより前**

状態: **CPU計測実装完了・実機比較待ち**

### 目的

Performance MonitorのDraw全体とSchedule CaptureのRender Domain時間を直接比較せず、Schedule外のEditor UI、ImGui、Platform Window、Present待機を分離する。

### CPU計測区間

- [x] Frame Setup
- [x] ImGui Begin / UI Frame
- [x] Render Schedule CPU
- [x] Debug Draw CPU
- [x] Editor UI Build CPU
- [x] ImGui Render / Platform Windows
- [x] Present / VSync Wait
- [x] Unaccounted / Timer Overhead
- [x] Total Draw

### 計測契約

- 完了済みフレームのSnapshotを次フレームのEditorへ渡す
- Performance Monitor自身の負荷は次フレームのEditor UI Buildへ含める
- CPU時間、Present待機、GPU時間を混同しない
- 同一Scene / View / Window条件で比較する

### 残作業

- [ ] VSync ON / OFF比較
- [ ] D3D11 Timestamp / Disjoint QueryによるGPU Frame Time
- [ ] 支配区間の確定
- [ ] 支配区間に応じた最適化優先順位の再決定

詳細:

- `Docs/Step17B_Draw_Performance_Breakdown.md`

---

## Step 17-C: Animation CPU Build / GPU Upload分離
""",
    "Step 17-B.1 insertion",
)
step_plan = replace_once(
    step_plan,
    """# 実装優先順位

1. **Step 17-A: SystemTask命名規則の統一**
2. **Step 17-B: Render Packet基盤**
3. **Step 17-C: Animation CPU Build / GPU Upload分離**
4. Step 17-D: Terrain CPU Mesh Build / GPU Upload分離
5. Step 17-E: Wave CPU Vertex Build / GPU Upload分離
6. Step 17-F: Physics Begin–Fetch待機隠蔽の再評価
""",
    """# 実装優先順位

1. **Step 17-A: SystemTask命名規則の統一**
2. **Step 17-B: Render Packet基盤**
3. **Step 17-B.1: Draw Performance計測内訳化**
4. **IRenderable内部のComponentRegistry参照除去**
5. **Step 17-C: Animation CPU Build / GPU Upload分離**
6. Step 17-D: Terrain CPU Mesh Build / GPU Upload分離
7. Step 17-E: Wave CPU Vertex Build / GPU Upload分離
8. Step 17-F: Physics Begin–Fetch待機隠蔽の再評価
""",
    "Step 17 final priority",
)
save(step_plan_path, step_plan)
