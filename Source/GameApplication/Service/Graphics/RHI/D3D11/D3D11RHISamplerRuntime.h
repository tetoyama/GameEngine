#pragma once

#include "D3D11RHIConversions.h"

namespace RHI::D3D11Detail {

inline D3D11_TEXTURE_ADDRESS_MODE ToSamplerAddress(SamplerAddressMode mode){
	switch(mode){
		case SamplerAddressMode::MirroredRepeat: return D3D11_TEXTURE_ADDRESS_MIRROR;
		case SamplerAddressMode::ClampToEdge: return D3D11_TEXTURE_ADDRESS_CLAMP;
		case SamplerAddressMode::ClampToBorder: return D3D11_TEXTURE_ADDRESS_BORDER;
		default: return D3D11_TEXTURE_ADDRESS_WRAP;
	}
}

inline D3D11_FILTER ToSamplerFilter(const SamplerDesc& desc){
	if(desc.anisotropyEnable){
		return desc.comparisonEnable ? D3D11_FILTER_COMPARISON_ANISOTROPIC : D3D11_FILTER_ANISOTROPIC;
	}
	const uint8_t key =
		(desc.minFilter == FilterMode::Linear ? 4u : 0u) |
		(desc.magFilter == FilterMode::Linear ? 2u : 0u) |
		(desc.mipmapMode == MipmapMode::Linear ? 1u : 0u);
	if(desc.comparisonEnable){
		switch(key){
			case 0: return D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
			case 1: return D3D11_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR;
			case 2: return D3D11_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT;
			case 3: return D3D11_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR;
			case 4: return D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT;
			case 5: return D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
			case 6: return D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
			default: return D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
		}
	}
	switch(key){
		case 0: return D3D11_FILTER_MIN_MAG_MIP_POINT;
		case 1: return D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;
		case 2: return D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
		case 3: return D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR;
		case 4: return D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
		case 5: return D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
		case 6: return D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		default: return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	}
}

} // namespace RHI::D3D11Detail

namespace RHI {

inline SamplerHandle D3D11RHIDevice::CreateSampler(const SamplerDesc& input){
	if(!m_device || input.minLod > input.maxLod) return {};
	if(input.anisotropyEnable && (input.maxAnisotropy == 0 || input.maxAnisotropy > 16)) return {};

	D3D11_SAMPLER_DESC native{};
	native.Filter = D3D11Detail::ToSamplerFilter(input);
	native.AddressU = D3D11Detail::ToSamplerAddress(input.addressU);
	native.AddressV = D3D11Detail::ToSamplerAddress(input.addressV);
	native.AddressW = D3D11Detail::ToSamplerAddress(input.addressW);
	native.MipLODBias = input.mipLodBias;
	native.MaxAnisotropy = input.anisotropyEnable ? input.maxAnisotropy : 1;
	native.ComparisonFunc = input.comparisonEnable
		? D3D11Detail::ToComparison(input.comparisonFunction)
		: D3D11_COMPARISON_NEVER;
	for(size_t i = 0; i < input.borderColor.size(); ++i) native.BorderColor[i] = input.borderColor[i];
	native.MinLOD = input.minLod;
	native.MaxLOD = input.maxLod;

	D3D11SamplerResource sampler;
	sampler.desc = input;
	if(FAILED(m_device->CreateSamplerState(&native, sampler.object.GetAddressOf()))) return {};
	return m_samplers.Create(std::move(sampler));
}

inline bool D3D11RHIDevice::DestroySampler(SamplerHandle handle){
	return m_samplers.Destroy(handle);
}

inline const SamplerDesc* D3D11RHIDevice::GetSamplerDesc(SamplerHandle handle) const {
	const auto* sampler = Find(handle);
	return sampler ? &sampler->desc : nullptr;
}

} // namespace RHI
