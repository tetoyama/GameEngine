#pragma once
namespace RHI {
inline bool D3D11RHIDevice::ImportSwapChainImages(){
	m_swapChain.m_images.clear();
	if(!m_swapChain.m_swapChain) return true;
	if(!m_device) return false;
	DXGI_SWAP_CHAIN_DESC swapDesc{};
	if(FAILED(m_swapChain.m_swapChain->GetDesc(&swapDesc)) || swapDesc.BufferCount == 0) return false;
	const bool flipModel = swapDesc.SwapEffect == DXGI_SWAP_EFFECT_FLIP_DISCARD ||
		swapDesc.SwapEffect == DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	const uint32_t imageCount = flipModel ? swapDesc.BufferCount : 1u;
	std::vector<TextureHandle> imported;
	imported.reserve(imageCount);
	for(uint32_t i = 0; i < imageCount; ++i){
		Microsoft::WRL::ComPtr<ID3D11Texture2D> image;
		if(FAILED(m_swapChain.m_swapChain->GetBuffer(i, IID_PPV_ARGS(image.GetAddressOf())))){
			for(auto h : imported) m_textures.Destroy(h);
			return false;
		}
		D3D11_TEXTURE2D_DESC nativeDesc{};
		image->GetDesc(&nativeDesc);
		D3D11TextureResource resource;
		resource.desc.width = nativeDesc.Width;
		resource.desc.height = nativeDesc.Height;
		resource.desc.mipLevels = 1;
		resource.desc.arraySize = 1;
		resource.desc.format = m_swapChain.m_desc.format;
		resource.desc.sampleCount = static_cast<uint8_t>(nativeDesc.SampleDesc.Count);
		resource.desc.bindFlags = TextureBindFlags::RenderTarget;
		resource.desc.initialState = ResourceState::Present;
		resource.desc.debugName = "D3D11 SwapChain Image " + std::to_string(i);
		resource.subresourceStates.assign(1, ResourceState::Present);
		resource.swapChainOwned = true;
		resource.object = std::move(image);
		const TextureHandle handle = m_textures.Create(std::move(resource));
		if(!handle){ for(auto h : imported) m_textures.Destroy(h); return false; }
		imported.push_back(handle);
	}
	m_swapChain.m_images = std::move(imported);
	return true;
}
inline bool D3D11RHIDevice::ReleaseSwapChainImages(){
	for(auto h : m_swapChain.m_images){ const auto* r = Find(h); if(!r || r->viewCount != 0) return false; }
	if(m_context) m_context->OMSetRenderTargets(0, nullptr, nullptr);
	for(auto h : m_swapChain.m_images) if(!m_textures.Destroy(h)) return false;
	m_swapChain.m_images.clear();
	return true;
}
inline bool D3D11RHICommandQueue::Present(IRHISwapChain& swapChain, bool verticalSync){
	if(m_type != swapChain.GetPresentQueueType()) return false;
	auto* native = dynamic_cast<D3D11RHISwapChain*>(&swapChain);
	return native && native->Present(verticalSync);
}
} // namespace RHI
