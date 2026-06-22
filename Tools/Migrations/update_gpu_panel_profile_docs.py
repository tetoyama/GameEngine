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


breakdown_path = "Docs/Step17B_Draw_Performance_Breakdown.md"
text = load(breakdown_path)
text = text.replace("Present / VSync Wait", "Present / Queue Wait")
text = replace_once(
    text,
    """- [x] Schedule CaptureでPacket Build / Command Submit分離を確認
- [ ] D3D11 Timestamp / Disjoint QueryによるGPU Frame Time計測
- [ ] VSync ON / OFFで同一Sceneを比較
- [ ] Editor View / Player View条件を揃えた比較
- [ ] 支配区間を確定
""",
    """- [x] Schedule CaptureでPacket Build / Command Submit分離を確認
- [x] Performance Monitor CPU内訳を実機表示
- [x] Editor Panel単位CPU計測を実装
- [x] D3D11 Timestamp / Disjoint QueryによるGPU Frame Time計測を実装
- [ ] GPU Frame Timeの実機表示確認
- [ ] Editor Panel単位CPU時間の実機表示確認
- [ ] VSync ON / OFFで同一Sceneを比較
- [ ] Editor View / Player View条件を揃えた比較
- [ ] 支配区間を最終確定
""",
    "breakdown implementation status",
)
text = replace_once(
    text,
    """---

## 実機検証手順
""",
    """---

## 2026-06-23 実機観測

### Editorレイアウト表示

- VSync: OFF
- Total Draw: 12.9716ms
- Render Schedule CPU: 5.6725ms
- Editor UI Build CPU: 6.7990ms
- ImGui Render / Platform Windows: 0.1718ms
- Present / Queue Wait: 0.0876ms

判断:

- Editor UI Buildが約52%を占める
- Render Scheduleが約44%を占める
- Editor使用時はPanel単位の調査が最優先

### Player大表示

- VSync: OFF
- Total Draw: 10.4272ms
- Render Schedule CPU: 4.5389ms
- Editor UI Build CPU: 0.3200ms
- ImGui Render / Platform Windows: 0.0569ms
- Present / Queue Wait: 5.3123ms

判断:

- Render ScheduleはSchedule Captureの約4.6msと整合する
- Present / Queue Waitが約51%を占める
- VSync OFFのため垂直同期待ちとは断定しない
- GPU負荷、Frame Queue、Desktop Compositor待ちをGPU Timestampで分離する

2条件は表示領域とEditor Panel配置が異なるため、直接的な性能比較ではなく支配区間の発見に使用する。

---

## 実機検証手順
""",
    "insert observed results",
)
text = replace_once(
    text,
    """## GPU計測の次段階

D3D11では次のQueryをFrame境界へ追加する。
""",
    """## GPU計測

D3D11では次のQueryをFrame境界へ追加した。
""",
    "GPU section status",
)
text = replace_once(
    text,
    """## 次の作業順

1. Performance Monitor内訳の実機確認
2. VSync ON / OFF比較
3. GPU Frame Time計測追加
4. 支配区間を基に最適化対象を決定
5. IRenderable内部のComponentRegistry参照除去
6. Step 17-C Animation CPU Build / GPU Upload分離
""",
    """## 次の作業順

1. GPU Frame TimeとEditor Panel単位CPU時間の実機確認
2. 同一条件でVSync ON / OFF比較
3. Editor Panelの支配要因を特定して不要な毎Frame処理を削減
4. GPU時間とPresent / Queue Waitの関係を判定
5. IRenderable内部のComponentRegistry参照除去
6. Pipeline / Material / Mesh単位のState Bind削減
7. Step 17-C Animation CPU Build / GPU Upload分離
""",
    "breakdown next steps",
)
save(breakdown_path, text)


step_path = "Docs/Step17_Task_Naming_And_Render_Task_Decomposition_Plan.md"
text = load(step_path)
text = replace_once(
    text,
    "状態: **CPU計測実装完了・実機比較待ち**",
    "状態: **CPU実機観測完了・GPU / Panel計測実装完了・実機確認待ち**",
    "Step 17-B.1 status",
)
text = text.replace("- [x] Present / VSync Wait", "- [x] Present / Queue Wait")
text = replace_once(
    text,
    """### 残作業

- [ ] VSync ON / OFF比較
- [ ] D3D11 Timestamp / Disjoint QueryによるGPU Frame Time
- [ ] 支配区間の確定
- [ ] 支配区間に応じた最適化優先順位の再決定
""",
    """### 実装済み追加計測

- [x] D3D11 Timestamp / Disjoint Queryの4フレームリング
- [x] Query結果を`DONOTFLUSH`で非同期回収
- [x] GPU Frame Time表示
- [x] Tearing Support表示
- [x] Editor Panel単位CPU計測
- [x] Present表示を`Present / Queue Wait`へ修正

### 残作業

- [ ] GPU Frame Time実機表示確認
- [ ] Editor Panel単位CPU時間実機表示確認
- [ ] VSync ON / OFF比較
- [ ] 支配区間の最終確定
- [ ] 支配区間に応じた最適化優先順位の再決定
""",
    "Step 17-B.1 remaining work",
)
save(step_path, text)
