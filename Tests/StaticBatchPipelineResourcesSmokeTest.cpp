#include <array>
#include <cassert>
#include <cstddef>

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

	StaticBatchPipelineResources resources;
	assert(!resources.IsAllocated());
	assert(!resources.IsReady());
	assert(!resources.Create(*device, {}, {}));

	constexpr std::array<std::byte, 4> vertexByteCode{};
	constexpr std::array<std::byte, 4> pixelByteCode{};
	assert(resources.Create(*device, vertexByteCode, pixelByteCode));
	assert(resources.IsAllocated());
	assert(resources.IsReady());

	const RHI::ShaderHandle vertexShader = resources.VertexShader();
	const RHI::ShaderHandle pixelShader = resources.PixelShader();
	const RHI::PipelineStateHandle pipeline = resources.PipelineState();
	assert(vertexShader);
	assert(pixelShader);
	assert(pipeline);

	const RHI::ShaderDesc* vertexDesc = device->GetShaderDesc(vertexShader);
	const RHI::ShaderDesc* pixelDesc = device->GetShaderDesc(pixelShader);
	assert(vertexDesc);
	assert(pixelDesc);
	assert(vertexDesc->stage == RHI::ShaderStage::Vertex);
	assert(pixelDesc->stage == RHI::ShaderStage::Pixel);
	assert(vertexDesc->entryPoint == "main");
	assert(pixelDesc->entryPoint == "main");

	const RHI::PipelineStateDesc* pipelineDesc = device->GetPipelineStateDesc(pipeline);
	assert(pipelineDesc);
	assert(pipelineDesc->vertexShader == vertexShader);
	assert(pipelineDesc->pixelShader == pixelShader);
	assert(pipelineDesc->inputLayout.size() == 10);
	assert(pipelineDesc->topology == RHI::PrimitiveTopology::TriangleList);
	assert(pipelineDesc->rasterizer.cullMode == RHI::CullMode::Back);
	assert(pipelineDesc->depthStencil.depthEnable);
	assert(pipelineDesc->depthStencil.depthWriteEnable);
	assert(pipelineDesc->depthStencil.depthFunction == RHI::ComparisonFunc::LessEqual);

	const RHI::RenderTargetLayoutDesc& renderTargets = pipelineDesc->renderTargets;
	assert(renderTargets.colorAttachmentCount == StaticBatchPipelineContract::GBufferColorAttachmentCount);
	for(std::size_t index = 0; index < 5; ++index){
		assert(renderTargets.colorFormats[index] == RHI::Format::RGBA16_Float);
	}
	assert(renderTargets.colorFormats[5] == RHI::Format::RGBA32_UInt);
	assert(renderTargets.depthStencilFormat == RHI::Format::D32_Float);
	assert(renderTargets.sampleCount == 1);

	RHI::CommandListCreateDesc commandDesc;
	commandDesc.queueType = RHI::CommandQueueType::Graphics;
	auto commandList = device->CreateCommandList(commandDesc);
	assert(commandList);
	commandList->Begin();
	assert(resources.Bind(*commandList));
	commandList->End();

	assert(!resources.Create(*device, vertexByteCode, pixelByteCode));
	assert(resources.VertexShader() == vertexShader);
	assert(resources.PixelShader() == pixelShader);
	assert(resources.PipelineState() == pipeline);

	assert(resources.Release(*device));
	assert(!resources.IsAllocated());
	assert(!resources.IsReady());
	assert(device->GetPipelineStateDesc(pipeline) == nullptr);
	assert(device->GetShaderDesc(pixelShader) == nullptr);
	assert(device->GetShaderDesc(vertexShader) == nullptr);
	assert(resources.Release(*device));
	return 0;
}
