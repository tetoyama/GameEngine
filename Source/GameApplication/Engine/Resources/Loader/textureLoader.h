// LoadTextureFromFile.h
#pragma once
#include "ResourceLoader.h"

#include <memory>
#include <string>
#include <d3d11.h>
#include <wrl/client.h>

#include "Engine/Resources/Data/textureData.h"
#include "Engine/Graphics/graphicsContext.h"
#include "Backends/DirectX11/DirectXTex.h"
#include "Backends/checkFileExtention.h"

#if _DEBUG
#pragma comment(lib,"DirectXTex_Debug.lib")
#else
#pragma comment(lib,"DirectXTex_Release.lib")
#endif

inline std::shared_ptr<TextureData> LoadTextureFromFile(const std::string& filePath, GraphicsContext* context) {
	std::shared_ptr<TextureData> tex = std::make_shared<TextureData>();
	tex->FilePath = filePath;

	bool isTgaFile = HasExtension(filePath, "tga");

	// UTF-8 → UTF-16 変換
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(), -1, NULL, 0);
	std::wstring w_FilePath(size_needed - 1, 0);
	MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(), -1, &w_FilePath[0], size_needed);

	DirectX::TexMetadata metadata;
	DirectX::ScratchImage image;

	HRESULT hr = S_OK;
	if (isTgaFile) {
		hr = DirectX::LoadFromTGAFile(w_FilePath.c_str(), &metadata, image);
	} else {
		hr = DirectX::LoadFromWICFile(w_FilePath.c_str(), DirectX::WIC_FLAGS_NONE, &metadata, image);
	}

	if (FAILED(hr)) {
		OutputDebugStringA(("Failed to load texture: " + filePath + "\n").c_str());
		return nullptr;
	}

	hr = DirectX::CreateShaderResourceView(
		context->GetDevice(),
		image.GetImages(),
		image.GetImageCount(),
		metadata,
		tex->pTexture.GetAddressOf()
	);

	if (FAILED(hr) || !tex->pTexture) {
		OutputDebugStringA(("Failed to create SRV for texture: " + filePath + "\n").c_str());
		return nullptr;
	}

	tex->Width = static_cast<int>(metadata.width);
	tex->Height = static_cast<int>(metadata.height);

	return tex;
}

template<>
inline void ResourceLoader<TextureData>::SetupLoadFunc(void* contextPtr) {
	OutputDebugStringA("SetupLoadFunc TextureData called\n");

	auto context = static_cast<GraphicsContext*>(contextPtr);
	SetLoadFunction([=](const std::string& path, void*) {
		return LoadTextureFromFile(path, context);
	});
}