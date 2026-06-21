#include <cassert>
#include "Service/Graphics/RHI/Null/NullRHIBackend.h"

int main(){
	RHI::BackendRegistry registry;
	assert(RHI::RegisterNullRHIBackend(registry));
	auto backend = registry.Create(RHI::BackendType::Null);
	assert(backend);

	RHI::DeviceCreateDesc desc;
	desc.swapChain.width = 640;
	desc.swapChain.height = 360;
	auto device = backend->CreateDevice(desc);
	assert(device);
	assert(device->GetQueue(RHI::CommandQueueType::Graphics));
	return 0;
}
