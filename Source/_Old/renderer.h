#pragma once
#include <string>
//計算
#include <DirectXMath.h>
using namespace DirectX;
//3D描画
#include <d3d11.h>
//2D描画
#include <d2d1.h>
#include <dwrite.h>

#define SCREEN_RATIO_X (Renderer::GetViewportSize().x/ SCREEN_WIDTH)
#define SCREEN_RATIO_Y (Renderer::GetViewportSize().y/ SCREEN_HEIGHT)

#define ANCHOR_XL(_x) (_x / SCREEN_RATIO_X)
#define ANCHOR_XC(_x) (SCREEN_WIDTH / 2 + _x)
#define ANCHOR_XR(_x) ((Renderer::GetViewportSize().x - _x) / SCREEN_RATIO_X)

#define ANCHOR_YU(_y) (_y / SCREEN_RATIO_Y)
#define ANCHOR_YC(_y) (SCREEN_HEIGHT / 2 + _y)
#define ANCHOR_YD(_y) ((Renderer::GetViewportSize().y - _y) / SCREEN_RATIO_Y)

struct VERTEX_3D {
	XMFLOAT3 Position;
	XMFLOAT3 Normal;
	XMFLOAT4 Diffuse;
	XMFLOAT2 TexCoord;
};

struct MATERIAL {
	XMFLOAT4	Ambient;
	XMFLOAT4	Diffuse;
	XMFLOAT4	Specular;
	XMFLOAT4	Emission;
	float		Shininess;
	BOOL		TextureEnable;
	float		Dummy[2];
};

struct LIGHT {
	BOOL		Enable;
	BOOL		Dummy[3];
	XMFLOAT4	Direction;
	XMFLOAT4	Diffuse;
	XMFLOAT4	Ambient;
};

class Renderer {
public:
	static void Init();
	static void Uninit();
	static void Begin();
	static void End();

	//リサイズ
	static void ResizeRenderer(XMFLOAT2 SetSize);

	//セッター
	static void SetDepthEnable(bool Enable);
	static void SetATCEnable(bool Enable);
	static void SetWorldViewProjection2D();
	static void SetWorldMatrix(XMMATRIX WorldMatrix);
	static void SetViewMatrix(XMMATRIX ViewMatrix);
	static void SetProjectionMatrix(XMMATRIX ProjectionMatrix);
	static void SetMaterial(MATERIAL Material);
	static void SetLight(LIGHT Light);

	//ゲッター
	static ID3D11Device* GetDevice(void) { return m_Device; }
	static ID3D11DeviceContext* GetDeviceContext(void) { return m_DeviceContext; }
	static XMFLOAT2 GetViewportSize() { return m_ViewPortSize; }

	//文字描画
	static void RenderText(WCHAR* pWCText, XMFLOAT2 StartPos, XMFLOAT4 Color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));

	static IDWriteTextLayout* CreateTextLayout(WCHAR* pWCText);
	static void DrawTextLayout(IDWriteTextLayout* pTextLayout, XMFLOAT2 StartPos = XMFLOAT2(0.0f, 0.0f), XMFLOAT4 Color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));

	static void SetTextSize(float SetSize = 1.0f, bool Reload = true);
	static void SetTextFont(std::wstring SetName, bool Reload = true);

	static void SetTextWeight(DWRITE_FONT_WEIGHT SetWeight, bool Reload = true);
	static void SetTextStyle(DWRITE_FONT_STYLE SetStyle, bool Reload = true);
	static void SetTextStretch(DWRITE_FONT_STRETCH SetStretch, bool Reload = true);

	//シェーダー
	static void CreateVertexShader(ID3D11VertexShader** VertexShader, ID3D11InputLayout** VertexLayout, const char* FileName);
	static void CreatePixelShader(ID3D11PixelShader** PixelShader, const char* FileName);

private:

	static D3D_FEATURE_LEVEL       m_FeatureLevel;

	static ID3D11Device* m_Device;
	static ID3D11DeviceContext* m_DeviceContext;
	static IDXGISwapChain* m_SwapChain;
	static ID3D11RenderTargetView* m_RenderTargetView;
	static ID3D11DepthStencilView* m_DepthStencilView;

	static ID3D11Buffer* m_WorldBuffer;
	static ID3D11Buffer* m_ViewBuffer;
	static ID3D11Buffer* m_ProjectionBuffer;
	static ID3D11Buffer* m_MaterialBuffer;
	static ID3D11Buffer* m_LightBuffer;


	static ID3D11DepthStencilState* m_DepthStateEnable;
	static ID3D11DepthStencilState* m_DepthStateDisable;

	static ID3D11BlendState* m_BlendState;
	static ID3D11BlendState* m_BlendStateATC;

	//テキスト表示用
	static ID2D1Factory* m_pD2DFactory;
	static IDWriteFactory* m_pDWriteFactory;
	static IDWriteTextFormat* m_pTextFormat;
	static ID2D1RenderTarget* m_pRT;
	static ID2D1SolidColorBrush* m_pSolidBrush;
	static IDXGISurface* m_pBackBuffer;

	static std::wstring				m_FontName;
	static std::wstring				m_FontLocate;
	static float					m_FontSize;
	static DWRITE_FONT_WEIGHT		m_FontWeight;
	static DWRITE_FONT_STYLE		m_FontStyle;
	static DWRITE_FONT_STRETCH		m_FontStretch;

	static XMFLOAT2 m_ViewPortSize;

	static void ReloadTextFormat();
	static void CreateNewViewPort(XMFLOAT2 SetSize, XMFLOAT2 WindowSize);

};
