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
	assert(gpuBuffer.Telemetry().growthDeniedCount == 0);

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

	std::vector<StaticBatchInstanceData> constrainedInstances(2);
	constrainedInstances[0].entityIndex = 21;
	constrainedInstances[1].entityIndex = 22;

	StaticBatchGpuInstanceBuffer constrainedBuffer;
	assert(constrainedBuffer.Reserve(*device, 2));
	const std::size_t configuredCapacity = constrainedBuffer.Capacity();
	assert(configuredCapacity >= 2);
	assert(constrainedBuffer.Upload(
		*device,
		*commandList,
		constrainedInstances,
		200,
		false
	));
	assert(constrainedBuffer.Telemetry().reallocationCount == 1);
	assert(constrainedBuffer.Telemetry().growthDeniedCount == 0);

	constrainedInstances.push_back({});
	constrainedInstances.back().entityIndex = 23;
	assert(!constrainedBuffer.Upload(
		*device,
		*commandList,
		constrainedInstances,
		201,
		false
	));
	assert(!constrainedBuffer.IsValid());
	assert(!constrainedBuffer.CanSubmit());
	assert(constrainedBuffer.Capacity() == configuredCapacity);
	assert(constrainedBuffer.Telemetry().failedUploadCount == 1);
	assert(constrainedBuffer.Telemetry().growthDeniedCount == 1);
	assert(constrainedBuffer.Telemetry().reallocationCount == 1);

	assert(constrainedBuffer.Upload(
		*device,
		*commandList,
		constrainedInstances,
		202,
		true
	));
	assert(constrainedBuffer.IsValid());
	assert(constrainedBuffer.InstanceCount() == 3);
	assert(constrainedBuffer.Capacity() >= 3);
	assert(constrainedBuffer.Telemetry().uploadCount == 2);
	assert(constrainedBuffer.Telemetry().reallocationCount == 2);
	assert(constrainedBuffer.Telemetry().growthDeniedCount == 1);

	commandList->End();
	assert(gpuBuffer.Release(*device));
	assert(!static_cast<bool>(gpuBuffer.Buffer()));
	assert(!gpuBuffer.IsValid());
	assert(!gpuBuffer.CanSubmit());
	assert(gpuBuffer.Capacity() == 0);

	assert(constrainedBuffer.Release(*device));
	assert(!static_cast<bool>(constrainedBuffer.Buffer()));
	assert(!constrainedBuffer.IsValid());
	assert(!constrainedBuffer.CanSubmit());
	assert(constrainedBuffer.Capacity() == 0);
	return 0;
}
