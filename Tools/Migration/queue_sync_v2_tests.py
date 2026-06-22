from queue_sync_v2_common import load, replace_once, save

path = "Tests/RHIRenderGraphBarrierSmokeTest.cpp"
text = load(path)
text = replace_once(
    text,
    """\tassert(graph.Execute(device));
\tassert((executionQueues == std::vector<RHI::CommandQueueType>{
\t\tRHI::CommandQueueType::Graphics,
\t\tRHI::CommandQueueType::Compute,
\t\tRHI::CommandQueueType::Copy,
\t\tRHI::CommandQueueType::Graphics
\t}));
}

void TestTransientLifetimeAnalysis(){
""",
    """\tassert(graph.Execute(device));
\tassert((executionQueues == std::vector<RHI::CommandQueueType>{
\t\tRHI::CommandQueueType::Graphics,
\t\tRHI::CommandQueueType::Compute,
\t\tRHI::CommandQueueType::Copy,
\t\tRHI::CommandQueueType::Graphics
\t}));
\tassert(graph.LastSubmittedQueueFenceValue(RHI::CommandQueueType::Graphics) == 1);
\tassert(graph.LastSubmittedQueueFenceValue(RHI::CommandQueueType::Compute) == 1);
\tassert(graph.LastSubmittedQueueFenceValue(RHI::CommandQueueType::Copy) == 1);

\texecutionQueues.clear();
\tassert(graph.Execute(device));
\tassert((executionQueues == std::vector<RHI::CommandQueueType>{
\t\tRHI::CommandQueueType::Graphics,
\t\tRHI::CommandQueueType::Compute,
\t\tRHI::CommandQueueType::Copy,
\t\tRHI::CommandQueueType::Graphics
\t}));
\tassert(graph.LastSubmittedQueueFenceValue(RHI::CommandQueueType::Graphics) == 2);
\tassert(graph.LastSubmittedQueueFenceValue(RHI::CommandQueueType::Compute) == 2);
\tassert(graph.LastSubmittedQueueFenceValue(RHI::CommandQueueType::Copy) == 2);
\tassert(graph.ReleaseExecutionState(device));
}

void TestQueueSharingValidation(){
\tRHI::NullRHIDevice device({});

\tRHI::BufferDesc exclusiveDesc;
\texclusiveDesc.byteSize = 64;
\tconst RHI::BufferHandle exclusiveBuffer = device.CreateBuffer(exclusiveDesc, {});
\tassert(exclusiveBuffer);

\tRHI::RenderGraph exclusiveGraph;
\tconst auto exclusiveResource = exclusiveGraph.ImportBuffer(
\t\texclusiveBuffer,
\t\texclusiveDesc.initialState,
\t\texclusiveDesc,
\t\t"ExclusiveBuffer"
\t);
\texclusiveGraph.AddPass(
\t\t"GraphicsWrite",
\t\tRHI::CommandQueueType::Graphics,
\t\t[&](RHI::RenderGraphPassBuilder& builder){ builder.Write(exclusiveResource); },
\t\t{}
\t);
\texclusiveGraph.AddPass(
\t\t"ComputeRead",
\t\tRHI::CommandQueueType::Compute,
\t\t[&](RHI::RenderGraphPassBuilder& builder){ builder.Read(exclusiveResource); },
\t\t{}
\t);
\tassert(!exclusiveGraph.Compile());
\tassert(!exclusiveGraph.Error().empty());

\tRHI::BufferDesc concurrentDesc;
\tconcurrentDesc.byteSize = 64;
\tconcurrentDesc.queueSharingMode = RHI::ResourceQueueSharingMode::Concurrent;
\tconst RHI::BufferHandle concurrentBuffer = device.CreateBuffer(concurrentDesc, {});
\tassert(concurrentBuffer);

\tRHI::RenderGraph concurrentGraph;
\tconst auto concurrentResource = concurrentGraph.ImportBuffer(
\t\tconcurrentBuffer,
\t\tconcurrentDesc.initialState,
\t\tconcurrentDesc,
\t\t"ConcurrentBuffer"
\t);
\tconcurrentGraph.AddPass(
\t\t"GraphicsWrite",
\t\tRHI::CommandQueueType::Graphics,
\t\t[&](RHI::RenderGraphPassBuilder& builder){ builder.Write(concurrentResource); },
\t\t{}
\t);
\tconcurrentGraph.AddPass(
\t\t"ComputeRead",
\t\tRHI::CommandQueueType::Compute,
\t\t[&](RHI::RenderGraphPassBuilder& builder){ builder.Read(concurrentResource); },
\t\t{}
\t);
\tassert(concurrentGraph.Compile());
\tassert(concurrentGraph.RequiresQueueSynchronization());
\tassert(concurrentGraph.Execute(device));
\tassert(concurrentGraph.ReleaseExecutionState(device));
}

void TestTransientLifetimeAnalysis(){
""",
    "queue lifetime and sharing tests",
)
text = replace_once(
    text,
    """\tTestQueueSynchronization();
\tTestTransientLifetimeAnalysis();
""",
    """\tTestQueueSynchronization();
\tTestQueueSharingValidation();
\tTestTransientLifetimeAnalysis();
""",
    "queue sharing test registration",
)
save(path, text)

print("Queue sync contract tests extended")
