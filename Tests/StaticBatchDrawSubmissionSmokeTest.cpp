#include <array>
#include <cassert>
#include <cstddef>
#include <span>

#include "Engine/Scene/System/Render/StaticBatch/StaticBatchDrawSubmission.h"
#include "Engine/Scene/System/Render/StaticBatch/StaticBatchPipelineResources.h"
#include "Service/Graphics/RHI/Null/NullRHIBackend.h"

int main(){
	RHI::BackendRegistry registry;
	assert(RHI::RegisterNullRHIBackend(registry));
	auto backend = registry.Create(RHI::BackendType::Null);
	assert(backend);

	RHI::DeviceCreateDesc deviceDesc;
	deviceDesc.swapChain.width = 64;
	deviceDesc.swapChain.height = 64;
	auto device = backend->CreateDevice(deviceDesc);
	assert(device);

	constexpr std::array<std::byte, 4> shaderBytes{};
	StaticBatchPipelineResources pipeline;
	assert(pipeline.Create(*device, shaderBytes, shaderBytes));

	constexpr std::array<std::byte, 60> vertexBytes{};
	constexpr std::array<std::byte, 12> indexBytes{};
	constexpr std::array<std::byte, 80> instanceBytes{};

	RHI::BufferDesc vertexDesc;
	vertexDesc.byteSize = static_cast<std::uint32_t>(vertexBytes.size());
	vertexDesc.stride = 60;
	vertexDesc.usage = RHI::ResourceUsage::Immutable;
	vertexDesc.bindFlags = RHI::BufferBindFlags::Vertex;
	vertexDesc.initialState = RHI::ResourceState::VertexBuffer;
	const RHI::BufferHandle vertex = device->CreateBuffer(vertexDesc, vertexBytes);
	assert(vertex);

	RHI::BufferDesc indexDesc;
	indexDesc.byteSize = static_cast<std::uint32_t>(indexBytes.size());
	indexDesc.stride = 4;
	indexDesc.usage = RHI::ResourceUsage::Immutable;
	indexDesc.bindFlags = RHI::BufferBindFlags::Index;
	indexDesc.initialState = RHI::ResourceState::IndexBuffer;
	const RHI::BufferHandle index = device->CreateBuffer(indexDesc, indexBytes);
	assert(index);

	RHI::BufferDesc instanceDesc;
	instanceDesc.byteSize = static_cast<std::uint32_t>(instanceBytes.size());
	instanceDesc.stride = StaticBatchGpuInstanceBuffer::InstanceStride;
	instanceDesc.usage = RHI::ResourceUsage::Dynamic;
	instanceDesc.bindFlags = RHI::BufferBindFlags::Vertex;
	instanceDesc.cpuAccess = RHI::CpuAccessFlags::Write;
	instanceDesc.initialState = RHI::ResourceState::VertexBuffer;
	const RHI::BufferHandle instance = device->CreateBuffer(instanceDesc);
	assert(instance);

	StaticBatchDrawSubmissionDesc desc;
	assert(!desc.IsValid());
	desc.pipeline = pipeline.PipelineState();
	desc.vertexBuffer = vertex;
	desc.indexBuffer = index;
	desc.instanceBuffer = instance;
	desc.vertexStride = 60;
	desc.indexCount = 3;
	desc.instanceCount = 1;
	desc.firstInstance = 4;
	assert(desc.IsValid());

	auto commandList = device->CreateCommandList({
		RHI::CommandQueueType::Graphics,
		false
	});
	assert(commandList);
	commandList->Begin();
	assert(SubmitStaticBatchDraw(*commandList, desc));
	commandList->End();

	assert(device->DestroyBuffer(instance));
	assert(device->DestroyBuffer(index));
	assert(device->DestroyBuffer(vertex));
	assert(pipeline.Release(*device));
	return 0;
}
