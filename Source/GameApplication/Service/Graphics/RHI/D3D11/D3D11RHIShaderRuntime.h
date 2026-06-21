#pragma once

namespace RHI {

inline ShaderHandle D3D11RHIDevice::CreateShader(
	const ShaderDesc& desc,
	std::span<const std::byte> byteCode
){
	if(!m_device || byteCode.empty()) return {};

	D3D11ShaderResource resource;
	resource.desc = desc;
	resource.byteCode.assign(byteCode.begin(), byteCode.end());
	HRESULT result = E_FAIL;

	switch(desc.stage){
		case ShaderStage::Vertex:
			result = m_device->CreateVertexShader(
				byteCode.data(),
				byteCode.size(),
				nullptr,
				resource.vertex.GetAddressOf()
			);
			break;
		case ShaderStage::Pixel:
			result = m_device->CreatePixelShader(
				byteCode.data(),
				byteCode.size(),
				nullptr,
				resource.pixel.GetAddressOf()
			);
			break;
		case ShaderStage::Compute:
			result = m_device->CreateComputeShader(
				byteCode.data(),
				byteCode.size(),
				nullptr,
				resource.compute.GetAddressOf()
			);
			break;
	}

	if(FAILED(result)) return {};
	return m_shaders.Create(std::move(resource));
}

} // namespace RHI
