#include <array>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <fstream>

#include "Engine/Scene/System/Render/StaticBatch/StaticBatchPipelineBootstrap.h"
#include "Service/Graphics/RHI/Null/NullRHIBackend.h"

namespace {

bool WriteBinaryFile(
	const char* path,
	const std::array<std::byte, 4>& bytes
){
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if(!file) return false;
	file.write(
		reinterpret_cast<const char*>(bytes.data()),
		static_cast<std::streamsize>(bytes.size())
	);
	return static_cast<bool>(file);
}

} // namespace

int main(){
	constexpr const char* vertexPath =
		"static-batch-pipeline-bootstrap-vs.cso";
	constexpr const char* pixelPath =
		"static-batch-pipeline-bootstrap-ps.cso";
	std::remove(vertexPath);
	std::remove(pixelPath);

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
	assert(
		StaticBatchPipelineBootstrap::Initialize(
			nullptr,
			resources,
			vertexPath,
			pixelPath
		) == StaticBatchPipelineBootstrapResult::InvalidDevice
	);
	assert(
		StaticBatchPipelineBootstrap::Initialize(
			device.get(),
			resources,
			vertexPath,
			pixelPath
		) == StaticBatchPipelineBootstrapResult::ShaderByteCodeLoadFailed
	);
	assert(!resources.IsAllocated());

	constexpr std::array<std::byte, 4> vertexByteCode{
		std::byte{0x01},
		std::byte{0x02},
		std::byte{0x03},
		std::byte{0x04}
	};
	constexpr std::array<std::byte, 4> pixelByteCode{
		std::byte{0x05},
		std::byte{0x06},
		std::byte{0x07},
		std::byte{0x08}
	};
	assert(WriteBinaryFile(vertexPath, vertexByteCode));
	assert(WriteBinaryFile(pixelPath, pixelByteCode));

	assert(
		StaticBatchPipelineBootstrap::Initialize(
			device.get(),
			resources,
			vertexPath,
			pixelPath
		) == StaticBatchPipelineBootstrapResult::Success
	);
	assert(resources.IsReady());

	const RHI::PipelineStateDesc* pipelineDesc =
		device->GetPipelineStateDesc(resources.PipelineState());
	assert(pipelineDesc);
	assert(pipelineDesc->inputLayout.size() == 10);
	assert(
		pipelineDesc->renderTargets.colorAttachmentCount ==
		StaticBatchPipelineContract::GBufferColorAttachmentCount
	);

	assert(
		StaticBatchPipelineBootstrap::Initialize(
			device.get(),
			resources,
			vertexPath,
			pixelPath
		) == StaticBatchPipelineBootstrapResult::AlreadyInitialized
	);
	assert(resources.Release(*device));
	assert(!resources.IsAllocated());

	std::remove(vertexPath);
	std::remove(pixelPath);
	return 0;
}
