#include <algorithm>
#include <cassert>
#include <vector>

#include "Service/Graphics/RHI/Null/NullRHIBackend.h"
#include "Service/Graphics/RHI/RHIRenderGraph.h"

namespace {

void TestAutomaticTransitions(){
	RHI::NullRHIDevice device({});

	RHI::BufferDesc desc;
	desc.byteSize = 256;
	desc.initialState = RHI::ResourceState::Common;
	const RHI::BufferHandle buffer = device.CreateBuffer(desc, {});
	assert(buffer);

	RHI::RenderGraph graph;
	const RHI::RenderGraphResource resource = graph.ImportBuffer(
		buffer,
		desc.initialState,
		"FrameBuffer"
	);
	assert(resource);

	std::vector<int> execution;
	const uint32_t uploadPass = graph.AddPass(
		"Upload",
		[&](RHI::RenderGraphPassBuilder& builder){
			builder.Write(resource, RHI::ResourceState::CopyDestination);
		},
		[&](RHI::IRHICommandList&){ execution.push_back(1); }
	);
	const uint32_t readPass = graph.AddPass(
		"Read",
		[&](RHI::RenderGraphPassBuilder& builder){
			builder.Read(resource, RHI::ResourceState::ShaderResource);
		},
		[&](RHI::IRHICommandList&){ execution.push_back(2); }
	);
	const uint32_t computePass = graph.AddPass(
		"ComputeWrite",
		[&](RHI::RenderGraphPassBuilder& builder){
			builder.Write(resource, RHI::ResourceState::UnorderedAccess);
		},
		[&](RHI::IRHICommandList&){ execution.push_back(3); }
	);
	const uint32_t repeatedComputePass = graph.AddPass(
		"ComputeWriteAgain",
		[&](RHI::RenderGraphPassBuilder& builder){
			builder.Write(resource, RHI::ResourceState::UnorderedAccess);
		},
		[&](RHI::IRHICommandList&){ execution.push_back(4); }
	);

	assert(graph.Compile());
	assert((graph.ExecutionOrder() == std::vector<uint32_t>{
		uploadPass,
		readPass,
		computePass,
		repeatedComputePass
	}));

	const auto& uploadBarriers = graph.BarriersForPass(uploadPass);
	assert(uploadBarriers.size() == 1);
	assert(uploadBarriers[0].type == RHI::ResourceBarrierType::Transition);
	assert(uploadBarriers[0].before == RHI::ResourceState::Common);
	assert(uploadBarriers[0].after == RHI::ResourceState::CopyDestination);
	assert(uploadBarriers[0].subresource == RHI::AllSubresources);

	const auto& readBarriers = graph.BarriersForPass(readPass);
	assert(readBarriers.size() == 1);
	assert(readBarriers[0].before == RHI::ResourceState::CopyDestination);
	assert(readBarriers[0].after == RHI::ResourceState::ShaderResource);

	const auto& computeBarriers = graph.BarriersForPass(computePass);
	assert(computeBarriers.size() == 1);
	assert(computeBarriers[0].before == RHI::ResourceState::ShaderResource);
	assert(computeBarriers[0].after == RHI::ResourceState::UnorderedAccess);

	const auto& repeatedBarriers = graph.BarriersForPass(repeatedComputePass);
	assert(repeatedBarriers.size() == 1);
	assert(repeatedBarriers[0].type == RHI::ResourceBarrierType::UnorderedAccess);
	assert(repeatedBarriers[0].before == RHI::ResourceState::UnorderedAccess);
	assert(repeatedBarriers[0].after == RHI::ResourceState::UnorderedAccess);
	assert(graph.FinalState(resource) == RHI::ResourceState::UnorderedAccess);
	assert(graph.FinalState(resource, 0) == RHI::ResourceState::UnorderedAccess);

	auto commandList = device.CreateCommandList({
		RHI::CommandQueueType::Graphics,
		false
	});
	assert(commandList);
	assert(graph.Execute(*commandList));
	assert((execution == std::vector<int>{1, 2, 3, 4}));
}

void TestSubresourceTransitions(){
	RHI::NullRHIDevice device({});

	RHI::TextureDesc desc;
	desc.width = 64;
	desc.height = 64;
	desc.mipLevels = 4;
	desc.initialState = RHI::ResourceState::Common;
	const RHI::TextureHandle texture = device.CreateTexture(desc, {}, 0);
	assert(texture);

	RHI::RenderGraph graph;
	const RHI::RenderGraphResource resource = graph.ImportTexture(
		texture,
		desc.initialState,
		desc,
		"MipChain"
	);
	assert(resource);

	const uint32_t writeMip0 = graph.AddPass(
		"WriteMip0",
		[&](RHI::RenderGraphPassBuilder& builder){
			builder.Write(
				resource,
				RHI::ResourceState::CopyDestination,
				RHI::RenderGraphSubresourceRange::Single(0)
			);
		},
		{}
	);
	const uint32_t writeMip1 = graph.AddPass(
		"WriteMip1",
		[&](RHI::RenderGraphPassBuilder& builder){
			builder.Write(
				resource,
				RHI::ResourceState::RenderTarget,
				RHI::RenderGraphSubresourceRange::Single(1)
			);
		},
		{}
	);
	const uint32_t readMip0 = graph.AddPass(
		"ReadMip0",
		[&](RHI::RenderGraphPassBuilder& builder){
			builder.Read(
				resource,
				RHI::ResourceState::ShaderResource,
				RHI::RenderGraphSubresourceRange::Single(0)
			);
		},
		{}
	);

	assert(graph.Compile());
	assert(graph.DependenciesForPass(writeMip0).empty());
	assert(graph.DependenciesForPass(writeMip1).empty());
	assert((graph.DependenciesForPass(readMip0) == std::vector<uint32_t>{writeMip0}));

	const auto& mip0WriteBarriers = graph.BarriersForPass(writeMip0);
	assert(mip0WriteBarriers.size() == 1);
	assert(mip0WriteBarriers[0].subresource == 0);
	assert(mip0WriteBarriers[0].before == RHI::ResourceState::Common);
	assert(mip0WriteBarriers[0].after == RHI::ResourceState::CopyDestination);

	const auto& mip1WriteBarriers = graph.BarriersForPass(writeMip1);
	assert(mip1WriteBarriers.size() == 1);
	assert(mip1WriteBarriers[0].subresource == 1);
	assert(mip1WriteBarriers[0].before == RHI::ResourceState::Common);
	assert(mip1WriteBarriers[0].after == RHI::ResourceState::RenderTarget);

	const auto& mip0ReadBarriers = graph.BarriersForPass(readMip0);
	assert(mip0ReadBarriers.size() == 1);
	assert(mip0ReadBarriers[0].subresource == 0);
	assert(mip0ReadBarriers[0].before == RHI::ResourceState::CopyDestination);
	assert(mip0ReadBarriers[0].after == RHI::ResourceState::ShaderResource);

	assert(graph.FinalState(resource) == RHI::ResourceState::Undefined);
	assert(graph.FinalState(resource, 0) == RHI::ResourceState::ShaderResource);
	assert(graph.FinalState(resource, 1) == RHI::ResourceState::RenderTarget);
	assert(graph.FinalState(resource, 2) == RHI::ResourceState::Common);
	assert(graph.FinalState(resource, 3) == RHI::ResourceState::Common);

	const uint32_t readAll = graph.AddPass(
		"ReadAllMips",
		[&](RHI::RenderGraphPassBuilder& builder){
			builder.Read(resource, RHI::ResourceState::ShaderResource);
		},
		{}
	);
	assert(graph.Compile());

	const auto& readAllBarriers = graph.BarriersForPass(readAll);
	assert(readAllBarriers.size() == 3);
	assert(std::none_of(
		readAllBarriers.begin(),
		readAllBarriers.end(),
		[](const RHI::ResourceBarrierDesc& barrier){
			return barrier.subresource == 0;
		}
	));
	assert(std::all_of(
		readAllBarriers.begin(),
		readAllBarriers.end(),
		[](const RHI::ResourceBarrierDesc& barrier){
			return barrier.type == RHI::ResourceBarrierType::Transition &&
				barrier.after == RHI::ResourceState::ShaderResource;
		}
	));
	assert(graph.FinalState(resource) == RHI::ResourceState::ShaderResource);

	auto commandList = device.CreateCommandList({
		RHI::CommandQueueType::Graphics,
		false
	});
	assert(commandList);
	assert(graph.Execute(*commandList));
}

void TestInvalidStateDeclarations(){
	RHI::NullRHIDevice device({});
	RHI::BufferDesc desc;
	desc.byteSize = 64;
	const RHI::BufferHandle buffer = device.CreateBuffer(desc, {});

	RHI::RenderGraph conflicting;
	const auto imported = conflicting.ImportBuffer(
		buffer,
		RHI::ResourceState::Common,
		"Conflicting"
	);
	conflicting.AddPass(
		"InvalidPass",
		[&](RHI::RenderGraphPassBuilder& builder){
			builder.Read(imported, RHI::ResourceState::ShaderResource);
			builder.Write(imported, RHI::ResourceState::CopyDestination);
		},
		{}
	);
	assert(!conflicting.Compile());
	assert(!conflicting.Error().empty());

	RHI::RenderGraph unbound;
	const auto logical = unbound.AddResource("LogicalOnly");
	unbound.AddPass(
		"InvalidStatePass",
		[&](RHI::RenderGraphPassBuilder& builder){
			builder.Read(logical, RHI::ResourceState::ShaderResource);
		},
		{}
	);
	assert(!unbound.Compile());
	assert(!unbound.Error().empty());
}

void TestInvalidSubresourceDeclarations(){
	RHI::NullRHIDevice device({});

	RHI::TextureDesc textureDesc;
	textureDesc.width = 32;
	textureDesc.height = 32;
	textureDesc.mipLevels = 4;
	const RHI::TextureHandle texture = device.CreateTexture(textureDesc, {}, 0);
	assert(texture);

	RHI::RenderGraph outOfBounds;
	const auto importedTexture = outOfBounds.ImportTexture(
		texture,
		textureDesc.initialState,
		textureDesc,
		"OutOfBounds"
	);
	outOfBounds.AddPass(
		"InvalidMipRange",
		[&](RHI::RenderGraphPassBuilder& builder){
			builder.Read(
				importedTexture,
				RHI::ResourceState::ShaderResource,
				RHI::RenderGraphSubresourceRange::Make(3, 2)
			);
		},
		{}
	);
	assert(!outOfBounds.Compile());
	assert(!outOfBounds.Error().empty());

	RHI::BufferDesc bufferDesc;
	bufferDesc.byteSize = 64;
	const RHI::BufferHandle buffer = device.CreateBuffer(bufferDesc, {});
	assert(buffer);

	RHI::RenderGraph bufferSubresource;
	const auto importedBuffer = bufferSubresource.ImportBuffer(
		buffer,
		bufferDesc.initialState,
		"BufferSubresource"
	);
	bufferSubresource.AddPass(
		"InvalidBufferRange",
		[&](RHI::RenderGraphPassBuilder& builder){
			builder.Read(
				importedBuffer,
				RHI::ResourceState::ShaderResource,
				RHI::RenderGraphSubresourceRange::Single(0)
			);
		},
		{}
	);
	assert(!bufferSubresource.Compile());
	assert(!bufferSubresource.Error().empty());

	RHI::RenderGraph overlapping;
	const auto overlappingTexture = overlapping.ImportTexture(
		texture,
		textureDesc.initialState,
		textureDesc,
		"Overlapping"
	);
	overlapping.AddPass(
		"OverlappingRanges",
		[&](RHI::RenderGraphPassBuilder& builder){
			builder.Read(
				overlappingTexture,
				RHI::ResourceState::ShaderResource,
				RHI::RenderGraphSubresourceRange::Make(0, 2)
			);
			builder.Write(
				overlappingTexture,
				RHI::ResourceState::RenderTarget,
				RHI::RenderGraphSubresourceRange::Make(1, 2)
			);
		},
		{}
	);
	assert(!overlapping.Compile());
	assert(!overlapping.Error().empty());

	RHI::RenderGraph disjoint;
	const auto disjointTexture = disjoint.ImportTexture(
		texture,
		textureDesc.initialState,
		textureDesc,
		"Disjoint"
	);
	disjoint.AddPass(
		"DisjointRanges",
		[&](RHI::RenderGraphPassBuilder& builder){
			builder.Write(
				disjointTexture,
				RHI::ResourceState::RenderTarget,
				RHI::RenderGraphSubresourceRange::Single(0)
			);
			builder.Read(
				disjointTexture,
				RHI::ResourceState::ShaderResource,
				RHI::RenderGraphSubresourceRange::Single(2)
			);
		},
		{}
	);
	assert(disjoint.Compile());
}

void TestQueueSynchronization(){
	RHI::NullRHIDevice device({});
	RHI::RenderGraph graph;
	const auto graphicsOutput = graph.AddResource("GraphicsOutput");
	const auto computeOutput = graph.AddResource("ComputeOutput");
	const auto copyOutput = graph.AddResource("CopyOutput");
	std::vector<RHI::CommandQueueType> executionQueues;

	const uint32_t graphicsProducer = graph.AddPass(
		"GraphicsProducer",
		RHI::CommandQueueType::Graphics,
		[&](RHI::RenderGraphPassBuilder& builder){
			builder.Write(graphicsOutput);
		},
		[&](RHI::IRHICommandList& commandList){
			executionQueues.push_back(commandList.GetQueueType());
		}
	);
	const uint32_t computeProducer = graph.AddPass(
		"ComputeProducer",
		RHI::CommandQueueType::Compute,
		[&](RHI::RenderGraphPassBuilder& builder){
			builder.Write(computeOutput);
		},
		[&](RHI::IRHICommandList& commandList){
			executionQueues.push_back(commandList.GetQueueType());
		}
	);
	const uint32_t copyJoin = graph.AddPass(
		"CopyJoin",
		RHI::CommandQueueType::Copy,
		[&](RHI::RenderGraphPassBuilder& builder){
			builder.Read(graphicsOutput);
			builder.Read(computeOutput);
			builder.Write(copyOutput);
		},
		[&](RHI::IRHICommandList& commandList){
			executionQueues.push_back(commandList.GetQueueType());
		}
	);
	const uint32_t graphicsConsumer = graph.AddPass(
		"GraphicsConsumer",
		RHI::CommandQueueType::Graphics,
		[&](RHI::RenderGraphPassBuilder& builder){
			builder.Read(copyOutput);
		},
		[&](RHI::IRHICommandList& commandList){
			executionQueues.push_back(commandList.GetQueueType());
		}
	);

	assert(graph.Compile());
	assert(graph.RequiresQueueSynchronization());
	assert(graph.QueueForPass(graphicsProducer) == RHI::CommandQueueType::Graphics);
	assert(graph.QueueForPass(computeProducer) == RHI::CommandQueueType::Compute);
	assert(graph.QueueForPass(copyJoin) == RHI::CommandQueueType::Copy);
	assert(graph.QueueSyncForPass(graphicsProducer).signalValue == 1);
	assert(graph.QueueSyncForPass(computeProducer).signalValue == 1);
	assert(graph.QueueSyncForPass(copyJoin).waits.size() == 2);
	assert(graph.QueueSyncForPass(copyJoin).signalValue == 1);
	assert(graph.QueueSyncForPass(graphicsConsumer).waits.size() == 1);
	assert(graph.QueueSyncForPass(graphicsConsumer).waits[0].sourceQueue ==
		RHI::CommandQueueType::Copy);
	assert(graph.Execute(device));
	assert((executionQueues == std::vector<RHI::CommandQueueType>{
		RHI::CommandQueueType::Graphics,
		RHI::CommandQueueType::Compute,
		RHI::CommandQueueType::Copy,
		RHI::CommandQueueType::Graphics
	}));
	assert(graph.LastSubmittedQueueFenceValue(RHI::CommandQueueType::Graphics) == 1);
	assert(graph.LastSubmittedQueueFenceValue(RHI::CommandQueueType::Compute) == 1);
	assert(graph.LastSubmittedQueueFenceValue(RHI::CommandQueueType::Copy) == 1);

	executionQueues.clear();
	assert(graph.Execute(device));
	assert((executionQueues == std::vector<RHI::CommandQueueType>{
		RHI::CommandQueueType::Graphics,
		RHI::CommandQueueType::Compute,
		RHI::CommandQueueType::Copy,
		RHI::CommandQueueType::Graphics
	}));
	assert(graph.LastSubmittedQueueFenceValue(RHI::CommandQueueType::Graphics) == 2);
	assert(graph.LastSubmittedQueueFenceValue(RHI::CommandQueueType::Compute) == 2);
	assert(graph.LastSubmittedQueueFenceValue(RHI::CommandQueueType::Copy) == 2);
	assert(graph.ReleaseExecutionState(device));
}

void TestQueueSharingValidation(){
	RHI::NullRHIDevice device({});

	RHI::BufferDesc exclusiveDesc;
	exclusiveDesc.byteSize = 64;
	const RHI::BufferHandle exclusiveBuffer = device.CreateBuffer(exclusiveDesc, {});
	assert(exclusiveBuffer);

	RHI::RenderGraph exclusiveGraph;
	const auto exclusiveResource = exclusiveGraph.ImportBuffer(
		exclusiveBuffer,
		exclusiveDesc.initialState,
		exclusiveDesc,
		"ExclusiveBuffer"
	);
	exclusiveGraph.AddPass(
		"GraphicsWrite",
		RHI::CommandQueueType::Graphics,
		[&](RHI::RenderGraphPassBuilder& builder){ builder.Write(exclusiveResource); },
		{}
	);
	exclusiveGraph.AddPass(
		"ComputeRead",
		RHI::CommandQueueType::Compute,
		[&](RHI::RenderGraphPassBuilder& builder){ builder.Read(exclusiveResource); },
		{}
	);
	assert(!exclusiveGraph.Compile());
	assert(!exclusiveGraph.Error().empty());

	RHI::BufferDesc concurrentDesc;
	concurrentDesc.byteSize = 64;
	concurrentDesc.queueSharingMode = RHI::ResourceQueueSharingMode::Concurrent;
	const RHI::BufferHandle concurrentBuffer = device.CreateBuffer(concurrentDesc, {});
	assert(concurrentBuffer);

	RHI::RenderGraph concurrentGraph;
	const auto concurrentResource = concurrentGraph.ImportBuffer(
		concurrentBuffer,
		concurrentDesc.initialState,
		concurrentDesc,
		"ConcurrentBuffer"
	);
	concurrentGraph.AddPass(
		"GraphicsWrite",
		RHI::CommandQueueType::Graphics,
		[&](RHI::RenderGraphPassBuilder& builder){ builder.Write(concurrentResource); },
		{}
	);
	concurrentGraph.AddPass(
		"ComputeRead",
		RHI::CommandQueueType::Compute,
		[&](RHI::RenderGraphPassBuilder& builder){ builder.Read(concurrentResource); },
		{}
	);
	assert(concurrentGraph.Compile());
	assert(concurrentGraph.RequiresQueueSynchronization());
	assert(concurrentGraph.Execute(device));
	assert(concurrentGraph.ReleaseExecutionState(device));
}

void TestTransientLifetimeAnalysis(){
	RHI::RenderGraph graph;
	const auto transientA = graph.AddResource("TransientA");
	const auto transientB = graph.AddResource("TransientB");
	const auto unused = graph.AddResource("Unused");

	const uint32_t writeA = graph.AddPass(
		"WriteA",
		[&](RHI::RenderGraphPassBuilder& builder){ builder.Write(transientA); },
		{}
	);
	const uint32_t readA = graph.AddPass(
		"ReadA",
		[&](RHI::RenderGraphPassBuilder& builder){ builder.Read(transientA); },
		{}
	);
	const uint32_t writeB = graph.AddPass(
		"WriteB",
		[&](RHI::RenderGraphPassBuilder& builder){ builder.Write(transientB); },
		{}
	);
	const uint32_t readB = graph.AddPass(
		"ReadB",
		[&](RHI::RenderGraphPassBuilder& builder){ builder.Read(transientB); },
		{}
	);

	assert(graph.Compile());
	const auto& lifetimeA = graph.Lifetime(transientA);
	const auto& lifetimeB = graph.Lifetime(transientB);
	assert(lifetimeA.firstPassIndex == writeA);
	assert(lifetimeA.lastPassIndex == readA);
	assert(lifetimeA.firstExecutionIndex == 0);
	assert(lifetimeA.lastExecutionIndex == 1);
	assert(lifetimeA.passUseCount == 2);
	assert(lifetimeB.firstPassIndex == writeB);
	assert(lifetimeB.lastPassIndex == readB);
	assert(lifetimeB.firstExecutionIndex == 2);
	assert(lifetimeB.lastExecutionIndex == 3);
	assert(lifetimeB.passUseCount == 2);
	assert(!lifetimeA.Overlaps(lifetimeB));
	assert(graph.CanAlias(transientA, transientB));
	assert(graph.IsTransient(transientA));
	assert(!graph.IsImported(transientA));
	assert(!graph.Lifetime(unused).IsUsed());
	assert(!graph.CanAlias(transientA, unused));
}


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

} // namespace

int main(){
	TestAutomaticTransitions();
	TestSubresourceTransitions();
	TestInvalidStateDeclarations();
	TestInvalidSubresourceDeclarations();
	TestQueueSynchronization();
	TestQueueSharingValidation();
	TestTransientLifetimeAnalysis();
	TestPassCulling();
	return 0;
}
