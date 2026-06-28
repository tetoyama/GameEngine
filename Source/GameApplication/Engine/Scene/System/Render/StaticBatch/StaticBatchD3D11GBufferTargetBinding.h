#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <d3d11.h>
#include <wrl/client.h>

class StaticBatchD3D11GBufferTargetBinding {
public:
	static constexpr std::size_t ColorTargetCount = 6;

	bool Resolve(
		ID3D11Device* expectedDevice,
		const std::array<ID3D11RenderTargetView*, ColorTargetCount>& targets,
		ID3D11DepthStencilView* depthTarget
	){
		Reset();
		if(!expectedDevice || !depthTarget) return false;

		D3D11_TEXTURE2D_DESC referenceDesc{};
		for(std::size_t index = 0; index < targets.size(); ++index){
			ID3D11RenderTargetView* target = targets[index];
			if(!target || !ValidateDevice(expectedDevice, target)){
				Reset();
				return false;
			}

			Microsoft::WRL::ComPtr<ID3D11Resource> resource;
			target->GetResource(resource.GetAddressOf());
			Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
			if(!resource || FAILED(resource.As(&texture)) || !texture){
				Reset();
				return false;
			}

			D3D11_TEXTURE2D_DESC textureDesc{};
			texture->GetDesc(&textureDesc);
			const DXGI_FORMAT expectedFormat = index < 5
				? DXGI_FORMAT_R16G16B16A16_FLOAT
				: DXGI_FORMAT_R32G32B32A32_UINT;
			if(textureDesc.Format != expectedFormat ||
				textureDesc.Width == 0 || textureDesc.Height == 0 ||
				textureDesc.SampleDesc.Count == 0 ||
				(textureDesc.BindFlags & D3D11_BIND_RENDER_TARGET) == 0){
				Reset();
				return false;
			}

			if(index == 0){
				referenceDesc = textureDesc;
			}else if(textureDesc.Width != referenceDesc.Width ||
				textureDesc.Height != referenceDesc.Height ||
				textureDesc.SampleDesc.Count != referenceDesc.SampleDesc.Count ||
				textureDesc.SampleDesc.Quality != referenceDesc.SampleDesc.Quality){
				Reset();
				return false;
			}
			m_targets[index] = target;
		}

		if(!ValidateDevice(expectedDevice, depthTarget)){
			Reset();
			return false;
		}
		Microsoft::WRL::ComPtr<ID3D11Resource> depthResource;
		depthTarget->GetResource(depthResource.GetAddressOf());
		Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTexture;
		if(!depthResource || FAILED(depthResource.As(&depthTexture)) || !depthTexture){
			Reset();
			return false;
		}

		D3D11_TEXTURE2D_DESC depthDesc{};
		depthTexture->GetDesc(&depthDesc);
		if(depthDesc.Width != referenceDesc.Width ||
			depthDesc.Height != referenceDesc.Height ||
			depthDesc.SampleDesc.Count != referenceDesc.SampleDesc.Count ||
			depthDesc.SampleDesc.Quality != referenceDesc.SampleDesc.Quality ||
			(depthDesc.BindFlags & D3D11_BIND_DEPTH_STENCIL) == 0){
			Reset();
			return false;
		}

		D3D11_DEPTH_STENCIL_VIEW_DESC depthViewDesc{};
		depthTarget->GetDesc(&depthViewDesc);
		if(depthViewDesc.Format != DXGI_FORMAT_D32_FLOAT){
			Reset();
			return false;
		}

		m_depthTarget = depthTarget;
		m_width = referenceDesc.Width;
		m_height = referenceDesc.Height;
		m_sampleCount = referenceDesc.SampleDesc.Count;
		return true;
	}

	bool Bind(ID3D11DeviceContext* context) const noexcept {
		if(!context || !IsValid()) return false;
		std::array<ID3D11RenderTargetView*, ColorTargetCount> targets{};
		for(std::size_t index = 0; index < targets.size(); ++index){
			targets[index] = m_targets[index].Get();
		}
		context->OMSetRenderTargets(
			static_cast<UINT>(targets.size()),
			targets.data(),
			m_depthTarget.Get()
		);
		return true;
	}

	void Reset() noexcept {
		for(auto& target : m_targets) target.Reset();
		m_depthTarget.Reset();
		m_width = 0;
		m_height = 0;
		m_sampleCount = 0;
	}

	bool IsValid() const noexcept {
		if(!m_depthTarget || m_width == 0 || m_height == 0 ||
			m_sampleCount == 0){
			return false;
		}
		for(const auto& target : m_targets){
			if(!target) return false;
		}
		return true;
	}

	std::uint32_t Width() const noexcept { return m_width; }
	std::uint32_t Height() const noexcept { return m_height; }
	std::uint32_t SampleCount() const noexcept { return m_sampleCount; }

private:
	template<typename TView>
	static bool ValidateDevice(
		ID3D11Device* expectedDevice,
		TView* view
	) noexcept {
		Microsoft::WRL::ComPtr<ID3D11Device> sourceDevice;
		view->GetDevice(sourceDevice.GetAddressOf());
		return sourceDevice.Get() == expectedDevice;
	}

	std::array<
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView>,
		ColorTargetCount
	> m_targets;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_depthTarget;
	std::uint32_t m_width = 0;
	std::uint32_t m_height = 0;
	std::uint32_t m_sampleCount = 0;
};
