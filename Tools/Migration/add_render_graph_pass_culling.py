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


path = "Source/GameApplication/Service/Graphics/RHI/RHIRenderGraph.h"
text = load(path)

text = replace_once(
    text,
    "enum class RenderGraphAccessType : uint8_t { Read, Write, ReadWrite };\n",
    """enum class RenderGraphAccessType : uint8_t { Read, Write, ReadWrite };

enum class RenderGraphPassFlags : uint8_t {
\tNone = 0,
\tCullable = 1u << 0
};

constexpr RenderGraphPassFlags operator|(
\tRenderGraphPassFlags lhs,
\tRenderGraphPassFlags rhs
) noexcept {
\treturn static_cast<RenderGraphPassFlags>(
\t\tstatic_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs)
\t);
}

constexpr bool HasPassFlag(
\tRenderGraphPassFlags value,
\tRenderGraphPassFlags flag
) noexcept {
\treturn (static_cast<uint8_t>(value) & static_cast<uint8_t>(flag)) != 0;
}
""",
    "RenderGraph pass flags",
)

text = replace_once(
    text,
    """\tuint32_t AddPass(
\t\tstd::string name,
\t\tconst std::function<void(RenderGraphPassBuilder&)>& setup,
\t\tExecuteCallback execute
\t){
\t\treturn AddPass(
\t\t\tstd::move(name),
\t\t\tCommandQueueType::Graphics,
\t\t\tsetup,
\t\t\tstd::move(execute)
\t\t);
\t}

\tuint32_t AddPass(
\t\tstd::string name,
\t\tCommandQueueType queue,
\t\tconst std::function<void(RenderGraphPassBuilder&)>& setup,
\t\tExecuteCallback execute
\t){
\t\tRenderGraphPassBuilder builder;
\t\tif(setup) setup(builder);
\t\tPass pass;
\t\tpass.name = std::move(name);
\t\tpass.queue = queue;
\t\tpass.accesses = builder.Accesses();
\t\tpass.setupError = builder.Error();
\t\tpass.execute = std::move(execute);
\t\tm_passes.push_back(std::move(pass));
\t\tm_compiled = false;
\t\treturn static_cast<uint32_t>(m_passes.size() - 1);
\t}
""",
    """\tbool MarkOutput(RenderGraphResource resource){
\t\tif(!resource || resource.index >= m_resources.size()) return false;
\t\tm_resources[resource.index].output = true;
\t\tm_compiled = false;
\t\treturn true;
\t}

\tuint32_t AddPass(
\t\tstd::string name,
\t\tconst std::function<void(RenderGraphPassBuilder&)>& setup,
\t\tExecuteCallback execute
\t){
\t\treturn AddPass(
\t\t\tstd::move(name),
\t\t\tCommandQueueType::Graphics,
\t\t\tRenderGraphPassFlags::None,
\t\t\tsetup,
\t\t\tstd::move(execute)
\t\t);
\t}

\tuint32_t AddPass(
\t\tstd::string name,
\t\tRenderGraphPassFlags flags,
\t\tconst std::function<void(RenderGraphPassBuilder&)>& setup,
\t\tExecuteCallback execute
\t){
\t\treturn AddPass(
\t\t\tstd::move(name),
\t\t\tCommandQueueType::Graphics,
\t\t\tflags,
\t\t\tsetup,
\t\t\tstd::move(execute)
\t\t);
\t}

\tuint32_t AddPass(
\t\tstd::string name,
\t\tCommandQueueType queue,
\t\tconst std::function<void(RenderGraphPassBuilder&)>& setup,
\t\tExecuteCallback execute
\t){
\t\treturn AddPass(
\t\t\tstd::move(name),
\t\t\tqueue,
\t\t\tRenderGraphPassFlags::None,
\t\t\tsetup,
\t\t\tstd::move(execute)
\t\t);
\t}

\tuint32_t AddPass(
\t\tstd::string name,
\t\tCommandQueueType queue,
\t\tRenderGraphPassFlags flags,
\t\tconst std::function<void(RenderGraphPassBuilder&)>& setup,
\t\tExecuteCallback execute
\t){
\t\tRenderGraphPassBuilder builder;
\t\tif(setup) setup(builder);
\t\tPass pass;
\t\tpass.name = std::move(name);
\t\tpass.queue = queue;
\t\tpass.flags = flags;
\t\tpass.accesses = builder.Accesses();
\t\tpass.setupError = builder.Error();
\t\tpass.execute = std::move(execute);
\t\tm_passes.push_back(std::move(pass));
\t\tm_compiled = false;
\t\treturn static_cast<uint32_t>(m_passes.size() - 1);
\t}
""",
    "AddPass culling overloads",
)

