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

	m_Textures[filePath] = std::make_shared<TextureData>();
	DirectX::TexMetadata _metadata;
	DirectX::ScratchImage _image;

	m_Textures[filePath]->TexturePath = filePath;
	
	//テクスチャ読み込み
	if(isTgaFile){

		LoadFromTGAFile(filePath.c_str(), &_metadata, _image);

	} else{
		
		LoadFromWICFile((wchar_t*)filePath.c_str(), DirectX::WIC_FLAGS::WIC_FLAGS_NONE, &_metadata, _image);
	}
	//読み込んだ画像データをDirectXへ渡してテクスチャとして管理させる
	CreateShaderResourceView(m_GraphicContext->GetDevice(), _image.GetImages(), _image.GetImageCount(), _metadata, m_Textures[filePath]->pTexture.GetAddressOf());

	m_Textures[filePath]->Width  = (int)_metadata.width;
	m_Textures[filePath]->Height = (int)_metadata.height;


	assert(m_Textures[filePath]->pTexture.Get());

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
