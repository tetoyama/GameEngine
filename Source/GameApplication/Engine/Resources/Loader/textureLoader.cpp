#include "textureLoader.h"

#include <d3d11.h>
#include "Backends/DirectX11/DirectXTex.h"

#include "Backends/checkFileExtention.h"

#include "Engine/Resources/Data/textureData.h"
#include "Engine/Graphics/graphicsContext.h"

#if _DEBUG
#pragma comment(lib,"DirectXTex_Debug.lib")
#else
#pragma comment(lib,"DirectXTex_Release.lib")
#endif

TextureData* TextureLoader::LoadTexture(const std::wstring& filePath){
	if(m_Textures.count(filePath)){
		return m_Textures[filePath].get();
	}
	bool isTgaFile = HasExtension(filePath, L"tga");

	std::shared_ptr<TextureData> textureData = std::make_shared<TextureData>();
	DirectX::TexMetadata _metadata{};
	DirectX::ScratchImage _image{};

	//テクスチャ読み込み
	if(isTgaFile){

		LoadFromTGAFile(filePath.c_str(), &_metadata, _image);

	} else{
		
		wchar_t* temp = (wchar_t*)filePath.c_str();
		LoadFromWICFile(temp, DirectX::WIC_FLAGS::WIC_FLAGS_NONE, &_metadata, _image);
	}
	//読み込んだ画像データをDirectXへ渡してテクスチャとして管理させる
	CreateShaderResourceView(m_GraphicContext->GetDevice(), _image.GetImages(), _image.GetImageCount(), _metadata, &textureData->pTexture);

	textureData->Width  = (int)_metadata.width;
	textureData->Height = (int)_metadata.height;

	m_Textures[filePath] = textureData;

	return m_Textures[filePath].get();
}

TextureData* TextureLoader::GetTexture(const std::wstring& filePath){
	auto it = m_Textures.find(filePath);
	if(it != m_Textures.end()){
		return it->second.get();
	}
	return nullptr;
}

void TextureLoader::SetTexture(const std::wstring& filePath){
	if(!m_Textures.count(filePath)){
		return;
	}
	TextureData* texture = m_Textures[filePath].get();
	m_GraphicContext->GetDeviceContext()->PSSetShaderResources(0, 1, &texture->pTexture);
}
