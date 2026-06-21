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

	auto commandList = device.CreateCommandList({
		RHI::CommandQueueType::Graphics,
		false
	});
	assert(commandList);
	assert(graph.Execute(*commandList));
	assert((execution == std::vector<int>{1, 2, 3, 4}));
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

} // namespace

int main(){
	TestAutomaticTransitions();
	TestInvalidStateDeclarations();
	return 0;
}
