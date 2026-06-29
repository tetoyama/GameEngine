#include <array>
#include <cassert>
#include <cstddef>

#include "Engine/Scene/System/Render/StaticBatch/StaticBatchPipelineResources.h"
#include "Engine/Scene/System/Render/StaticBatch/StaticBatchShadowPipelineResources.h"
#include "Engine/Scene/System/Render/StaticBatch/StaticBatchShadowSubmission.h"
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

	constexpr std::array<std::byte, 4> vertexByteCode{
		std::byte{0x01},
		std::byte{0x02},
		std::byte{0x03},
		std::byte{0x04}
	};
	constexpr std::array<std::byte, 4> gBufferPixelByteCode{
		std::byte{0x05},
		std::byte{0x06},
		std::byte{0x07},
		std::byte{0x08}
	};
	constexpr std::array<std::byte, 4> shadowPixelByteCode{
		std::byte{0x09},
		std::byte{0x0a},
		std::byte{0x0b},
		std::byte{0x0c}
	};

	StaticBatchPipelineResources sharedResources;
	assert(sharedResources.Create(
		*device,
		vertexByteCode,
		gBufferPixelByteCode
	));

	StaticBatchShadowPipelineResources shadowResources;
	assert(shadowResources.Create(
		*device,
		sharedResources.VertexShader(),
		shadowPixelByteCode
	));
	assert(shadowResources.IsReady());

	const RHI::PipelineStateDesc* pipelineDesc =
		device->GetPipelineStateDesc(shadowResources.PipelineState());
	assert(pipelineDesc);
	assert(pipelineDesc->vertexShader == sharedResources.VertexShader());
	assert(pipelineDesc->pixelShader == shadowResources.PixelShader());
	assert(pipelineDesc->inputLayout.size() == 10);
	assert(pipelineDesc->topology == RHI::PrimitiveTopology::TriangleList);
	assert(pipelineDesc->rasterizer.cullMode == RHI::CullMode::Back);
	assert(!pipelineDesc->rasterizer.depthClipEnable);
	assert(pipelineDesc->depthStencil.depthEnable);
	assert(pipelineDesc->depthStencil.depthWriteEnable);
	assert(
		pipelineDesc->depthStencil.depthFunction ==
		RHI::ComparisonFunc::LessEqual
	);
	assert(pipelineDesc->renderTargets.colorAttachmentCount == 0);
	assert(
		pipelineDesc->renderTargets.depthStencilFormat ==
		RHI::Format::D32_Float
	);
	assert(pipelineDesc->renderTargets.sampleCount == 1);

	StaticBatchShadowSubmissionTelemetry telemetry;
	telemetry.submittedGroupCount = 2;
	telemetry.submittedInstanceCount = 7;
	assert(telemetry.EstimatedDrawCallReduction() == 5);
	telemetry.submittedGroupCount = 7;
	telemetry.submittedInstanceCount = 2;
	assert(telemetry.EstimatedDrawCallReduction() == 0);

	RHI::CommandListCreateDesc commandDesc;
	commandDesc.queueType = RHI::CommandQueueType::Graphics;
	auto commandList = device->CreateCommandList(commandDesc);
	assert(commandList);
	commandList->Begin();
	assert(commandList->SetPipelineState(shadowResources.PipelineState()));
	commandList->End();

	assert(shadowResources.Release(*device));
	assert(!shadowResources.IsAllocated());
	assert(sharedResources.IsReady());
	assert(sharedResources.Release(*device));
	return 0;
}
