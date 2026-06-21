#pragma once

namespace RHI {

inline D3D11RHISwapChain::D3D11RHISwapChain(
	IDXGISwapChain* swapChain,
	const SwapChainDesc& desc
){
	Attach(swapChain, desc);
}

inline void D3D11RHISwapChain::Attach(
	IDXGISwapChain* swapChain,
	const SwapChainDesc& desc
){
	m_swapChain = swapChain;
	m_desc = desc;
}

inline bool D3D11RHISwapChain::Resize(uint32_t width, uint32_t height){
	if(!m_swapChain || width == 0 || height == 0) return false;
	const UINT flags = m_desc.allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u;
	if(FAILED(m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, flags))){
		return false;
	}
	m_desc.width = width;
	m_desc.height = height;
	return true;
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
