#pragma once

namespace RHI {

inline D3D11RHICommandList::D3D11RHICommandList(D3D11RHIDevice* owner)
	: m_owner(owner){
}

inline void D3D11RHICommandList::Attach(D3D11RHIDevice* owner){
	m_owner = owner;
	m_recording = false;
	m_renderPassActive = false;
}

inline D3D11RHIDevice::D3D11RHIDevice(
	ID3D11Device* device,
	ID3D11DeviceContext* context,
	IDXGISwapChain* swapChain,
	const SwapChainDesc& swapChainDesc
)
	: m_device(device)
	, m_context(context)
	, m_swapChain(swapChain, swapChainDesc)
	, m_immediateCommandList(this){
}

inline D3D11RHIDevice::~D3D11RHIDevice(){
	m_pipelines.Clear();
	m_shaders.Clear();
	m_textures.Clear();
	m_buffers.Clear();
}

inline bool D3D11RHIDevice::DestroyBuffer(BufferHandle handle){
	return m_buffers.Destroy(handle);
}

inline bool D3D11RHIDevice::DestroyTexture(TextureHandle handle){
	return m_textures.Destroy(handle);
}

inline bool D3D11RHIDevice::DestroyShader(ShaderHandle handle){
	return m_shaders.Destroy(handle);
}

inline bool D3D11RHIDevice::DestroyPipelineState(PipelineStateHandle handle){
	return m_pipelines.Destroy(handle);
}

inline const BufferDesc* D3D11RHIDevice::GetBufferDesc(BufferHandle handle) const {
	const auto* resource = Find(handle);
	return resource ? &resource->desc : nullptr;
}

inline const TextureDesc* D3D11RHIDevice::GetTextureDesc(TextureHandle handle) const {
	const auto* resource = Find(handle);
	return resource ? &resource->desc : nullptr;
}

inline const ShaderDesc* D3D11RHIDevice::GetShaderDesc(ShaderHandle handle) const {
	const auto* resource = Find(handle);
	return resource ? &resource->desc : nullptr;
}

inline const PipelineStateDesc* D3D11RHIDevice::GetPipelineStateDesc(
	PipelineStateHandle handle
) const {
	const auto* resource = Find(handle);
	return resource ? &resource->desc : nullptr;
}

inline void D3D11RHIDevice::WaitIdle(){
	if(m_context) m_context->Flush();
}

} // namespace RHI
