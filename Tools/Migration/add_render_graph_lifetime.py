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


# ----------------------------------------------------------------------
# RenderGraph: compile-time logical resource lifetime analysis.
# ----------------------------------------------------------------------
path = "Source/GameApplication/Service/Graphics/RHI/RHIRenderGraph.h"
text = load(path)

text = replace_once(
    text,
    """struct RenderGraphResource {
\tuint32_t index = (std::numeric_limits<uint32_t>::max)();
\tconstexpr explicit operator bool() const noexcept { return index != (std::numeric_limits<uint32_t>::max)(); }
\tfriend constexpr bool operator==(const RenderGraphResource&, const RenderGraphResource&) noexcept = default;
};

""",
    """inline constexpr uint32_t InvalidRenderGraphIndex =
\t(std::numeric_limits<uint32_t>::max)();

struct RenderGraphResource {
\tuint32_t index = InvalidRenderGraphIndex;
\tconstexpr explicit operator bool() const noexcept {
\t\treturn index != InvalidRenderGraphIndex;
\t}
\tfriend constexpr bool operator==(
\t\tconst RenderGraphResource&,
\t\tconst RenderGraphResource&
\t) noexcept = default;
};

struct RenderGraphResourceLifetime {
\tuint32_t firstExecutionIndex = InvalidRenderGraphIndex;
\tuint32_t lastExecutionIndex = InvalidRenderGraphIndex;
\tuint32_t firstPassIndex = InvalidRenderGraphIndex;
\tuint32_t lastPassIndex = InvalidRenderGraphIndex;
\tuint32_t passUseCount = 0;

\tconstexpr bool IsUsed() const noexcept {
\t\treturn firstExecutionIndex != InvalidRenderGraphIndex;
\t}

\tconstexpr bool Overlaps(
\t\tconst RenderGraphResourceLifetime& other
\t) const noexcept {
\t\tif(!IsUsed() || !other.IsUsed()) return false;
\t\treturn !(lastExecutionIndex < other.firstExecutionIndex ||
\t\t\tother.lastExecutionIndex < firstExecutionIndex);
\t}
};

""",
    "RenderGraph lifetime public types",
)

text = replace_once(
    text,
    """\tbool Compile(){
\t\tm_executionOrder.clear();
\t\tm_finalStates.clear();
\t\tm_error.clear();
""",
    """\tbool Compile(){
\t\tm_executionOrder.clear();
\t\tm_resourceLifetimes.clear();
\t\tm_finalStates.clear();
\t\tm_error.clear();
""",
    "Compile lifetime reset",
)

text = replace_once(
    text,
    """\t\tBuildDependencies();
\t\tif(!BuildExecutionOrder()) return Fail("RenderGraph dependency cycle detected.");
\t\tif(!BuildBarriers()) return false;
""",
    """\t\tBuildDependencies();
\t\tif(!BuildExecutionOrder()) return Fail("RenderGraph dependency cycle detected.");
\t\tBuildResourceLifetimes();
\t\tif(!BuildBarriers()) return false;
""",
    "Compile lifetime analysis",
)

