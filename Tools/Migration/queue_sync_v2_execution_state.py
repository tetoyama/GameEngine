from queue_sync_v2_common import load, replace_once, save

path = "Source/GameApplication/Service/Graphics/RHI/RHIRenderGraph.h"
text = load(path)
text = replace_once(
    text,
    """\tbool RequiresQueueSynchronization() const noexcept {
\t\treturn std::any_of(
\t\t\tm_queueSync.begin(),
\t\t\tm_queueSync.end(),
\t\t\t[](const RenderGraphPassQueueSync& sync){
\t\t\t\treturn sync.RequiresSynchronization();
\t\t\t}
\t\t);
\t}

""",
    """\tbool RequiresQueueSynchronization() const noexcept {
\t\treturn std::any_of(
\t\t\tm_queueSync.begin(),
\t\t\tm_queueSync.end(),
\t\t\t[](const RenderGraphPassQueueSync& sync){
\t\t\t\treturn sync.RequiresSynchronization();
\t\t\t}
\t\t);
\t}

\tuint64_t LastSubmittedQueueFenceValue(CommandQueueType queue) const noexcept {
\t\treturn m_queueFenceValues[QueueIndex(queue)];
\t}

\tbool ReleaseExecutionState(IRHIDevice& device){
\t\tif(!m_executionDevice) return true;
\t\tif(m_executionDevice != &device) return false;

\t\tdevice.WaitIdle();
\t\tbool released = true;
\t\tfor(std::unique_ptr<IRHIFence>& fence : m_queueFences){
\t\t\tif(!fence) continue;
\t\t\tconst FenceHandle handle = fence->GetHandle();
\t\t\tif(!device.DestroyFence(handle)) released = false;
\t\t\tfence.reset();
\t\t}
\t\tm_queueFenceValues.fill(0);
\t\tm_executionDevice = nullptr;
\t\treturn released;
\t}

""",
    "RenderGraph execution state API",
)
text = replace_once(
    text,
    """\tstd::vector<RenderGraphPassQueueSync> m_queueSync;
\tstd::vector<std::vector<ResourceState>> m_finalStates;
""",
    """\tstd::vector<RenderGraphPassQueueSync> m_queueSync;
\tstd::vector<std::vector<ResourceState>> m_finalStates;
\tIRHIDevice* m_executionDevice = nullptr;
\tstd::array<std::unique_ptr<IRHIFence>, 3> m_queueFences;
\tstd::array<uint64_t, 3> m_queueFenceValues{};
""",
    "RenderGraph persistent execution state",
)
save(path, text)

print("RenderGraph execution state added")
