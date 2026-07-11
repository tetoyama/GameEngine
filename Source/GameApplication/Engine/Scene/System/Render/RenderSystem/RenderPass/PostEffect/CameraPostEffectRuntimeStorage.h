// =======================================================================
//
// CameraPostEffectRuntimeStorage.h
//
// Step 18-A: CameraComponentからD3D11 PostEffect資源所有権を分離する。
// PostEffectPassごとにCamera / Effect Index単位のRuntimeを保持する。
//
// =======================================================================
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <unordered_map>

#include <d3d11.h>
#include <wrl/client.h>

#include "Backends/myVector2.h"

struct CameraPostEffectRuntimeKey {
	std::uint32_t sceneContextID = 0;
	std::uint64_t cameraEntity = 0;
	int effectIndex = -1;

	bool operator==(const CameraPostEffectRuntimeKey&) const noexcept = default;
};

struct CameraPostEffectRuntimeKeyHash {
	std::size_t operator()(const CameraPostEffectRuntimeKey& key) const noexcept {
		std::size_t hash = static_cast<std::size_t>(key.sceneContextID);
		hash ^= static_cast<std::size_t>(key.cameraEntity) +
			0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
		hash ^= static_cast<std::size_t>(static_cast<std::uint32_t>(key.effectIndex)) +
			0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
		return hash;
	}
};

struct CameraPostEffectRuntime {
	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView;
	Vector2 resolution{0.0f, 0.0f};
	int mipLevels = 0;
	std::uint64_t lastUsedGeneration = 0;

	bool IsValid() const noexcept {
		return texture && renderTargetView && shaderResourceView;
	}

	bool Ensure(
		ID3D11Device* device,
		const Vector2& requestedSize,
		int requestedMipLevels
	){
		if(!device){
			return false;
		}

		const Vector2 normalizedSize{
			(std::max)(requestedSize.x, 1.0f),
			(std::max)(requestedSize.y, 1.0f)
		};
		const int normalizedMipLevels = (std::max)(requestedMipLevels, 1);
		if(IsValid() &&
			resolution.x == normalizedSize.x &&
			resolution.y == normalizedSize.y &&
			mipLevels == normalizedMipLevels){
			return true;
		}

		Microsoft::WRL::ComPtr<ID3D11Texture2D> newTexture;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> newRenderTargetView;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> newShaderResourceView;

		D3D11_TEXTURE2D_DESC textureDesc{};
		textureDesc.Width = static_cast<UINT>(normalizedSize.x);
		textureDesc.Height = static_cast<UINT>(normalizedSize.y);
		textureDesc.MipLevels = static_cast<UINT>(normalizedMipLevels);
		textureDesc.ArraySize = 1;
		textureDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.Usage = D3D11_USAGE_DEFAULT;
		textureDesc.BindFlags =
			D3D11_BIND_RENDER_TARGET |
			D3D11_BIND_SHADER_RESOURCE;
		if(normalizedMipLevels > 1){
			textureDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
		}

		HRESULT result = device->CreateTexture2D(
			&textureDesc,
			nullptr,
			newTexture.GetAddressOf()
		);
		if(FAILED(result)){
			return false;
		}

		D3D11_RENDER_TARGET_VIEW_DESC renderTargetDesc{};
		renderTargetDesc.Format = textureDesc.Format;
		renderTargetDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		renderTargetDesc.Texture2D.MipSlice = 0;
		result = device->CreateRenderTargetView(
			newTexture.Get(),
			&renderTargetDesc,
			newRenderTargetView.GetAddressOf()
		);
		if(FAILED(result)){
			return false;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceDesc{};
		shaderResourceDesc.Format = textureDesc.Format;
		shaderResourceDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		shaderResourceDesc.Texture2D.MostDetailedMip = 0;
		shaderResourceDesc.Texture2D.MipLevels =
			static_cast<UINT>(normalizedMipLevels);
		result = device->CreateShaderResourceView(
			newTexture.Get(),
			&shaderResourceDesc,
			newShaderResourceView.GetAddressOf()
		);
		if(FAILED(result)){
			return false;
		}

		// Transactional commit: 3資源すべて成功するまで旧Runtimeを維持する。
		texture = std::move(newTexture);
		renderTargetView = std::move(newRenderTargetView);
		shaderResourceView = std::move(newShaderResourceView);
		resolution = normalizedSize;
		mipLevels = normalizedMipLevels;
		return true;
	}

	void Clear(ID3D11DeviceContext* context, const float clearColor[4]) const {
		if(context && renderTargetView && clearColor){
			context->ClearRenderTargetView(renderTargetView.Get(), clearColor);
		}
	}
};

class CameraPostEffectRuntimeStorage final {
public:
	std::uint64_t BeginCamera(
		std::uint32_t sceneContextID,
		std::uint64_t cameraEntity
	) noexcept {
		m_activeSceneContextID = sceneContextID;
		m_activeCameraEntity = cameraEntity;
		return ++m_generation;
	}

	CameraPostEffectRuntime& Acquire(
		const CameraPostEffectRuntimeKey& key,
		std::uint64_t generation
	){
		CameraPostEffectRuntime& runtime = m_entries[key];
		runtime.lastUsedGeneration = generation;
		return runtime;
	}

	void EndCamera(std::uint64_t generation){
		for(auto it = m_entries.begin(); it != m_entries.end();){
			const CameraPostEffectRuntimeKey& key = it->first;
			const bool belongsToActiveCamera =
				key.sceneContextID == m_activeSceneContextID &&
				key.cameraEntity == m_activeCameraEntity;
			if(belongsToActiveCamera &&
				it->second.lastUsedGeneration != generation){
				it = m_entries.erase(it);
			}else{
				++it;
			}
		}
	}

	void Reset() noexcept {
		m_entries.clear();
		m_generation = 0;
		m_activeSceneContextID = 0;
		m_activeCameraEntity = 0;
	}

	std::size_t Size() const noexcept {
		return m_entries.size();
	}

	bool Contains(const CameraPostEffectRuntimeKey& key) const {
		return m_entries.contains(key);
	}

private:
	std::unordered_map<
		CameraPostEffectRuntimeKey,
		CameraPostEffectRuntime,
		CameraPostEffectRuntimeKeyHash
	> m_entries;
	std::uint64_t m_generation = 0;
	std::uint32_t m_activeSceneContextID = 0;
	std::uint64_t m_activeCameraEntity = 0;
};