text = replace_once(
    text,
    """\tconst std::vector<ResourceBarrierDesc>& BarriersForPass(uint32_t passIndex) const noexcept {
\t\tstatic const std::vector<ResourceBarrierDesc> empty;
\t\treturn passIndex < m_passes.size() ? m_passes[passIndex].barriers : empty;
\t}
\tResourceState FinalState(RenderGraphResource resource) const noexcept {
""",
    """\tconst std::vector<ResourceBarrierDesc>& BarriersForPass(uint32_t passIndex) const noexcept {
\t\tstatic const std::vector<ResourceBarrierDesc> empty;
\t\treturn passIndex < m_passes.size() ? m_passes[passIndex].barriers : empty;
\t}
\n\tconst RenderGraphResourceLifetime& Lifetime(
\t\tRenderGraphResource resource
\t) const noexcept {
\t\tstatic const RenderGraphResourceLifetime empty;
\t\treturn resource && resource.index < m_resourceLifetimes.size()
\t\t\t? m_resourceLifetimes[resource.index]
\t\t\t: empty;
\t}
\n\tbool IsImported(RenderGraphResource resource) const noexcept {
\t\treturn resource && resource.index < m_resources.size() &&
\t\t\tm_resources[resource.index].IsBound();
\t}
\n\tbool IsTransient(RenderGraphResource resource) const noexcept {
\t\treturn resource && resource.index < m_resources.size() &&
\t\t\t!m_resources[resource.index].IsBound();
\t}
\n\tbool CanAlias(
\t\tRenderGraphResource lhs,
\t\tRenderGraphResource rhs
\t) const noexcept {
\t\tif(lhs == rhs || !IsTransient(lhs) || !IsTransient(rhs)) return false;
\t\tconst RenderGraphResourceLifetime& left = Lifetime(lhs);
\t\tconst RenderGraphResourceLifetime& right = Lifetime(rhs);
\t\treturn left.IsUsed() && right.IsUsed() && !left.Overlaps(right);
\t}
\n\tResourceState FinalState(RenderGraphResource resource) const noexcept {
""",
    "Lifetime query API",
)

text = replace_once(
    text,
    """\t\tm_passes.clear();
\t\tm_executionOrder.clear();
\t\tm_finalStates.clear();
""",
    """\t\tm_passes.clear();
\t\tm_executionOrder.clear();
\t\tm_resourceLifetimes.clear();
\t\tm_finalStates.clear();
""",
    "Reset lifetime state",
)

text = replace_once(
    text,
    """\tvoid AppendBarrier(Pass& pass, const Resource& resource, ResourceBarrierType type,
""",
    """\tvoid BuildResourceLifetimes(){
\t\tm_resourceLifetimes.assign(m_resources.size(), {});
\t\tstd::vector<uint8_t> seenInPass(m_resources.size(), 0);

\t\tfor(uint32_t executionIndex = 0;
\t\t\texecutionIndex < m_executionOrder.size();
\t\t\t++executionIndex){
\t\t\tstd::fill(seenInPass.begin(), seenInPass.end(), 0);
\t\t\tconst uint32_t passIndex = m_executionOrder[executionIndex];

\t\t\tfor(const RenderGraphAccess& access : m_passes[passIndex].accesses){
\t\t\t\tconst uint32_t resourceIndex = access.resource.index;
\t\t\t\tif(seenInPass[resourceIndex]) continue;
\t\t\t\tseenInPass[resourceIndex] = 1;

\t\t\t\tRenderGraphResourceLifetime& lifetime =
\t\t\t\t\tm_resourceLifetimes[resourceIndex];
\t\t\t\tif(!lifetime.IsUsed()){
\t\t\t\t\tlifetime.firstExecutionIndex = executionIndex;
\t\t\t\t\tlifetime.firstPassIndex = passIndex;
\t\t\t\t}
\t\t\t\tlifetime.lastExecutionIndex = executionIndex;
\t\t\t\tlifetime.lastPassIndex = passIndex;
\t\t\t\t++lifetime.passUseCount;
\t\t\t}
\t\t}
\t}

\tvoid AppendBarrier(Pass& pass, const Resource& resource, ResourceBarrierType type,
""",
    "BuildResourceLifetimes implementation",
)

text = replace_once(
    text,
    """\tstd::vector<Pass> m_passes;
\tstd::vector<uint32_t> m_executionOrder;
\tstd::vector<std::vector<ResourceState>> m_finalStates;
""",
    """\tstd::vector<Pass> m_passes;
\tstd::vector<uint32_t> m_executionOrder;
\tstd::vector<RenderGraphResourceLifetime> m_resourceLifetimes;
\tstd::vector<std::vector<ResourceState>> m_finalStates;
""",
    "Lifetime member storage",
)

save(path, text)


