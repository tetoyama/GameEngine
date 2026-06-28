#pragma once

#include <cstdint>

#include "Service/Graphics/RHI/RHIInterfaces.h"
#include "System/Render/RenderSystem/RenderPacket/StaticBatchGpuInstanceBuffer.h"

struct StaticBatchDrawSubmissionDesc {
	RHI::PipelineStateHandle pipeline;
	RHI::BufferHandle vertexBuffer;
	RHI::BufferHandle indexBuffer;
	RHI::BufferHandle instanceBuffer;
	std::uint32_t vertexStride = 0;
	RHI::IndexFormat indexFormat = RHI::IndexFormat::UInt32;
	std::uint32_t indexCount = 0;
	std::uint32_t instanceCount = 0;
	std::uint32_t firstInstance = 0;

	bool IsValid() const noexcept {
		return pipeline && vertexBuffer && indexBuffer && instanceBuffer &&
			vertexStride > 0 && indexCount > 0 && instanceCount > 0;
	}
};

inline bool SubmitStaticBatchDraw(
	RHI::IRHICommandList& commandList,
	const StaticBatchDrawSubmissionDesc& desc
){
	if(!desc.IsValid()) return false;
	if(!commandList.SetPipelineState(desc.pipeline)) return false;
	if(!commandList.SetVertexBuffer(
		0, desc.vertexBuffer, desc.vertexStride, 0
	)) return false;
	if(!commandList.SetVertexBuffer(
		1,
		desc.instanceBuffer,
		StaticBatchGpuInstanceBuffer::InstanceStride,
		0
	)) return false;
	if(!commandList.SetIndexBuffer(
		desc.indexBuffer, desc.indexFormat, 0
	)) return false;
	return commandList.DrawIndexedInstanced(
		desc.indexCount,
		desc.instanceCount,
		0,
		0,
		desc.firstInstance
	);
}
