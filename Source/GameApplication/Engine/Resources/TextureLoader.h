#pragma once
//========================================================================
//
// テクスチャ管理	[texture.h]
// 
//========================================================================
#include <string>

class GraphicsContext;
//========================================================================
// マクロ定義
//========================================================================
constexpr auto MAXTEXTURE = (256);
constexpr auto DEFAULT_TEXTURE = (L"Asset\\Texture\\White.tga");

//========================================================================
// プロトタイプ宣言
//========================================================================
void InitializeTexture(GraphicsContext* set);
void FinalizeTexture(void);

int LoadTexture(const std::wstring& FileName);
int LoadTgaTexture(const std::wstring& FileName);

void UnloadTexture(const unsigned int Texture);
void SetTexture(const unsigned int Texture);

float GetTextureWidth(const unsigned int Texture);
float GetTextureHeight(const unsigned int Texture);