text = replace_once(
    text,
    """\t\tm_executionOrder.clear();
\t\tm_resourceLifetimes.clear();
""",
    """\t\tm_executionOrder.clear();
\t\tm_passLive.clear();
\t\tm_resourceLifetimes.clear();
""",
    "Compile pass liveness reset",
)

text = replace_once(
    text,
    """\t\tif(!ValidateQueueSharing()) return false;
\t\tBuildDependencies();
\t\tif(!BuildExecutionOrder()) return Fail("RenderGraph dependency cycle detected.");
""",
    """\t\tBuildDependencies(false);
\t\tBuildPassLiveness();
\t\tClearDependencyGraph();
\t\tBuildDependencies(true);
\t\tif(!ValidateQueueSharing()) return false;
\t\tif(!BuildExecutionOrder()) return Fail("RenderGraph dependency cycle detected.");
""",
    "Compile pass culling pipeline",
)

text = replace_once(
    text,
    "\tconst std::vector<uint32_t>& ExecutionOrder() const noexcept { return m_executionOrder; }\n",
    """\tconst std::vector<uint32_t>& ExecutionOrder() const noexcept { return m_executionOrder; }

\tbool IsPassCulled(uint32_t passIndex) const noexcept {
\t\treturn passIndex < m_passes.size() &&
\t\t\tm_passLive.size() == m_passes.size() &&
\t\t\tm_passLive[passIndex] == 0;
\t}

\tsize_t CulledPassCount() const noexcept {
\t\tif(m_passLive.size() != m_passes.size()) return 0;
\t\treturn static_cast<size_t>(std::count(m_passLive.begin(), m_passLive.end(), uint8_t{0}));
\t}
""",
    "Pass culling query API",
)

text = replace_once(
    text,
    """\t\tm_executionOrder.clear();
\t\tm_resourceLifetimes.clear();
\t\tm_queueSync.clear();
""",
    """\t\tm_executionOrder.clear();
\t\tm_passLive.clear();
\t\tm_resourceLifetimes.clear();
\t\tm_queueSync.clear();
""",
    "Reset pass liveness",
)

text = replace_once(
    text,
    """\t\tuint32_t subresourceCount = 1;
\t\tResourceQueueSharingMode queueSharingMode = ResourceQueueSharingMode::Exclusive;
\t\tbool IsBound() const noexcept { return static_cast<bool>(buffer) != static_cast<bool>(texture); }
""",
    """\t\tuint32_t subresourceCount = 1;
\t\tResourceQueueSharingMode queueSharingMode = ResourceQueueSharingMode::Exclusive;
\t\tbool output = false;
\t\tbool IsBound() const noexcept { return static_cast<bool>(buffer) != static_cast<bool>(texture); }
""",
    "Resource output flag",
)

text = replace_once(
    text,
    """\t\tstd::string name;
\t\tCommandQueueType queue = CommandQueueType::Graphics;
\t\tstd::vector<RenderGraphAccess> accesses;
""",
    """\t\tstd::string name;
\t\tCommandQueueType queue = CommandQueueType::Graphics;
\t\tRenderGraphPassFlags flags = RenderGraphPassFlags::None;
\t\tstd::vector<RenderGraphAccess> accesses;
""",
    "Pass flags storage",
)

