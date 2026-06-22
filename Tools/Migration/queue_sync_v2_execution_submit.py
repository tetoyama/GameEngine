from queue_sync_v2_common import load, replace_once, save

path = "Source/GameApplication/Service/Graphics/RHI/RHIRenderGraph.h"
text = load(path)
text = replace_once(
    text,
    """\t\tstd::array<std::unique_ptr<IRHIFence>, 3> queueFences;
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
""",
    """\t\tif(m_executionDevice && m_executionDevice != &device){
\t\t\treturn Fail(
\t\t\t\t"RenderGraph execution device changed; release execution state before reusing the graph."
\t\t\t);
\t\t}
\t\tm_executionDevice = &device;

\t\tconst std::array<uint64_t, 3> executionBase = m_queueFenceValues;
\t\tfor(const RenderGraphPassQueueSync& sync : m_queueSync){
\t\t\tif(sync.signalValue == 0) continue;
\t\t\tconst size_t queueIndex = QueueIndex(sync.queue);
\t\t\tif(!m_queueFences[queueIndex]){
\t\t\t\tm_queueFences[queueIndex] = device.CreateFence(0);
\t\t\t\tif(!m_queueFences[queueIndex]){
\t\t\t\t\treturn Fail("RenderGraph failed to create a queue synchronization fence.");
\t\t\t\t}
\t\t\t}
\t\t}
""",
    "reusable queue fence initialization",
)
text = text.replace(
    "queueFences[QueueIndex(wait.sourceQueue)].get()",
    "m_queueFences[QueueIndex(wait.sourceQueue)].get()",
)
text = text.replace(
    "queueFences[QueueIndex(sync.queue)].get()",
    "m_queueFences[QueueIndex(sync.queue)].get()",
)
text = replace_once(
    text,
    """\t\t\t\twaits.push_back({fence->GetHandle(), wait.value});
""",
    """\t\t\t\twaits.push_back({
\t\t\t\t\tfence->GetHandle(),
\t\t\t\t\texecutionBase[QueueIndex(wait.sourceQueue)] + wait.value
\t\t\t\t});
""",
    "timeline wait value",
)
text = replace_once(
    text,
    """\t\t\t\tsignals[0] = {fence->GetHandle(), sync.signalValue};
""",
    """\t\t\t\tsignals[0] = {
\t\t\t\t\tfence->GetHandle(),
\t\t\t\t\texecutionBase[QueueIndex(sync.queue)] + sync.signalValue
\t\t\t\t};
""",
    "timeline signal value",
)
text = replace_once(
    text,
    """\t\t\tif(!queue->Submit(submit)){
\t\t\t\treturn Fail(pass.name + ": queue submission failed.");
\t\t\t}
\t\t}
\t\treturn true;
\t}
""",
    """\t\t\tif(!queue->Submit(submit)){
\t\t\t\treturn Fail(pass.name + ": queue submission failed.");
\t\t\t}
\t\t\tif(sync.signalValue != 0){
\t\t\t\tm_queueFenceValues[QueueIndex(sync.queue)] = signals[0].value;
\t\t\t}
\t\t}
\t\treturn true;
\t}
""",
    "persist submitted timeline values",
)
save(path, text)

print("RenderGraph queue submission hardened")
