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

	RHI::CommandListCreateDesc commandDesc;
	commandDesc.queueType = RHI::CommandQueueType::Graphics;
	auto commandList = device->CreateCommandList(commandDesc);
	assert(commandList);
	commandList->Begin();
	assert(!commandList->DrawIndexedInstanced(36, 4));
	commandList->End();
	return 0;
}
