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


# ======================================================================
# Public queue submit contract: multiple waits/signals per submission.
# Legacy single-fence fields remain during migration.
# ======================================================================
path = "Source/GameApplication/Service/Graphics/RHI/RHIExecution.h"
text = load(path)
text = replace_once(
    text,
    """struct QueueSubmitDesc {
\tstd::span<IRHICommandList* const> commandLists;
\tFenceHandle waitFence;
\tuint64_t waitValue = 0;
\tFenceHandle signalFence;
\tuint64_t signalValue = 0;
};
""",
    """struct QueueFenceWait {
\tFenceHandle fence;
\tuint64_t value = 0;
};

struct QueueFenceSignal {
\tFenceHandle fence;
\tuint64_t value = 0;
};

struct QueueSubmitDesc {
\tstd::span<IRHICommandList* const> commandLists;
\tstd::span<const QueueFenceWait> waits;
\tstd::span<const QueueFenceSignal> signals;

\t// Compatibility path for callers that submit one wait and one signal.
\tFenceHandle waitFence;
\tuint64_t waitValue = 0;
\tFenceHandle signalFence;
\tuint64_t signalValue = 0;
};
""",
    "QueueSubmitDesc extension",
)
save(path, text)


# ======================================================================
# Null backend: validate all command lists, then process every wait/signal.
# ======================================================================
path = "Source/GameApplication/Service/Graphics/RHI/Null/NullRHIBackend.h"
text = load(path)
text = replace_once(
    text,
    """inline bool NullRHICommandQueue::Submit(const QueueSubmitDesc& d){
\tif(!m_owner) return false;
\tif(d.waitFence){ auto s = m_owner->FindFence(d.waitFence); if(!s) return false; std::unique_lock lock(s->mutex); s->condition.wait(lock, [&]{ return s->completedValue >= d.waitValue; }); }
\tfor(auto* list : d.commandLists){ if(!list || list->GetQueueType() != m_type) return false; auto* n = dynamic_cast<NullRHICommandList*>(list); if(!n || !n->IsClosed()) return false; }
\tif(d.signalFence){ auto s = m_owner->FindFence(d.signalFence); if(!s) return false; { std::scoped_lock lock(s->mutex); s->completedValue = (std::max)(s->completedValue, d.signalValue); } s->condition.notify_all(); }
\treturn true;
}
""",
    """inline bool NullRHICommandQueue::Submit(const QueueSubmitDesc& desc){
\tif(!m_owner) return false;

\tfor(IRHICommandList* commandList : desc.commandLists){
\t\tif(!commandList || commandList->GetQueueType() != m_type) return false;
\t\tauto* list = dynamic_cast<NullRHICommandList*>(commandList);
\t\tif(!list || !list->IsClosed()) return false;
\t}

\tauto waitFence = [this](FenceHandle handle, uint64_t value){
\t\tauto state = m_owner->FindFence(handle);
\t\tif(!state) return false;
\t\tstd::unique_lock lock(state->mutex);
\t\tstate->condition.wait(lock, [&]{ return state->completedValue >= value; });
\t\treturn true;
\t};
\tif(desc.waitFence && !waitFence(desc.waitFence, desc.waitValue)) return false;
\tfor(const QueueFenceWait& wait : desc.waits){
\t\tif(!wait.fence || !waitFence(wait.fence, wait.value)) return false;
\t}

\tauto signalFence = [this](FenceHandle handle, uint64_t value){
\t\tauto state = m_owner->FindFence(handle);
\t\tif(!state) return false;
\t\t{
\t\t\tstd::scoped_lock lock(state->mutex);
\t\t\tstate->completedValue = (std::max)(state->completedValue, value);
\t\t}
\t\tstate->condition.notify_all();
\t\treturn true;
\t};
\tif(desc.signalFence && !signalFence(desc.signalFence, desc.signalValue)) return false;
\tfor(const QueueFenceSignal& signal : desc.signals){
\t\tif(!signal.fence || !signalFence(signal.fence, signal.value)) return false;
\t}
\treturn true;
}
""",
    "Null queue submit",
)
save(path, text)


