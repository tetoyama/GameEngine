# Step 16-F: Pass Culling Completion

## 完了内容

RenderGraph Compile時に不要なPassを除外し、実行・Barrier・Resource Lifetime・Queue同期の対象を生存Passだけへ限定する仕組みを追加した。

## 安全側の契約

- 既存互換性のため、通常のPassは既定でCullingしない
- Culling対象は`RenderGraphPassFlags::Cullable`を明示したPassだけ
- 非Cullable Passは副作用を持つ可能性があるRootとして保持する
- Imported ResourceへWrite / ReadWriteするPassは外部副作用Rootとして保持する
- `MarkOutput()`されたResourceへWrite / ReadWriteするPassは出力Rootとして保持する
- Root Passの全依存元を逆向きに辿り、生産Passを保持する

## Compile順序

1. 全PassのAccess宣言を検証
2. 全Passで一時依存Graphを構築
3. Rootから逆向きにPass Livenessを決定
4. 生存Passだけで依存Graphを再構築
5. 生存PassだけでQueue Sharingを検証
6. Execution Order、Lifetime、Queue同期、Barrierを構築

この順序により、完全にCullingされるCross-Queue Resourceは不要なExclusive Sharing Errorを発生させない。

## 公開API

- `RenderGraphPassFlags::Cullable`
- `RenderGraph::MarkOutput(RenderGraphResource)`
- `RenderGraph::IsPassCulled(uint32_t)`
- `RenderGraph::CulledPassCount()`

## 検証

Contract Testで次を確認する。

- 未使用のCullable Producer / Consumerが両方Cullingされる
- MarkOutputされたResourceのWriterと依存Producerが保持される
- Imported Resource Writerが保持される
- 既定の非Cullable Passが保持される
- CullingされたCross-Queue Exclusive ResourceがQueue Sharing Errorを起こさない
- Culling PassがExecution Order、Lifetime、Queue同期へ混入しない
