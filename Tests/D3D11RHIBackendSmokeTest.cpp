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
	assert(registered || RHI::BackendRegistry::Instance().IsRegistered(RHI::BackendType::Direct3D11));
	auto backend = RHI::BackendRegistry::Instance().Create(RHI::BackendType::Direct3D11);
	assert(backend && backend->GetType() == RHI::BackendType::Direct3D11);
	assert(backend->GetName() == "Direct3D 11");
	assert(backend->IsSupported());
	assert(!backend->EnumerateAdapters().empty());
}

void TestLogicalQueueFallbackAndGpuFence(){
	Microsoft::WRL::ComPtr<ID3D11Device> nativeDevice;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> nativeContext;
	D3D_FEATURE_LEVEL featureLevel{};
	const HRESULT result = D3D11CreateDevice(
		nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
		D3D11_SDK_VERSION, nativeDevice.GetAddressOf(),
		&featureLevel, nativeContext.GetAddressOf()
	);
	assert(SUCCEEDED(result) && nativeDevice && nativeContext);

	RHI::D3D11RHIDevice device(
		nativeDevice.Get(), nativeContext.Get(), nullptr, RHI::SwapChainDesc{}
	);
	const auto& capabilities = device.GetCapabilities();
	assert(capabilities.backend == RHI::BackendType::Direct3D11);
	assert(capabilities.supportsCompute);
	assert(!capabilities.supportsAsyncCompute);
	assert(!capabilities.supportsMultipleCommandQueues);
	assert(!capabilities.supportsTimelineSynchronization);

	auto* graphicsQueue = device.GetQueue(RHI::CommandQueueType::Graphics);
	auto* computeQueue = device.GetQueue(RHI::CommandQueueType::Compute);
	auto* copyQueue = device.GetQueue(RHI::CommandQueueType::Copy);
	assert(graphicsQueue && computeQueue && copyQueue);
	assert(graphicsQueue->GetType() == RHI::CommandQueueType::Graphics);
	assert(computeQueue->GetType() == RHI::CommandQueueType::Compute);
	assert(copyQueue->GetType() == RHI::CommandQueueType::Copy);
	assert(!device.CreateCommandList({RHI::CommandQueueType::Graphics, true}));

	RHI::BufferDesc bufferDesc;
	bufferDesc.byteSize = 64;
	bufferDesc.stride = 16;
	bufferDesc.structured = true;
	bufferDesc.bindFlags = RHI::BufferBindFlags::ShaderResource |
		RHI::BufferBindFlags::UnorderedAccess;
	bufferDesc.initialState = RHI::ResourceState::Common;
	const RHI::BufferHandle buffer = device.CreateBuffer(bufferDesc, {});
	assert(buffer);

	RHI::BufferViewDesc bufferSrvDesc;
	bufferSrvDesc.buffer = buffer;
	bufferSrvDesc.type = RHI::BufferViewType::ShaderResource;
	const RHI::BufferViewHandle bufferSrv = device.CreateBufferView(bufferSrvDesc);
	RHI::BufferViewDesc bufferUavDesc = bufferSrvDesc;
	bufferUavDesc.type = RHI::BufferViewType::UnorderedAccess;
	const RHI::BufferViewHandle bufferUav = device.CreateBufferView(bufferUavDesc);
	assert(bufferSrv && bufferUav);
	assert(device.GetBufferViewDesc(bufferSrv)->elementCount == 4);
	assert(!device.DestroyBuffer(buffer));

	RHI::TextureDesc textureDesc;
	textureDesc.width = 8;
	textureDesc.height = 8;
	textureDesc.bindFlags = RHI::TextureBindFlags::ShaderResource |
		RHI::TextureBindFlags::RenderTarget;
	const RHI::TextureHandle texture = device.CreateTexture(textureDesc, {}, 0);
	assert(texture);
	RHI::TextureViewDesc textureSrvDesc;
	textureSrvDesc.texture = texture;
	textureSrvDesc.type = RHI::TextureViewType::ShaderResource;
	const RHI::TextureViewHandle textureSrv = device.CreateTextureView(textureSrvDesc);
	RHI::TextureViewDesc renderViewDesc;
	renderViewDesc.texture = texture;
	renderViewDesc.type = RHI::TextureViewType::RenderTarget;
	const RHI::TextureViewHandle renderView = device.CreateTextureView(renderViewDesc);
	assert(textureSrv && renderView);
	assert(!device.DestroyTexture(texture));

	RHI::SamplerDesc samplerDesc;
	samplerDesc.anisotropyEnable = true;
	samplerDesc.maxAnisotropy = 8;
	const RHI::SamplerHandle sampler = device.CreateSampler(samplerDesc);
	assert(sampler && device.GetSamplerDesc(sampler));

	auto graphicsList = device.CreateCommandList({RHI::CommandQueueType::Graphics, false});
	auto computeList = device.CreateCommandList({RHI::CommandQueueType::Compute, false});
	auto copyList = device.CreateCommandList({RHI::CommandQueueType::Copy, false});
	assert(graphicsList && computeList && copyList);

	graphicsList->Begin();
	RHI::ResourceBarrierDesc barrier;
	barrier.buffer = buffer;
	barrier.before = RHI::ResourceState::Common;
	barrier.after = RHI::ResourceState::CopyDestination;
	assert(graphicsList->ResourceBarrier(std::span<const RHI::ResourceBarrierDesc>(&barrier, 1)));
	barrier.before = RHI::ResourceState::Common;
	barrier.after = RHI::ResourceState::ShaderResource;
	assert(!graphicsList->ResourceBarrier(std::span<const RHI::ResourceBarrierDesc>(&barrier, 1)));
	barrier.before = RHI::ResourceState::Undefined;
	barrier.after = RHI::ResourceState::UnorderedAccess;
	assert(graphicsList->ResourceBarrier(std::span<const RHI::ResourceBarrierDesc>(&barrier, 1)));
	barrier.type = RHI::ResourceBarrierType::UnorderedAccess;
	assert(graphicsList->ResourceBarrier(std::span<const RHI::ResourceBarrierDesc>(&barrier, 1)));
	assert(graphicsList->SetBufferView(RHI::ShaderStage::Pixel, 0, bufferSrv));
	assert(graphicsList->SetTextureView(RHI::ShaderStage::Pixel, 0, textureSrv));
	assert(graphicsList->SetSampler(RHI::ShaderStage::Pixel, 0, sampler));
	RHI::RenderPassDesc renderPass;
	renderPass.colorAttachments.push_back({renderView});
	assert(graphicsList->BeginRenderPass(renderPass));
	graphicsList->EndRenderPass();
	graphicsList->End();

	computeList->Begin();
	assert(!computeList->BeginRenderPass(renderPass));
	assert(computeList->SetBufferView(RHI::ShaderStage::Compute, 0, bufferUav));
	assert(computeList->SetSampler(RHI::ShaderStage::Compute, 0, sampler));
	computeList->Dispatch(1, 1, 1);
	computeList->End();

	copyList->Begin();
	assert(!copyList->BeginRenderPass(renderPass));
	assert(!copyList->SetTextureView(RHI::ShaderStage::Compute, 0, textureSrv));
	assert(!copyList->SetSampler(RHI::ShaderStage::Compute, 0, sampler));
	copyList->End();

	auto fence = device.CreateFence(0);
	assert(fence && fence->GetCompletedValue() == 0);
	std::array<RHI::IRHICommandList*, 1> lists{computeList.get()};
	RHI::QueueSubmitDesc submit;
	submit.commandLists = std::span<RHI::IRHICommandList* const>(lists);
	submit.signalFence = fence->GetHandle();
	submit.signalValue = 1;
	assert(computeQueue->Submit(submit));
	assert(fence->Wait(1, 5'000'000'000ull));
	assert(fence->GetCompletedValue() >= 1);
	assert(!graphicsQueue->Submit(submit));

	lists[0] = copyList.get();
	submit.commandLists = std::span<RHI::IRHICommandList* const>(lists);
	submit.waitFence = fence->GetHandle();
	submit.waitValue = 1;
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

	assert(device.DestroyBufferView(bufferSrv));
	assert(device.DestroyBufferView(bufferUav));
	assert(device.DestroyBuffer(buffer));
	assert(device.DestroyTextureView(textureSrv));
	assert(device.DestroyTextureView(renderView));
	assert(device.DestroyTexture(texture));
	assert(device.DestroySampler(sampler));
}

} // namespace

int main(){
	TestBackendRegistration();
	TestLogicalQueueFallbackAndGpuFence();
	return 0;
}