# ======================================================================
# D3D11 logical queues: all waits serialize on the Immediate Context;
# every requested signal receives an EVENT query.
# ======================================================================
path = "Source/GameApplication/Service/Graphics/RHI/D3D11/D3D11RHIDeviceRuntime.h"
text = load(path)
text = replace_once(
    text,
    """inline bool D3D11RHICommandQueue::Submit(const QueueSubmitDesc& desc){
\tif(!m_owner || !m_owner->m_context) return false;
\tfor(IRHICommandList* commandList : desc.commandLists){
\t\tif(!commandList || commandList->GetQueueType() != m_type) return false;
\t\tauto* list = dynamic_cast<D3D11RHICommandList*>(commandList);
\t\tif(!list || !list->IsClosed()) return false;
\t}
\tif(desc.waitFence){
\t\tauto state = m_owner->FindFence(desc.waitFence);
\t\tif(!state || !state->Wait(desc.waitValue, (std::numeric_limits<uint64_t>::max)())) return false;
\t}
\tif(desc.signalFence){
\t\tauto state = m_owner->FindFence(desc.signalFence);
\t\tif(!m_owner->SignalFence(state, desc.signalValue)) return false;
\t}
\telse m_owner->m_context->Flush();
\treturn true;
}
""",
    """inline bool D3D11RHICommandQueue::Submit(const QueueSubmitDesc& desc){
\tif(!m_owner || !m_owner->m_context) return false;
\tfor(IRHICommandList* commandList : desc.commandLists){
\t\tif(!commandList || commandList->GetQueueType() != m_type) return false;
\t\tauto* list = dynamic_cast<D3D11RHICommandList*>(commandList);
\t\tif(!list || !list->IsClosed()) return false;
\t}

\tauto waitFence = [this](FenceHandle handle, uint64_t value){
\t\tauto state = m_owner->FindFence(handle);
\t\treturn state && state->Wait(
\t\t\tvalue,
\t\t\t(std::numeric_limits<uint64_t>::max)()
\t\t);
\t};
\tif(desc.waitFence && !waitFence(desc.waitFence, desc.waitValue)) return false;
\tfor(const QueueFenceWait& wait : desc.waits){
\t\tif(!wait.fence || !waitFence(wait.fence, wait.value)) return false;
\t}

\tbool signaled = false;
\tauto signalFence = [this, &signaled](FenceHandle handle, uint64_t value){
\t\tauto state = m_owner->FindFence(handle);
\t\tif(!m_owner->SignalFence(state, value)) return false;
\t\tsignaled = true;
\t\treturn true;
\t};
\tif(desc.signalFence && !signalFence(desc.signalFence, desc.signalValue)) return false;
\tfor(const QueueFenceSignal& signal : desc.signals){
\t\tif(!signal.fence || !signalFence(signal.fence, signal.value)) return false;
\t}
\tif(!signaled) m_owner->m_context->Flush();
\treturn true;
}
""",
    "D3D11 queue submit",
)
save(path, text)


# ======================================================================
# RenderGraph queue assignment, synchronization plan and device execution.
# ======================================================================
path = "Source/GameApplication/Service/Graphics/RHI/RHIRenderGraph.h"
text = load(path)
text = replace_once(text, "#include <algorithm>\n", "#include <algorithm>\n#include <array>\n", "array include")
text = replace_once(text, "#include <limits>\n", "#include <limits>\n#include <memory>\n", "memory include")

