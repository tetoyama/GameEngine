#pragma once

#include <d3d11.h>
#include <wrl/client.h>

class StaticBatchShadowPixelState final {
public:
	StaticBatchShadowPixelState(
		ID3D11DeviceContext* context,
		UINT textureSlot
	) noexcept
		: m_context(context),
		  m_textureSlot(textureSlot) {
		if(!m_context) return;
		m_context->PSGetShaderResources(
			m_textureSlot,
			1,
			m_previousTexture.ReleaseAndGetAddressOf()
		);
		m_context->PSGetSamplers(
			0,
			1,
			m_materialSampler.ReleaseAndGetAddressOf()
		);
	}

	~StaticBatchShadowPixelState(){
		if(!m_context) return;
		ID3D11ShaderResourceView* texture = m_previousTexture.Get();
		m_context->PSSetShaderResources(m_textureSlot, 1, &texture);
		ID3D11SamplerState* sampler = m_materialSampler.Get();
		m_context->PSSetSamplers(0, 1, &sampler);
	}

	StaticBatchShadowPixelState(const StaticBatchShadowPixelState&) = delete;
	StaticBatchShadowPixelState& operator=(
		const StaticBatchShadowPixelState&
	) = delete;

	bool CanSampleDiffuseTexture() const noexcept {
		return m_context != nullptr && m_materialSampler.Get() != nullptr;
	}

	bool BindDiffuseTexture(ID3D11ShaderResourceView* texture) noexcept {
		if(!m_context) return false;
		m_context->PSSetShaderResources(m_textureSlot, 1, &texture);
		if(texture){
			ID3D11SamplerState* sampler = m_materialSampler.Get();
			m_context->PSSetSamplers(0, 1, &sampler);
		}
		return true;
	}

private:
	ID3D11DeviceContext* m_context = nullptr;
	UINT m_textureSlot = 0;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_previousTexture;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> m_materialSampler;
};
