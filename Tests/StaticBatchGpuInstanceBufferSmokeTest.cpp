#include <cassert>
#include <vector>

#include "Engine/Scene/System/Render/RenderSystem/RenderPacket/StaticBatchGpuInstanceBuffer.h"
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

	RHI::CommandListCreateDesc commandDesc;
	commandDesc.queueType = RHI::CommandQueueType::Graphics;
	auto commandList = device->CreateCommandList(commandDesc);
	assert(commandList);
	commandList->Begin();

	std::vector<StaticBatchInstanceData> instances(2);
	instances[0].entityIndex = 11;
	instances[1].entityIndex = 12;

	StaticBatchGpuInstanceBuffer gpuBuffer;
	assert(gpuBuffer.Upload(*device, *commandList, instances, 100));
	assert(gpuBuffer.IsValid());
	assert(gpuBuffer.CanSubmit());
	assert(gpuBuffer.InstanceCount() == 2);
	assert(gpuBuffer.Capacity() >= 2);
	assert(gpuBuffer.UploadedSourceRevision() == 100);
	assert(gpuBuffer.Bind(*commandList, 1));

	const RHI::BufferDesc* firstDesc = device->GetBufferDesc(gpuBuffer.Buffer());
	assert(firstDesc);
	assert(firstDesc->stride == sizeof(StaticBatchInstanceData));
	assert(firstDesc->byteSize >= 2 * sizeof(StaticBatchInstanceData));
	assert(firstDesc->usage == RHI::ResourceUsage::Dynamic);
	assert(RHI::HasAnyFlag(firstDesc->bindFlags, RHI::BufferBindFlags::Vertex));
	assert(RHI::HasAnyFlag(firstDesc->cpuAccess, RHI::CpuAccessFlags::Write));

	assert(gpuBuffer.Telemetry().uploadCount == 1);
	assert(gpuBuffer.Telemetry().reallocationCount == 1);
	assert(gpuBuffer.Telemetry().failedUploadCount == 0);

	assert(gpuBuffer.Upload(*device, *commandList, instances, 100));
	assert(gpuBuffer.Telemetry().uploadCount == 1);
	assert(gpuBuffer.Telemetry().reallocationCount == 1);

	instances.push_back({});
	instances.back().entityIndex = 13;
	assert(gpuBuffer.Upload(*device, *commandList, instances, 101));
	assert(gpuBuffer.InstanceCount() == 3);
	assert(gpuBuffer.Capacity() >= 3);
	assert(gpuBuffer.UploadedSourceRevision() == 101);
	assert(gpuBuffer.Telemetry().uploadCount == 2);
	assert(gpuBuffer.Telemetry().reallocationCount == 2);

	commandList->End();
	assert(gpuBuffer.Release(*device));
	assert(!static_cast<bool>(gpuBuffer.Buffer()));
	assert(!gpuBuffer.IsValid());
	assert(!gpuBuffer.CanSubmit());
	assert(gpuBuffer.Capacity() == 0);
	return 0;
}