# ----------------------------------------------------------------------
# Contract test: compile the new lifetime and alias-query surface.
# Runtime execution remains manual under the current CI policy.
# ----------------------------------------------------------------------
path = "Tests/RHIRenderGraphBarrierSmokeTest.cpp"
text = load(path)
text = replace_once(
    text,
    """} // namespace

int main(){
""",
    """void TestTransientLifetimeAnalysis(){
\tRHI::RenderGraph graph;
\tconst auto transientA = graph.AddResource("TransientA");
\tconst auto transientB = graph.AddResource("TransientB");
\tconst auto unused = graph.AddResource("Unused");

\tconst uint32_t writeA = graph.AddPass(
\t\t"WriteA",
\t\t[&](RHI::RenderGraphPassBuilder& builder){ builder.Write(transientA); },
\t\t{}
\t);
\tconst uint32_t readA = graph.AddPass(
\t\t"ReadA",
\t\t[&](RHI::RenderGraphPassBuilder& builder){ builder.Read(transientA); },
\t\t{}
\t);
\tconst uint32_t writeB = graph.AddPass(
\t\t"WriteB",
\t\t[&](RHI::RenderGraphPassBuilder& builder){ builder.Write(transientB); },
\t\t{}
\t);
\tconst uint32_t readB = graph.AddPass(
\t\t"ReadB",
\t\t[&](RHI::RenderGraphPassBuilder& builder){ builder.Read(transientB); },
\t\t{}
\t);

\tassert(graph.Compile());
\tconst auto& lifetimeA = graph.Lifetime(transientA);
\tconst auto& lifetimeB = graph.Lifetime(transientB);
\tassert(lifetimeA.firstPassIndex == writeA);
\tassert(lifetimeA.lastPassIndex == readA);
\tassert(lifetimeA.firstExecutionIndex == 0);
\tassert(lifetimeA.lastExecutionIndex == 1);
\tassert(lifetimeA.passUseCount == 2);
\tassert(lifetimeB.firstPassIndex == writeB);
\tassert(lifetimeB.lastPassIndex == readB);
\tassert(lifetimeB.firstExecutionIndex == 2);
\tassert(lifetimeB.lastExecutionIndex == 3);
\tassert(lifetimeB.passUseCount == 2);
\tassert(!lifetimeA.Overlaps(lifetimeB));
\tassert(graph.CanAlias(transientA, transientB));
\tassert(graph.IsTransient(transientA));
\tassert(!graph.IsImported(transientA));
\tassert(!graph.Lifetime(unused).IsUsed());
\tassert(!graph.CanAlias(transientA, unused));
}

} // namespace

int main(){
""",
    "Lifetime test insertion",
)
text = replace_once(
    text,
    """\tTestInvalidSubresourceDeclarations();
\treturn 0;
""",
    """\tTestInvalidSubresourceDeclarations();
\tTestTransientLifetimeAnalysis();
\treturn 0;
""",
    "Lifetime test registration",
)
save(path, text)


# ----------------------------------------------------------------------
# Migration plan and completion note.
# ----------------------------------------------------------------------
path = "Docs/ECS_Scheduler_Migration_Plan.md"
text = load(path)
text = replace_once(
    text,
    "- [ ] Transient Resource寿命解析",
    "- [x] Transient Resource寿命解析",
    "Step 16-F lifetime checkbox",
)
text = replace_once(
    text,
    """1. Step 16-FのTransient Resource寿命解析
2. Step 16-FのQueue間同期
3. Step 16-FのPass Culling
4. D3D11最小実描画Triangle Test
5. 既存Player View実機回帰
6. Step 16完了報告
7. Step 17 RenderWorld移行
""",
    """1. Step 16-FのQueue間同期
2. Step 16-FのPass Culling
3. D3D11最小実描画Triangle Test
4. 既存Player View実機回帰
5. Step 16完了報告
6. Step 17 RenderWorld移行
""",
    "Current work position",
)
save(path, text)

completion = Path("Docs/Step16F_Transient_Resource_Lifetime_Completion.md")
completion.write_text(
    """# Step 16-F: Transient Resource Lifetime Analysis Completion

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
""",
    encoding="utf-8",
    newline="\n",
)

print("RenderGraph lifetime analysis migration applied")
