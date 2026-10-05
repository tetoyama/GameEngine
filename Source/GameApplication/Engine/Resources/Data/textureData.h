// =======================================================================
// 
// textureData.h
// 
// =======================================================================
#pragma once
#include <string>
#include <d3d11.h>
#include <wrl/client.h> 
#include "Backends/DirectX11/DirectXTex.h"
#include "Service/Graphics/RHI/RHIInterfaces.h"

// テクスチャリソースのデータを保持する構造体
struct TextureData {
	TextureData(){
		OutputDebugStringA("Created TextureData\n");
	}
	~TextureData(){
		OutputDebugStringA(("Destroyed TextureData: " + FilePath + "\n").c_str());
		pTexture.Reset();
		ResetRHI();
	}
	TextureData(const TextureData&) = delete;
	TextureData& operator=(const TextureData&) = delete;

	// Temporary editor bridge: reuse the resource already loaded by the
	// existing ResourceService. No second texture cache or file loader.
	// Native RHI resources belong to this TextureData and follow its lifetime.
	RHI::TextureViewHandle EnsureRHI(RHI::IRHIDevice& device,
		ID3D11Device* sourceDevice, ID3D11DeviceContext* sourceContext) {
		if(m_rhiDevice == &device && !m_rhiLifetime.expired() && m_rhiView &&
			m_rhiSource.Get() == pTexture.Get()) return m_rhiView;
		ResetRHI();
		if(!pTexture || !sourceDevice || !sourceContext) return {};
		D3D11_SHADER_RESOURCE_VIEW_DESC sourceView{}; pTexture->GetDesc(&sourceView);
		if(sourceView.ViewDimension!=D3D11_SRV_DIMENSION_TEXTURE2D || sourceView.Texture2D.MostDetailedMip!=0) return {};
		Microsoft::WRL::ComPtr<ID3D11Resource> resource;
		pTexture->GetResource(&resource);
		DirectX::ScratchImage source, decompressed, converted;
		if(FAILED(DirectX::CaptureTexture(sourceDevice, sourceContext, resource.Get(), source))) return {};
		const auto& metadata = source.GetMetadata();
		if(metadata.arraySize != 1 || metadata.dimension != DirectX::TEX_DIMENSION_TEXTURE2D) return {};
		const auto* image = source.GetImage(0,0,0);
		if(!image) return {};
		if(DirectX::IsCompressed(image->format)) {
			if(FAILED(DirectX::Decompress(*image, DXGI_FORMAT_R32G32B32A32_FLOAT, decompressed))) return {};
			image = decompressed.GetImage(0,0,0);
		}
		if(image->format != DXGI_FORMAT_R8G8B8A8_UNORM) {
			if(FAILED(DirectX::Convert(*image, DXGI_FORMAT_R8G8B8A8_UNORM,
				DirectX::TEX_FILTER_DEFAULT, .5f, converted))) return {};
			image = converted.GetImage(0,0,0);
		}
		RHI::TextureDesc desc;
		desc.width=static_cast<uint32_t>(image->width); desc.height=static_cast<uint32_t>(image->height);
		desc.initialState=RHI::ResourceState::ShaderResource; desc.debugName=FilePath;
		auto texture=device.CreateTexture(desc,
			{reinterpret_cast<const std::byte*>(image->pixels),image->slicePitch},static_cast<uint32_t>(image->rowPitch));
		if(!texture) return {};
		RHI::TextureViewDesc view; view.texture=texture;
		auto binding=device.CreateTextureView(view);
		if(!binding) { device.DestroyTexture(texture); return {}; }
		m_rhiDevice=&device; m_rhiLifetime=device.GetLifetimeToken(); m_rhiTexture=texture; m_rhiView=binding;
		m_rhiSource=pTexture;
		return m_rhiView;
	}
	void ResetRHI() noexcept {
		if(m_rhiDevice && !m_rhiLifetime.expired()) {
			if(m_rhiView) m_rhiDevice->DestroyTextureView(m_rhiView);
			if(m_rhiTexture) m_rhiDevice->DestroyTexture(m_rhiTexture);
		}
		m_rhiDevice=nullptr; m_rhiLifetime.reset(); m_rhiTexture={}; m_rhiView={}; m_rhiSource.Reset();
	}
	std::string FilePath;
	Microsoft::WRL::ComPtr <ID3D11ShaderResourceView> pTexture;	//ポインター
	int Width = 0;
	int Height = 0;
private:
	RHI::IRHIDevice* m_rhiDevice = nullptr;
	RHI::IRHIDevice::LifetimeToken m_rhiLifetime;
	RHI::TextureHandle m_rhiTexture;
	RHI::TextureViewHandle m_rhiView;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_rhiSource;
};
