#include <cassert>

#include "Service/Graphics/RHI/Null/NullRHIBackend.h"
#include "Service/Graphics/RHI/RHIService.h"

int main(){
	RHI::RenderHardwareInterfaceService service;
	assert(RHI::RegisterNullRHIBackend(service.GetRegistry()));
	assert(service.SelectBackend(RHI::BackendType::Null));
	assert(service.GetBackend());

	RHI::DeviceCreateDesc desc;
	desc.swapChain.width = 640;
	desc.swapChain.height = 360;
	auto device = service.GetBackend()->CreateDevice(desc);
	assert(device);
	assert(service.AdoptDevice(std::move(device)));
	assert(service.GetDevice());
	assert(service.GetDevice()->GetQueue(RHI::CommandQueueType::Graphics));

	RHI::CommandListCreateDesc commandDesc;
	commandDesc.queueType = RHI::CommandQueueType::Graphics;
	auto commandList = service.GetDevice()->CreateCommandList(commandDesc);
	assert(commandList);
	commandList->Begin();
	assert(!commandList->DrawIndexedInstanced(36, 4));
	commandList->End();

	auto released = service.ReleaseDevice();
	assert(released);
	assert(!service.GetDevice());
	assert(service.AdoptDevice(std::move(released)));
	service.Shutdown();
	assert(!service.GetDevice());
	assert(!service.GetBackend());
	return 0;
}