text = replace_once(
    text,
    """\tbool ValidateQueueSharing(){
\t\tstd::vector<uint8_t> queueMasks(m_resources.size(), 0);
\t\tfor(const Pass& pass : m_passes){
\t\t\tconst uint8_t queueBit = static_cast<uint8_t>(1u << QueueIndex(pass.queue));
\t\t\tfor(const RenderGraphAccess& access : pass.accesses){
\t\t\t\tqueueMasks[access.resource.index] |= queueBit;
\t\t\t}
\t\t}

\t\tfor(size_t resourceIndex = 0; resourceIndex < m_resources.size(); ++resourceIndex){
\t\t\tconst uint8_t queueMask = queueMasks[resourceIndex];
\t\t\tconst bool usesMultipleQueues =
\t\t\t\tqueueMask != 0 &&
\t\t\t\t(queueMask & static_cast<uint8_t>(queueMask - 1)) != 0;
\t\t\tif(!usesMultipleQueues) continue;

\t\t\tconst Resource& resource = m_resources[resourceIndex];
\t\t\tif(resource.queueSharingMode == ResourceQueueSharingMode::Concurrent) continue;

\t\t\tconst std::string resourceName = resource.name.empty()
\t\t\t\t? std::string("RenderGraph resource #") + std::to_string(resourceIndex)
\t\t\t\t: resource.name;
\t\t\treturn Fail(
\t\t\t\tresourceName +
\t\t\t\t": exclusive resource is used by multiple queue domains; declare Concurrent sharing."
\t\t\t);
\t\t}
\t\treturn true;
\t}

\tvoid BuildDependencies(){
\t\tfor(uint32_t earlier = 0; earlier < m_passes.size(); ++earlier){
\t\t\tfor(uint32_t later = earlier + 1; later < m_passes.size(); ++later){
\t\t\t\tif(!Conflicts(m_passes[earlier], m_passes[later])) continue;
\t\t\t\tm_passes[earlier].dependents.push_back(later);
\t\t\t\tm_passes[later].dependencies.push_back(earlier);
\t\t\t}
\t\t}
\t}
\tbool BuildExecutionOrder(){
\t\tstd::vector<uint32_t> unresolved;
\t\tstd::vector<uint32_t> ready;
\t\tfor(uint32_t index = 0; index < m_passes.size(); ++index){
\t\t\tunresolved.push_back(static_cast<uint32_t>(m_passes[index].dependencies.size()));
\t\t\tif(unresolved.back() == 0) ready.push_back(index);
\t\t}
\t\twhile(!ready.empty()){
\t\t\tauto next = (std::min_element)(ready.begin(), ready.end());
\t\t\tconst uint32_t index = *next;
\t\t\tready.erase(next);
\t\t\tm_executionOrder.push_back(index);
\t\t\tfor(uint32_t dependent : m_passes[index].dependents){
\t\t\t\tif(unresolved[dependent] == 0) continue;
\t\t\t\t--unresolved[dependent];
\t\t\t\tif(unresolved[dependent] == 0) ready.push_back(dependent);
\t\t\t}
\t\t}
\t\treturn m_executionOrder.size() == m_passes.size();
\t}
""",
    """\tbool IsPassLiveInternal(uint32_t passIndex) const noexcept {
\t\treturn passIndex < m_passLive.size() && m_passLive[passIndex] != 0;
\t}

\tvoid ClearDependencyGraph(){
\t\tfor(Pass& pass : m_passes){
\t\t\tpass.dependencies.clear();
\t\t\tpass.dependents.clear();
\t\t}
\t}

\tvoid BuildPassLiveness(){
\t\tm_passLive.assign(m_passes.size(), 0);
\t\tstd::vector<uint32_t> pending;

\t\tfor(uint32_t passIndex = 0; passIndex < m_passes.size(); ++passIndex){
\t\t\tconst Pass& pass = m_passes[passIndex];
\t\t\tbool root = !HasPassFlag(pass.flags, RenderGraphPassFlags::Cullable);

\t\t\tfor(const RenderGraphAccess& access : pass.accesses){
\t\t\t\tif(!Writes(access.type)) continue;
\t\t\t\tconst Resource& resource = m_resources[access.resource.index];
\t\t\t\tif(resource.output || resource.IsBound()){
\t\t\t\t\troot = true;
\t\t\t\t\tbreak;
\t\t\t\t}
\t\t\t}

\t\t\tif(!root) continue;
\t\t\tm_passLive[passIndex] = 1;
\t\t\tpending.push_back(passIndex);
\t\t}

\t\twhile(!pending.empty()){
\t\t\tconst uint32_t passIndex = pending.back();
\t\t\tpending.pop_back();
\t\t\tfor(uint32_t dependency : m_passes[passIndex].dependencies){
\t\t\t\tif(m_passLive[dependency]) continue;
\t\t\t\tm_passLive[dependency] = 1;
\t\t\t\tpending.push_back(dependency);
\t\t\t}
\t\t}
\t}

\tbool ValidateQueueSharing(){
\t\tstd::vector<uint8_t> queueMasks(m_resources.size(), 0);
\t\tfor(uint32_t passIndex = 0; passIndex < m_passes.size(); ++passIndex){
\t\t\tif(!IsPassLiveInternal(passIndex)) continue;
\t\t\tconst Pass& pass = m_passes[passIndex];
\t\t\tconst uint8_t queueBit = static_cast<uint8_t>(1u << QueueIndex(pass.queue));
\t\t\tfor(const RenderGraphAccess& access : pass.accesses){
\t\t\t\tqueueMasks[access.resource.index] |= queueBit;
\t\t\t}
\t\t}

\t\tfor(size_t resourceIndex = 0; resourceIndex < m_resources.size(); ++resourceIndex){
\t\t\tconst uint8_t queueMask = queueMasks[resourceIndex];
\t\t\tconst bool usesMultipleQueues =
\t\t\t\tqueueMask != 0 &&
\t\t\t\t(queueMask & static_cast<uint8_t>(queueMask - 1)) != 0;
\t\t\tif(!usesMultipleQueues) continue;

\t\t\tconst Resource& resource = m_resources[resourceIndex];
\t\t\tif(resource.queueSharingMode == ResourceQueueSharingMode::Concurrent) continue;

\t\t\tconst std::string resourceName = resource.name.empty()
\t\t\t\t? std::string("RenderGraph resource #") + std::to_string(resourceIndex)
\t\t\t\t: resource.name;
\t\t\treturn Fail(
\t\t\t\tresourceName +
\t\t\t\t": exclusive resource is used by multiple queue domains; declare Concurrent sharing."
\t\t\t);
\t\t}
\t\treturn true;
\t}

\tvoid BuildDependencies(bool liveOnly){
\t\tfor(uint32_t earlier = 0; earlier < m_passes.size(); ++earlier){
\t\t\tif(liveOnly && !IsPassLiveInternal(earlier)) continue;
\t\t\tfor(uint32_t later = earlier + 1; later < m_passes.size(); ++later){
\t\t\t\tif(liveOnly && !IsPassLiveInternal(later)) continue;
\t\t\t\tif(!Conflicts(m_passes[earlier], m_passes[later])) continue;
\t\t\t\tm_passes[earlier].dependents.push_back(later);
\t\t\t\tm_passes[later].dependencies.push_back(earlier);
\t\t\t}
\t\t}
\t}

\tbool BuildExecutionOrder(){
\t\tstd::vector<uint32_t> unresolved(m_passes.size(), 0);
\t\tstd::vector<uint32_t> ready;
\t\tsize_t liveCount = 0;
\t\tfor(uint32_t index = 0; index < m_passes.size(); ++index){
\t\t\tif(!IsPassLiveInternal(index)) continue;
\t\t\t++liveCount;
\t\t\tunresolved[index] = static_cast<uint32_t>(m_passes[index].dependencies.size());
\t\t\tif(unresolved[index] == 0) ready.push_back(index);
\t\t}
\t\twhile(!ready.empty()){
\t\t\tauto next = (std::min_element)(ready.begin(), ready.end());
\t\t\tconst uint32_t index = *next;
\t\t\tready.erase(next);
\t\t\tm_executionOrder.push_back(index);
\t\t\tfor(uint32_t dependent : m_passes[index].dependents){
\t\t\t\tif(unresolved[dependent] == 0) continue;
\t\t\t\t--unresolved[dependent];
\t\t\t\tif(unresolved[dependent] == 0) ready.push_back(dependent);
\t\t\t}
\t\t}
\t\treturn m_executionOrder.size() == liveCount;
\t}
""",
    "Pass liveness and live dependency graph",
)

