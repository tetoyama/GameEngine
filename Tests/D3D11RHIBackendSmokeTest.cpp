#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include <span>
#include <type_traits>

#include <d3d11.h>
#include <wrl/client.h>

#include "Service/Graphics/RHI/D3D11/D3D11RHIBackend.h"

namespace {

void TestBackendRegistration(){
	static_assert(std::is_base_of_v<RHI::IRHIBackend, RHI::D3D11RHIBackend>);
	static_assert(std::is_base_of_v<RHI::IRHIDevice, RHI::D3D11RHIDevice>);
	static_assert(std::is_base_of_v<RHI::IRHICommandQueue, RHI::D3D11RHICommandQueue>);
	static_assert(std::is_base_of_v<RHI::IRHICommandList, RHI::D3D11RHICommandList>);
	static_assert(std::is_base_of_v<RHI::IRHIFence, RHI::D3D11RHIFence>);

	const bool registered = RHI::RegisterD3D11RHIBackend();
	assert(registered || RHI::BackendRegistry::Instance().IsRegistered(
		RHI::BackendType::Direct3D11
	));

	std::unique_ptr<RHI::IRHIBackend> backend =
		RHI::BackendRegistry::Instance().Create(
			RHI::BackendType::Direct3D11
		);
	assert(backend);
	assert(backend->GetType() == RHI::BackendType::Direct3D11);
	assert(backend->GetName() == "Direct3D 11");
	assert(backend->IsSupported());
	assert(!backend->EnumerateAdapters().empty());
}

void TestLogicalQueueFallbackAndGpuFence(){
	Microsoft::WRL::ComPtr<ID3D11Device> nativeDevice;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> nativeContext;
	D3D_FEATURE_LEVEL featureLevel{};

	const HRESULT createResult = D3D11CreateDevice(
		nullptr,
		D3D_DRIVER_TYPE_WARP,
		nullptr,
		0,
		nullptr,
		0,
		D3D11_SDK_VERSION,
		nativeDevice.GetAddressOf(),
		&featureLevel,
		nativeContext.GetAddressOf()
	);
	assert(SUCCEEDED(createResult));
	assert(nativeDevice);
	assert(nativeContext);

	RHI::SwapChainDesc swapChainDesc;
	RHI::D3D11RHIDevice device(
		nativeDevice.Get(),
		nativeContext.Get(),
		nullptr,
		swapChainDesc
	);

	const RHI::DeviceCapabilities& capabilities = device.GetCapabilities();
	assert(capabilities.backend == RHI::BackendType::Direct3D11);
	assert(capabilities.supportsCompute);
	assert(!capabilities.supportsAsyncCompute);
	assert(!capabilities.supportsMultipleCommandQueues);
	assert(!capabilities.supportsTimelineSynchronization);

	RHI::IRHICommandQueue* graphicsQueue =
		device.GetQueue(RHI::CommandQueueType::Graphics);
	RHI::IRHICommandQueue* computeQueue =
		device.GetQueue(RHI::CommandQueueType::Compute);
	RHI::IRHICommandQueue* copyQueue =
		device.GetQueue(RHI::CommandQueueType::Copy);
	assert(graphicsQueue);
	assert(computeQueue);
	assert(copyQueue);
	assert(graphicsQueue->GetType() == RHI::CommandQueueType::Graphics);
	assert(computeQueue->GetType() == RHI::CommandQueueType::Compute);
	assert(copyQueue->GetType() == RHI::CommandQueueType::Copy);

	assert(!device.CreateCommandList({
		RHI::CommandQueueType::Graphics,
		true
	}));

	std::unique_ptr<RHI::IRHICommandList> graphicsList =
		device.CreateCommandList({RHI::CommandQueueType::Graphics, false});
	std::unique_ptr<RHI::IRHICommandList> computeList =
		device.CreateCommandList({RHI::CommandQueueType::Compute, false});
	std::unique_ptr<RHI::IRHICommandList> copyList =
		device.CreateCommandList({RHI::CommandQueueType::Copy, false});
	assert(graphicsList);
	assert(computeList);
	assert(copyList);

	graphicsList->Begin();
	graphicsList->End();

	computeList->Begin();
	RHI::RenderPassDesc renderPass;
	assert(!computeList->BeginRenderPass(renderPass));
	computeList->Dispatch(1, 1, 1);
	computeList->End();

	copyList->Begin();
	assert(!copyList->BeginRenderPass(renderPass));
	copyList->End();

	std::unique_ptr<RHI::IRHIFence> fence = device.CreateFence(0);
	assert(fence);
	assert(fence->GetCompletedValue() == 0);

	std::array<RHI::IRHICommandList*, 1> lists{computeList.get()};
	RHI::QueueSubmitDesc submit;
	submit.commandLists = std::span<RHI::IRHICommandList* const>(lists);
	submit.signalFence = fence->GetHandle();
	submit.signalValue = 1;
	assert(computeQueue->Submit(submit));
	assert(fence->Wait(1, 5'000'000'000ull));
	assert(fence->GetCompletedValue() >= 1);

	// 論理Queueが異なるCommandListは提出できない。
	assert(!graphicsQueue->Submit(submit));

	lists[0] = copyList.get();
	submit.commandLists = std::span<RHI::IRHICommandList* const>(lists);
	submit.waitFence = fence->GetHandle();
	submit.waitValue = 1;
	submit.signalFence = fence->GetHandle();
	submit.signalValue = 2;
	assert(copyQueue->Submit(submit));
	assert(fence->Wait(2, 5'000'000'000ull));

	lists[0] = graphicsList.get();
	submit.commandLists = std::span<RHI::IRHICommandList* const>(lists);
	submit.waitValue = 2;
	submit.signalValue = 3;
	assert(graphicsQueue->Submit(submit));
	assert(fence->Wait(3, 5'000'000'000ull));

	device.WaitIdle();
}

} // namespace

int main(){
	TestBackendRegistration();
	TestLogicalQueueFallbackAndGpuFence();
	return 0;
}
