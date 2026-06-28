#pragma once

#include <memory>
#include <dxgi1_5.h>

#include "Service/Graphics/graphicsContext.h"
#include "D3D11RHIConversions.h"
#include "D3D11RHIDevice.h"

namespace RHI {

inline IRHIDevice* EnsureGraphicsContextRHIDevice(GraphicsContext& graphics){
	RenderHardwareInterfaceService* service = graphics.GetRHIService();
	if(!service) return nullptr;
	if(IRHIDevice* existing = service->GetDevice()){
		return existing->GetBackendType() == BackendType::Direct3D11
			? existing
			: nullptr;
	}
	if(service->GetSelectedBackend() != BackendType::Direct3D11){
		return nullptr;
	}

	ID3D11Device* nativeDevice = graphics.GetDevice();
	ID3D11DeviceContext* nativeContext = graphics.GetDeviceContext();
	IDXGISwapChain* nativeSwapChain = graphics.GetSwapChain();
	if(!nativeDevice || !nativeContext || !nativeSwapChain){
		return nullptr;
	}

	DXGI_SWAP_CHAIN_DESC nativeDesc{};
	if(FAILED(nativeSwapChain->GetDesc(&nativeDesc))){
		return nullptr;
	}

	SwapChainDesc swapChainDesc;
	swapChainDesc.width = nativeDesc.BufferDesc.Width;
	swapChainDesc.height = nativeDesc.BufferDesc.Height;
	swapChainDesc.bufferCount = nativeDesc.BufferCount;
	swapChainDesc.format = D3D11Detail::FromDXGIFormat(nativeDesc.BufferDesc.Format);
	swapChainDesc.allowTearing =
		(nativeDesc.Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) != 0;
	swapChainDesc.debugName = "GraphicsContext SwapChain";
	if(swapChainDesc.width == 0 || swapChainDesc.height == 0 ||
		swapChainDesc.bufferCount == 0 || swapChainDesc.format == Format::Unknown){
		return nullptr;
	}

	auto device = std::make_unique<D3D11RHIDevice>(
		nativeDevice,
		nativeContext,
		nativeSwapChain,
		swapChainDesc
	);
	if(!service->AdoptDevice(std::move(device))){
		return nullptr;
	}
	return service->GetDevice();
}

} // namespace RHI