text = replace_once(
    text,
    """\tstd::vector<Pass> m_passes;
\tstd::vector<uint32_t> m_executionOrder;
\tstd::vector<RenderGraphResourceLifetime> m_resourceLifetimes;
""",
    """\tstd::vector<Pass> m_passes;
\tstd::vector<uint32_t> m_executionOrder;
\tstd::vector<uint8_t> m_passLive;
\tstd::vector<RenderGraphResourceLifetime> m_resourceLifetimes;
""",
    "Pass liveness member",
)

save(path, text)


path = "Tests/RHIRenderGraphBarrierSmokeTest.cpp"
text = load(path)
pass_culling_test = r'''
void TestPassCulling(){
	RHI::NullRHIDevice device({});

	RHI::BufferDesc externalDesc;
	externalDesc.byteSize = 64;
	externalDesc.initialState = RHI::ResourceState::Common;
	const RHI::BufferHandle externalBuffer = device.CreateBuffer(externalDesc, {});
	assert(externalBuffer);

	RHI::RenderGraph graph;
	const auto deadExclusive = graph.AddResource(
		"DeadExclusive",
		RHI::ResourceQueueSharingMode::Exclusive
	);
	const auto intermediate = graph.AddResource("Intermediate");
	const auto output = graph.AddResource("Output");
	assert(graph.MarkOutput(output));
	const auto importedOutput = graph.ImportBuffer(
		externalBuffer,
		externalDesc.initialState,
		"ExternalOutput"
	);

	std::vector<int> execution;
	const uint32_t deadWrite = graph.AddPass(
		"DeadWrite",
		RHI::CommandQueueType::Graphics,
		RHI::RenderGraphPassFlags::Cullable,
		[&](RHI::RenderGraphPassBuilder& builder){ builder.Write(deadExclusive); },
		[&](RHI::IRHICommandList&){ execution.push_back(1); }
	);
	const uint32_t deadRead = graph.AddPass(
		"DeadRead",
		RHI::CommandQueueType::Compute,
		RHI::RenderGraphPassFlags::Cullable,
		[&](RHI::RenderGraphPassBuilder& builder){ builder.Read(deadExclusive); },
		[&](RHI::IRHICommandList&){ execution.push_back(2); }
	);
	const uint32_t producer = graph.AddPass(
		"Producer",
		RHI::RenderGraphPassFlags::Cullable,
		[&](RHI::RenderGraphPassBuilder& builder){ builder.Write(intermediate); },
		[&](RHI::IRHICommandList&){ execution.push_back(3); }
	);
	const uint32_t outputWriter = graph.AddPass(
		"OutputWriter",
		RHI::RenderGraphPassFlags::Cullable,
		[&](RHI::RenderGraphPassBuilder& builder){
			builder.Read(intermediate);
			builder.Write(output);
		},
		[&](RHI::IRHICommandList&){ execution.push_back(4); }
	);
	const uint32_t importedWriter = graph.AddPass(
		"ImportedWriter",
		RHI::RenderGraphPassFlags::Cullable,
		[&](RHI::RenderGraphPassBuilder& builder){
			builder.Write(importedOutput, RHI::ResourceState::CopyDestination);
		},
		[&](RHI::IRHICommandList&){ execution.push_back(5); }
	);
	const uint32_t compatibilityRoot = graph.AddPass(
		"CompatibilityRoot",
		[](RHI::RenderGraphPassBuilder&){},
		[&](RHI::IRHICommandList&){ execution.push_back(6); }
	);

	assert(graph.Compile());
	assert(graph.IsPassCulled(deadWrite));
	assert(graph.IsPassCulled(deadRead));
	assert(!graph.IsPassCulled(producer));
	assert(!graph.IsPassCulled(outputWriter));
	assert(!graph.IsPassCulled(importedWriter));
	assert(!graph.IsPassCulled(compatibilityRoot));
	assert(graph.CulledPassCount() == 2);
	assert((graph.ExecutionOrder() == std::vector<uint32_t>{
		producer,
		outputWriter,
		importedWriter,
		compatibilityRoot
	}));
	assert(!graph.RequiresQueueSynchronization());
	assert(!graph.Lifetime(deadExclusive).IsUsed());
	assert(graph.Lifetime(intermediate).IsUsed());
	assert(graph.Lifetime(output).IsUsed());
	assert(graph.FinalState(importedOutput) == RHI::ResourceState::CopyDestination);

	auto commandList = device.CreateCommandList({
		RHI::CommandQueueType::Graphics,
		false
	});
	assert(commandList);
	assert(graph.Execute(*commandList));
	assert((execution == std::vector<int>{3, 4, 5, 6}));
}

'''
text = replace_once(
    text,
    "} // namespace\n\nint main(){\n",
    pass_culling_test + "} // namespace\n\nint main(){\n",
    "Pass culling test insertion",
)
text = replace_once(
    text,
    "\tTestTransientLifetimeAnalysis();\n\treturn 0;\n",
    "\tTestTransientLifetimeAnalysis();\n\tTestPassCulling();\n\treturn 0;\n",
    "Pass culling test invocation",
)
save(path, text)


