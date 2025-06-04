#include "textureLoader.h"

/*

//========================================================================
//
// テクスチャ管理	[texture.cpp]
// 
//========================================================================
#include "TextureLoader.h"
#include "Engine/Graphics/graphicsContext.h"
#include <vector>

//テクスチャサポートライブラリ
#include "Backends/DirectX11/DirectXTex.h"
#if _DEBUG
#pragma comment(lib,"DirectXTex_Debug.lib")
#else
#pragma comment(lib,"DirectXTex_Release.lib")
#endif

//========================================================================
// 構造体宣言
//========================================================================


//========================================================================
// グローバル変数
//========================================================================
static GraphicsContext* g_pGraphicsContext = nullptr;
static std::vector<Texture> g_Textures;
static int DefaultTextureID = -1;
//========================================================================
// 初期化
//========================================================================
void InitializeTexture(GraphicsContext* set){
	g_pGraphicsContext = set;
	g_Textures.resize(MAXTEXTURE);

	for(Texture t : g_Textures){
		t.UseCount = 0;
		t.pTexture = NULL;
		t.TexturePath[0] = '\0';
	}

	DefaultTextureID = LoadTgaTexture(DEFAULT_TEXTURE);
}

//========================================================================
// 終了
//========================================================================
void FinalizeTexture(void){

	UnloadTexture(DefaultTextureID);

	for(Texture t : g_Textures){

		//未解放時警告を出す
		assert(t.UseCount == 0);

		//消し残しを開放する
		if(t.UseCount > 0){
			t.pTexture->Release();
			t.pTexture = NULL;
		}
	}
}

//========================================================================
// テクスチャ読み込み
//========================================================================
int LoadTexture(const std::wstring& FileName){

	int SetTextureLocate = -1;
	{
		int i = 0;
		for(Texture& t : g_Textures){
			if(t.TexturePath == FileName && t.UseCount > 0){
				t.UseCount++;
				return i;
			}
			i++;
		}
	}
	for(int i = 0; i < g_Textures.size(); i++){

		if(g_Textures[i].UseCount <= 0){
			SetTextureLocate = i;
			g_Textures[i].UseCount = 1;
			break;
		}
	}
	//空きを見つけられなかった場合に警告を出す
	//assert(SetTextureLocate != -1);
	if(SetTextureLocate == -1){

		SetTextureLocate = (int)g_Textures.size();
		g_Textures.emplace_back(Texture());

	}
	g_Textures[SetTextureLocate].UseCount = 1;
	g_Textures[SetTextureLocate].TexturePath = FileName;

	DirectX::TexMetadata _metadata{};
	DirectX::ScratchImage _image{};
	//テクスチャ読み込み
	wchar_t* temp = (wchar_t*)FileName.c_str();
	LoadFromWICFile(temp, DirectX::WIC_FLAGS::WIC_FLAGS_NONE, &_metadata, _image);
	//読み込んだ画像データをDirectXへ渡してテクスチャとして管理させる
	CreateShaderResourceView(g_pGraphicsContext->GetDevice(), _image.GetImages(), _image.GetImageCount(), _metadata, &g_Textures[SetTextureLocate].pTexture);

	g_Textures[SetTextureLocate].Width = (int)_metadata.width;
	g_Textures[SetTextureLocate].Height = (int)_metadata.height;

	//失敗時に警告を出す
	assert(g_Textures[SetTextureLocate].pTexture);

	return SetTextureLocate;
}

//========================================================================
// テクスチャ読み込み(tgaファイル)
//========================================================================
int LoadTgaTexture(const std::wstring& FileName){

	int SetTextureLocate = -1;
	{
		int i = 0;
		for(Texture& t : g_Textures){
			if(t.TexturePath == FileName && t.UseCount > 0){
				t.UseCount++;
				return i;
			}
			i++;
		}
	}
	for(int i = 0; i < g_Textures.size(); i++){

		if(g_Textures[i].UseCount <= 0){
			SetTextureLocate = i;
			g_Textures[i].UseCount = 1;
			break;
		}
	}
	//空きを見つけられなかった場合に警告を出す
	//assert(SetTextureLocate != -1);
	if(SetTextureLocate == -1){

		SetTextureLocate = (int)g_Textures.size();
		g_Textures.emplace_back(Texture());

	}
	g_Textures[SetTextureLocate].UseCount = 1;
	g_Textures[SetTextureLocate].TexturePath = FileName;

	DirectX::TexMetadata _metadata{};
	DirectX::ScratchImage _image{};
	//テクスチャ読み込み
	LoadFromTGAFile(FileName.c_str(), &_metadata, _image);
	//読み込んだ画像データをDirectXへ渡してテクスチャとして管理させる
	CreateShaderResourceView(g_pGraphicsContext->GetDevice(), _image.GetImages(), _image.GetImageCount(), _metadata, &g_Textures[SetTextureLocate].pTexture);

	//失敗時に警告を出す
	assert(g_Textures[SetTextureLocate].pTexture);

	return SetTextureLocate;
}

//========================================================================
// テクスチャ開放
//========================================================================
void UnloadTexture(const unsigned int Texture){

	assert(g_Textures[Texture].UseCount > 0 && g_Textures[Texture].pTexture != NULL);
	if(g_Textures[Texture].UseCount > 0 && g_Textures[Texture].pTexture != NULL){

		g_Textures[Texture].UseCount--;

		if(0 == g_Textures[Texture].UseCount){
			g_Textures[Texture].pTexture->Release();
			g_Textures[Texture].pTexture = NULL;
		}
	}
}

//========================================================================
// テクスチャ設定
//========================================================================
void SetTexture(const unsigned int Texture){

	//無効なテクスチャの場合警告を出す
	assert(g_Textures[Texture].pTexture != NULL);
	assert(Texture >= 0);
	assert(Texture < g_Textures.size());
	if(Texture < 0 || g_Textures[Texture].pTexture == NULL){
		return;
	}
	//テクスチャ設定
	g_pGraphicsContext->GetDeviceContext()->PSSetShaderResources(0, 1, &g_Textures[Texture].pTexture);
}

//========================================================================
// テクスチャのサイズ取得
//========================================================================
float GetTextureWidth(const unsigned int Texture){
	return (float)g_Textures[Texture].Width;
}
float GetTextureHeight(unsigned int Texture){
	return (float)g_Textures[Texture].Height;
}
*/

#include <d3d11.h>

//テクスチャサポートライブラリ
#include "Backends/DirectX11/DirectXTex.h"
#if _DEBUG
#pragma comment(lib,"DirectXTex_Debug.lib")
#else
#pragma comment(lib,"DirectXTex_Release.lib")
#endif

#include "Backends/checkFileExtention.h"
#include "Engine/Resources/Data/textureData.h"
#include "Engine/Graphics/graphicsContext.h"

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
