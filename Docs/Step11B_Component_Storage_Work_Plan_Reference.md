# Step 11-B Component Storage Work Plan Reference

PR #45のComponent Storage追加計画は、次を正式な作業計画として扱う。

- `Docs/Step11B_Component_Storage_Work_Plan.md`
- `Docs/Step11_Component_Storage_Strategy.md`

実施位置は、現在のStop / Material Alpha / RectTransform回帰修正とStep 17のRender Packet / Frame Pacing安定化後、Step 18 RenderWorld着手前とする。

採用Storage:

- `SparseComponentStorage<T>`
- `DenseComponentPool<T>`
- `DirectPagedComponentStorage<T>`

`ChunkedDenseComponentPool`は導入しない。通常Denseの連続性を維持し、再確保は`reserve()`と事前集計で制御する。