path = "Docs/ECS_Scheduler_Migration_Plan.md"
text = load(path)
text = replace_once(text, "- [ ] Pass Culling\n", "- [x] Pass Culling\n", "Pass Culling checklist")
text = replace_once(
    text,
    "- `Docs/Step16F_Queue_Synchronization_Completion.md`\n",
    "- `Docs/Step16F_Queue_Synchronization_Completion.md`\n"
    "- `Docs/Step16F_Pass_Culling_Completion.md`\n",
    "Pass Culling completion document link",
)
text = replace_once(
    text,
    """1. Step 16-FのPass Culling
2. D3D11最小実描画Triangle Test
3. 既存Player View実機回帰
4. Step 17-B Render Packet基盤
5. Step 17-C Animation CPU Build / GPU Upload分離
6. Step 17-D Terrain CPU Mesh Build / GPU Upload分離
7. Step 17-E Wave CPU Vertex Build / GPU Upload分離
""",
    """1. D3D11最小実描画Triangle Test
2. 既存Player View実機回帰
3. Step 17-B Render Packet基盤
4. Step 17-C Animation CPU Build / GPU Upload分離
5. Step 17-D Terrain CPU Mesh Build / GPU Upload分離
6. Step 17-E Wave CPU Vertex Build / GPU Upload分離
""",
    "Current work position after Pass Culling",
)
save(path, text)


path = "Docs/Step16_RHI_MultiBackend_Architecture.md"
text = load(path)
text = replace_once(
    text,
    """RenderGraphは論理Resource Accessから依存順序とBarrierを構築する。
BarrierのNative変換はBackend側の責務とする。
""",
    """RenderGraphは論理Resource Accessから依存順序とBarrierを構築する。
BarrierのNative変換はBackend側の責務とする。

Pass Cullingは`RenderGraphPassFlags::Cullable`を明示したPassだけを対象にする。既存Passは既定でRootとして保持し、Imported ResourceへのWrite、`MarkOutput()`されたResourceへのWrite、非Cullable PassをRootとして依存元を逆向きに生存判定する。Culling後のPassだけで依存、Queue Sharing検証、Barrier、Resource Lifetime、Queue同期を再構築する。
""",
    "RenderGraph Pass Culling architecture",
)
save(path, text)


completion = """# Step 16-F: Pass Culling Completion

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
"""
save("Docs/Step16F_Pass_Culling_Completion.md", completion)

print("RenderGraph Pass Culling migration applied")
