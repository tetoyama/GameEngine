# Step 11-B Component Storage Migration Index

正式な作業計画:

- `Docs/Step11B_Component_Storage_Work_Plan.md`
- `Docs/Step11_Component_Storage_Strategy.md`

採用Storage:

- `SparseComponentStorage<T>`
- `DenseComponentPool<T>`
- `DirectPagedComponentStorage<T>`

`ChunkedDenseComponentPool`は導入しない。

実施位置:

現在の描画回帰修正とStep 17安定化後、Step 18 RenderWorld着手前。
