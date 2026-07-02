#pragma once

#include <dxgi1_4.h>

namespace RHI {

inline D3D11RHISwapChain::D3D11RHISwapChain(
	D3D11RHIDevice* owner,
	IDXGISwapChain* swapChain,
	const SwapChainDesc& desc
){
	Attach(owner, swapChain, desc);
}

inline void D3D11RHISwapChain::Attach(
	D3D11RHIDevice* owner,
	IDXGISwapChain* swapChain,
	const SwapChainDesc& desc
){
	m_owner = owner;
	m_swapChain = swapChain;
	m_desc = desc;
	m_images.clear();
}

inline uint32_t D3D11RHISwapChain::GetCurrentImageIndex() const noexcept {
	if(m_images.empty()) return 0;
	Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain3;
	if(m_swapChain && SUCCEEDED(m_swapChain.As(&swapChain3)) && swapChain3){
		const uint32_t index = swapChain3->GetCurrentBackBufferIndex();
		return index < m_images.size() ? index : 0;
	}
	return 0;
}

inline bool D3D11RHISwapChain::Resize(uint32_t width, uint32_t height){
	if(!m_owner || !m_swapChain || width == 0 || height == 0) return false;

	m_owner->WaitIdle();
	if(!m_owner->ReleaseSwapChainImages()) return false;
	if(m_owner->m_context) m_owner->m_context->OMSetRenderTargets(0, nullptr, nullptr);

	const UINT flags = m_desc.allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u;
	if(FAILED(m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, flags))){
		m_owner->ImportSwapChainImages();
		return false;
	}

	m_desc.width = width;
	m_desc.height = height;
	return m_owner->ImportSwapChainImages();
}

inline bool D3D11RHISwapChain::Present(bool verticalSync){
	if(!m_swapChain) return false;
	const UINT interval = verticalSync ? 1u : 0u;
	const UINT flags = (!verticalSync && m_desc.allowTearing)
		? DXGI_PRESENT_ALLOW_TEARING
		: 0u;
	return SUCCEEDED(m_swapChain->Present(interval, flags));
}

} // namespace RHI
