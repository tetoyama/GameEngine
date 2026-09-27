# Step 16-F: Transient Resource Lifetime Analysis Completion

## 完了内容

RenderGraphの確定実行順を基準に、各Logical Resourceの生存区間を解析する仕組みを追加した。

- 最初に使用されるExecution Index / Pass Index
- 最後に使用されるExecution Index / Pass Index
- Resourceを利用するPass数
- 生存区間の重複判定
- Imported ResourceとTransient Logical Resourceの区別
- 生存区間が重ならないTransient ResourceのAlias可否判定

## 契約

`CanAlias()`はLogical Resourceの生存区間だけを判定する。実際のGPU Resource割り当て、Heap配置、Backend固有Alias Barrierは行わない。物理Resource Poolへの割り当てはRenderWorld移行時にこの解析結果を利用して実装する。

未使用Resourceは生存区間を持たず、Alias候補にはしない。Imported Resourceは外部所有のためAlias候補にしない。

## 検証方針

通常CIではRenderGraph lifetime APIを利用するContract Testをコンパイルする。実行を伴うSmoke Testは手動`workflow_dispatch`に限定する。