text = replace_once(
    text,
    """struct RenderGraphResourceLifetime {
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
    """struct RenderGraphResourceLifetime {
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

struct RenderGraphQueueWait {
\tCommandQueueType sourceQueue = CommandQueueType::Graphics;
\tuint32_t producerPassIndex = InvalidRenderGraphIndex;
\tuint64_t value = 0;
};

struct RenderGraphPassQueueSync {
\tCommandQueueType queue = CommandQueueType::Graphics;
\tstd::vector<RenderGraphQueueWait> waits;
\tuint64_t signalValue = 0;

\tbool RequiresSynchronization() const noexcept {
\t\treturn !waits.empty() || signalValue != 0;
\t}
};

""",
    "RenderGraph queue sync types",
)

text = replace_once(
    text,
    """\tuint32_t AddPass(std::string name, const std::function<void(RenderGraphPassBuilder&)>& setup,
\t\tExecuteCallback execute){
\t\tRenderGraphPassBuilder builder;
\t\tif(setup) setup(builder);
\t\tPass pass;
\t\tpass.name = std::move(name);
\t\tpass.accesses = builder.Accesses();
\t\tpass.setupError = builder.Error();
\t\tpass.execute = std::move(execute);
\t\tm_passes.push_back(std::move(pass));
\t\tm_compiled = false;
\t\treturn static_cast<uint32_t>(m_passes.size() - 1);
\t}
""",
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
    "AddPass queue overload",
)

text = replace_once(
    text,
    """\t\tm_executionOrder.clear();
\t\tm_resourceLifetimes.clear();
\t\tm_finalStates.clear();
""",
    """\t\tm_executionOrder.clear();
\t\tm_resourceLifetimes.clear();
\t\tm_queueSync.clear();
\t\tm_finalStates.clear();
""",
    "Compile queue sync reset",
)
text = replace_once(
    text,
    """\t\tBuildResourceLifetimes();
\t\tif(!BuildBarriers()) return false;
""",
    """\t\tBuildResourceLifetimes();
\t\tBuildQueueSynchronization();
\t\tif(!BuildBarriers()) return false;
""",
    "Compile queue sync plan",
)

text = replace_once(
    text,
    """\tbool Execute(IRHICommandList& commandList){
\t\tif(!m_compiled && !Compile()) return false;
\t\tcommandList.Begin();
\t\tfor(uint32_t passIndex : m_executionOrder){
\t\t\tPass& pass = m_passes[passIndex];
\t\t\tif(!pass.barriers.empty() && !commandList.ResourceBarrier(pass.barriers)){
\t\t\t\tcommandList.End();
\t\t\t\tm_error = pass.name + ": backend rejected generated resource barriers.";
\t\t\t\treturn false;
\t\t\t}
\t\t\tif(pass.execute) pass.execute(commandList);
\t\t}
\t\tcommandList.End();
\t\treturn true;
\t}
""",
    """\tbool Execute(IRHICommandList& commandList){
\t\tif(!m_compiled && !Compile()) return false;
\t\tfor(uint32_t passIndex : m_executionOrder){
\t\t\tif(m_passes[passIndex].queue != commandList.GetQueueType()){
\t\t\t\treturn Fail(
\t\t\t\t\t"RenderGraph contains multiple queue domains; use Execute(IRHIDevice&)."
\t\t\t\t);
\t\t\t}
\t\t}

\t\tcommandList.Begin();
\t\tfor(uint32_t passIndex : m_executionOrder){
\t\t\tPass& pass = m_passes[passIndex];
\t\t\tif(!RecordPass(commandList, pass)){
\t\t\t\tcommandList.End();
\t\t\t\treturn false;
\t\t\t}
\t\t}
\t\tcommandList.End();
\t\treturn true;
\t}

\tbool Execute(IRHIDevice& device){
\t\tif(!m_compiled && !Compile()) return false;

\t\tstd::array<std::unique_ptr<IRHIFence>, 3> queueFences;
\t\tfor(const RenderGraphPassQueueSync& sync : m_queueSync){
\t\t\tif(sync.signalValue == 0) continue;
\t\t\tconst size_t queueIndex = QueueIndex(sync.queue);
\t\t\tif(!queueFences[queueIndex]){
\t\t\t\tqueueFences[queueIndex] = device.CreateFence(0);
\t\t\t\tif(!queueFences[queueIndex]){
\t\t\t\t\treturn Fail("RenderGraph failed to create a queue synchronization fence.");
\t\t\t\t}
\t\t\t}
\t\t}

\t\tfor(uint32_t passIndex : m_executionOrder){
\t\t\tPass& pass = m_passes[passIndex];
\t\t\tconst RenderGraphPassQueueSync& sync = m_queueSync[passIndex];
\t\t\tIRHICommandQueue* queue = device.GetQueue(pass.queue);
\t\t\tif(!queue){
\t\t\t\treturn Fail(pass.name + ": requested queue is not available.");
\t\t\t}

\t\t\tCommandListCreateDesc commandListDesc;
\t\t\tcommandListDesc.queueType = pass.queue;
\t\t\tstd::unique_ptr<IRHICommandList> commandList =
\t\t\t\tdevice.CreateCommandList(commandListDesc);
\t\t\tif(!commandList){
\t\t\t\treturn Fail(pass.name + ": failed to create a command list.");
\t\t\t}

\t\t\tcommandList->Begin();
\t\t\tif(!RecordPass(*commandList, pass)){
\t\t\t\tcommandList->End();
\t\t\t\treturn false;
\t\t\t}
\t\t\tcommandList->End();

\t\t\tstd::vector<QueueFenceWait> waits;
\t\t\twaits.reserve(sync.waits.size());
\t\t\tfor(const RenderGraphQueueWait& wait : sync.waits){
\t\t\t\tIRHIFence* fence = queueFences[QueueIndex(wait.sourceQueue)].get();
\t\t\t\tif(!fence){
\t\t\t\t\treturn Fail(pass.name + ": missing producer queue fence.");
\t\t\t\t}
\t\t\t\twaits.push_back({fence->GetHandle(), wait.value});
\t\t\t}

\t\t\tstd::array<QueueFenceSignal, 1> signals{};
\t\t\tstd::span<const QueueFenceSignal> signalSpan;
\t\t\tif(sync.signalValue != 0){
\t\t\t\tIRHIFence* fence = queueFences[QueueIndex(sync.queue)].get();
\t\t\t\tif(!fence){
\t\t\t\t\treturn Fail(pass.name + ": missing signal queue fence.");
\t\t\t\t}
\t\t\t\tsignals[0] = {fence->GetHandle(), sync.signalValue};
\t\t\t\tsignalSpan = signals;
\t\t\t}

\t\t\tstd::array<IRHICommandList*, 1> commandLists{commandList.get()};
\t\t\tQueueSubmitDesc submit;
\t\t\tsubmit.commandLists = commandLists;
\t\t\tsubmit.waits = waits;
\t\t\tsubmit.signals = signalSpan;
\t\t\tif(!queue->Submit(submit)){
\t\t\t\treturn Fail(pass.name + ": queue submission failed.");
\t\t\t}
\t\t}
\t\treturn true;
\t}
""",
    "RenderGraph multi-queue execution",
)

text = replace_once(
    text,
    """\tconst std::vector<ResourceBarrierDesc>& BarriersForPass(uint32_t passIndex) const noexcept {
\t\tstatic const std::vector<ResourceBarrierDesc> empty;
\t\treturn passIndex < m_passes.size() ? m_passes[passIndex].barriers : empty;
\t}

""",
    """\tconst std::vector<ResourceBarrierDesc>& BarriersForPass(uint32_t passIndex) const noexcept {
\t\tstatic const std::vector<ResourceBarrierDesc> empty;
\t\treturn passIndex < m_passes.size() ? m_passes[passIndex].barriers : empty;
\t}

\tCommandQueueType QueueForPass(uint32_t passIndex) const noexcept {
\t\treturn passIndex < m_passes.size()
\t\t\t? m_passes[passIndex].queue
\t\t\t: CommandQueueType::Graphics;
\t}

\tconst RenderGraphPassQueueSync& QueueSyncForPass(
\t\tuint32_t passIndex
\t) const noexcept {
\t\tstatic const RenderGraphPassQueueSync empty;
\t\treturn passIndex < m_queueSync.size() ? m_queueSync[passIndex] : empty;
\t}

\tbool RequiresQueueSynchronization() const noexcept {
\t\treturn std::any_of(
\t\t\tm_queueSync.begin(),
\t\t\tm_queueSync.end(),
\t\t\t[](const RenderGraphPassQueueSync& sync){
\t\t\t\treturn sync.RequiresSynchronization();
\t\t\t}
\t\t);
\t}

""",
    "Queue sync query API",
)

text = replace_once(
    text,
    """\t\tm_executionOrder.clear();
\t\tm_resourceLifetimes.clear();
\t\tm_finalStates.clear();
""",
    """\t\tm_executionOrder.clear();
\t\tm_resourceLifetimes.clear();
\t\tm_queueSync.clear();
\t\tm_finalStates.clear();
""",
    "Reset queue sync",
)

text = replace_once(
    text,
    """\tstruct Pass {
\t\tstd::string name;
\t\tstd::vector<RenderGraphAccess> accesses;
""",
    """\tstruct Pass {
\t\tstd::string name;
\t\tCommandQueueType queue = CommandQueueType::Graphics;
\t\tstd::vector<RenderGraphAccess> accesses;
""",
    "Pass queue field",
)

text = replace_once(
    text,
    """\tbool Fail(std::string message){ m_error = std::move(message); m_compiled = false; return false; }
\tstatic bool Writes(RenderGraphAccessType type){ return type != RenderGraphAccessType::Read; }
""",
    """\tbool Fail(std::string message){ m_error = std::move(message); m_compiled = false; return false; }

\tstatic size_t QueueIndex(CommandQueueType queue) noexcept {
\t\tswitch(queue){
\t\t\tcase CommandQueueType::Graphics: return 0;
\t\t\tcase CommandQueueType::Compute: return 1;
\t\t\tcase CommandQueueType::Copy: return 2;
\t\t}
\t\treturn 0;
\t}

\tbool RecordPass(IRHICommandList& commandList, Pass& pass){
\t\tif(!pass.barriers.empty() && !commandList.ResourceBarrier(pass.barriers)){
\t\t\tm_error = pass.name + ": backend rejected generated resource barriers.";
\t\t\treturn false;
\t\t}
\t\tif(pass.execute) pass.execute(commandList);
\t\treturn true;
\t}

\tstatic bool Writes(RenderGraphAccessType type){ return type != RenderGraphAccessType::Read; }
""",
    "RenderGraph queue helpers",
)

text = replace_once(
    text,
    """\tvoid AppendBarrier(Pass& pass, const Resource& resource, ResourceBarrierType type,
""",
    """\tvoid BuildQueueSynchronization(){
\t\tm_queueSync.assign(m_passes.size(), {});
\t\tstd::array<uint64_t, 3> nextSignalValue{1, 1, 1};

\t\tfor(uint32_t passIndex : m_executionOrder){
\t\t\tPass& pass = m_passes[passIndex];
\t\t\tRenderGraphPassQueueSync& sync = m_queueSync[passIndex];
\t\t\tsync.queue = pass.queue;

\t\t\tconst bool hasCrossQueueDependent = std::any_of(
\t\t\t\tpass.dependents.begin(),
\t\t\t\tpass.dependents.end(),
\t\t\t\t[this, &pass](uint32_t dependent){
\t\t\t\t\treturn m_passes[dependent].queue != pass.queue;
\t\t\t\t}
\t\t\t);
\t\t\tif(hasCrossQueueDependent){
\t\t\t\tsync.signalValue = nextSignalValue[QueueIndex(pass.queue)]++;
\t\t\t}
\t\t}

\t\tfor(uint32_t passIndex : m_executionOrder){
\t\t\tconst Pass& pass = m_passes[passIndex];
\t\t\tRenderGraphPassQueueSync& sync = m_queueSync[passIndex];
\t\t\tstd::array<uint64_t, 3> waitValues{};
\t\t\tstd::array<uint32_t, 3> producers{
\t\t\t\tInvalidRenderGraphIndex,
\t\t\t\tInvalidRenderGraphIndex,
\t\t\t\tInvalidRenderGraphIndex
\t\t\t};

\t\t\tfor(uint32_t dependency : pass.dependencies){
\t\t\t\tconst Pass& producer = m_passes[dependency];
\t\t\t\tif(producer.queue == pass.queue) continue;
\t\t\t\tconst size_t sourceIndex = QueueIndex(producer.queue);
\t\t\t\tconst uint64_t value = m_queueSync[dependency].signalValue;
\t\t\t\tif(value > waitValues[sourceIndex]){
\t\t\t\t\twaitValues[sourceIndex] = value;
\t\t\t\t\tproducers[sourceIndex] = dependency;
\t\t\t\t}
\t\t\t}

\t\t\tfor(size_t sourceIndex = 0; sourceIndex < waitValues.size(); ++sourceIndex){
\t\t\t\tif(waitValues[sourceIndex] == 0) continue;
\t\t\t\tsync.waits.push_back({
\t\t\t\t\tm_passes[producers[sourceIndex]].queue,
\t\t\t\t\tproducers[sourceIndex],
\t\t\t\t\twaitValues[sourceIndex]
\t\t\t\t});
\t\t\t}
\t\t}
\t}

\tvoid AppendBarrier(Pass& pass, const Resource& resource, ResourceBarrierType type,
""",
    "BuildQueueSynchronization implementation",
)

text = replace_once(
    text,
    """\tstd::vector<uint32_t> m_executionOrder;
\tstd::vector<RenderGraphResourceLifetime> m_resourceLifetimes;
\tstd::vector<std::vector<ResourceState>> m_finalStates;
""",
    """\tstd::vector<uint32_t> m_executionOrder;
\tstd::vector<RenderGraphResourceLifetime> m_resourceLifetimes;
\tstd::vector<RenderGraphPassQueueSync> m_queueSync;
\tstd::vector<std::vector<ResourceState>> m_finalStates;
""",
    "Queue sync member storage",
)
save(path, text)


# ======================================================================
# Contract test. It compiles automatically and executes only manually.
# ======================================================================
path = "Tests/RHIRenderGraphBarrierSmokeTest.cpp"
text = load(path)
text = replace_once(
    text,
    """void TestTransientLifetimeAnalysis(){
""",
    """void TestQueueSynchronization(){
\tRHI::NullRHIDevice device({});
\tRHI::RenderGraph graph;
\tconst auto graphicsOutput = graph.AddResource("GraphicsOutput");
\tconst auto computeOutput = graph.AddResource("ComputeOutput");
\tconst auto copyOutput = graph.AddResource("CopyOutput");
\tstd::vector<RHI::CommandQueueType> executionQueues;

\tconst uint32_t graphicsProducer = graph.AddPass(
\t\t"GraphicsProducer",
\t\tRHI::CommandQueueType::Graphics,
\t\t[&](RHI::RenderGraphPassBuilder& builder){
\t\t\tbuilder.Write(graphicsOutput);
\t\t},
\t\t[&](RHI::IRHICommandList& commandList){
\t\t\texecutionQueues.push_back(commandList.GetQueueType());
\t\t}
\t);
\tconst uint32_t computeProducer = graph.AddPass(
\t\t"ComputeProducer",
\t\tRHI::CommandQueueType::Compute,
\t\t[&](RHI::RenderGraphPassBuilder& builder){
\t\t\tbuilder.Write(computeOutput);
\t\t},
\t\t[&](RHI::IRHICommandList& commandList){
\t\t\texecutionQueues.push_back(commandList.GetQueueType());
\t\t}
\t);
\tconst uint32_t copyJoin = graph.AddPass(
\t\t"CopyJoin",
\t\tRHI::CommandQueueType::Copy,
\t\t[&](RHI::RenderGraphPassBuilder& builder){
\t\t\tbuilder.Read(graphicsOutput);
\t\t\tbuilder.Read(computeOutput);
\t\t\tbuilder.Write(copyOutput);
\t\t},
\t\t[&](RHI::IRHICommandList& commandList){
\t\t\texecutionQueues.push_back(commandList.GetQueueType());
\t\t}
\t);
\tconst uint32_t graphicsConsumer = graph.AddPass(
\t\t"GraphicsConsumer",
\t\tRHI::CommandQueueType::Graphics,
\t\t[&](RHI::RenderGraphPassBuilder& builder){
\t\t\tbuilder.Read(copyOutput);
\t\t},
\t\t[&](RHI::IRHICommandList& commandList){
\t\t\texecutionQueues.push_back(commandList.GetQueueType());
\t\t}
\t);

\tassert(graph.Compile());
\tassert(graph.RequiresQueueSynchronization());
\tassert(graph.QueueForPass(graphicsProducer) == RHI::CommandQueueType::Graphics);
\tassert(graph.QueueForPass(computeProducer) == RHI::CommandQueueType::Compute);
\tassert(graph.QueueForPass(copyJoin) == RHI::CommandQueueType::Copy);
\tassert(graph.QueueSyncForPass(graphicsProducer).signalValue == 1);
\tassert(graph.QueueSyncForPass(computeProducer).signalValue == 1);
\tassert(graph.QueueSyncForPass(copyJoin).waits.size() == 2);
\tassert(graph.QueueSyncForPass(copyJoin).signalValue == 1);
\tassert(graph.QueueSyncForPass(graphicsConsumer).waits.size() == 1);
\tassert(graph.QueueSyncForPass(graphicsConsumer).waits[0].sourceQueue ==
\t\tRHI::CommandQueueType::Copy);
\tassert(graph.Execute(device));
\tassert((executionQueues == std::vector<RHI::CommandQueueType>{
\t\tRHI::CommandQueueType::Graphics,
\t\tRHI::CommandQueueType::Compute,
\t\tRHI::CommandQueueType::Copy,
\t\tRHI::CommandQueueType::Graphics
\t}));
}

void TestTransientLifetimeAnalysis(){
""",
    "Queue synchronization test insertion",
)
text = replace_once(
    text,
    """\tTestInvalidSubresourceDeclarations();
\tTestTransientLifetimeAnalysis();
""",
    """\tTestInvalidSubresourceDeclarations();
\tTestQueueSynchronization();
\tTestTransientLifetimeAnalysis();
""",
    "Queue synchronization test registration",
)
save(path, text)


# ======================================================================
# Documentation.
# ======================================================================
path = "Docs/ECS_Scheduler_Migration_Plan.md"
text = load(path)
text = replace_once(text, "- [ ] Queue間同期", "- [x] Queue間同期", "Queue sync checkbox")
text = replace_once(
    text,
    """1. Step 16-FのQueue間同期
2. Step 16-FのPass Culling
3. D3D11最小実描画Triangle Test
4. 既存Player View実機回帰
5. Step 16完了報告
6. Step 17 RenderWorld移行
""",
    """1. Step 16-FのPass Culling
2. D3D11最小実描画Triangle Test
3. 既存Player View実機回帰
4. Step 16完了報告
5. Step 17 RenderWorld移行
""",
    "Current work position",
)
save(path, text)

Path("Docs/Step16F_Queue_Synchronization_Completion.md").write_text(
    """# Step 16-F: Queue Synchronization Completion

## 完了内容

RenderGraph PassへGraphics / Compute / CopyのQueue Domainを追加し、Hazard依存からQueue間Fence同期計画を自動生成する仕組みを追加した。

- 同一Queueの依存はQueue内の提出順で保証する
- 異なるQueueへ値を渡すProducer PassはQueue固有Fence ValueをSignalする
- Consumer Passは依存元Queueごとの最新Fence ValueをWaitする
- 複数Queueへ依存するPassは複数Fence Waitを一度のSubmitで宣言できる
- RenderGraphがDeviceからCommand ListとFenceを生成し、Pass単位でQueueへSubmitする
- 既存の単一Command List実行は単一Queue Graphだけに制限する

## D3D11契約

D3D11のGraphics / Compute / Copyは論理Queueであり、Immediate Contextへ直列化する。Queue間WaitはCPU側でEVENT Query完了を待ち、SignalはEVENT Queryを挿入する。`supportsMultipleCommandQueues`と`supportsAsyncCompute`はfalseのままで、Native並列実行を偽装しない。

## 将来Backend契約

D3D12 / Vulkan Backendは`QueueSubmitDesc::waits`と`signals`をNative Queue同期へ変換する。FenceはQueueごとに単調増加させるため、異なるQueueで同じValueを使用しても相互に干渉しない。

## 検証方針

通常CIではNull Backendを使う複数Queue依存Contract Testをコンパイルする。実行を伴うTestは手動`workflow_dispatch`に限定する。
""",
    encoding="utf-8",
    newline="\n",
)

print("RenderGraph queue synchronization migration applied")
