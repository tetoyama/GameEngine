// =======================================================================
// 
// D2DRenderer.h
// 
// =======================================================================
#pragma once
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <string>
#include <DirectXMath.h>

// GraphicsContext クラスの前方宣言
class GraphicsContext;

// Direct2D を用いた2Dテキスト描画クラス
class D2DRenderer {
public:
	D2DRenderer(GraphicsContext* context, HWND hwnd);
	~D2DRenderer();

	void DrawText2D(const std::wstring& text, float x, float y, float fontSize, D2D1::ColorF color);
	void BeginDraw();
	void EndDraw();
	void ReloadTextFormat();
	Microsoft::WRL::ComPtr<IDWriteTextLayout> CreateTextLayout(const std::wstring& text);
	void DrawTextLayout(IDWriteTextLayout* textLayout, const DirectX::XMFLOAT2& pos, const DirectX::XMFLOAT4& color);
	void RenderText(const std::wstring& text, const DirectX::XMFLOAT2& pos, const DirectX::XMFLOAT4& color);
	void SetTextSize(float size, bool reload = true);
	void SetTextFont(const std::wstring& name, bool reload = true);
	void SetTextWeight(DWRITE_FONT_WEIGHT weight, bool reload = true);
	void SetTextStyle(DWRITE_FONT_STYLE style, bool reload = true);
	void SetTextStretch(DWRITE_FONT_STRETCH stretch, bool reload = true);

	// D2Dリソース解放
	void OnResizeRelease(){
		if(m_pD2dRenderTarget){
			m_pD2dRenderTarget->Release();
			m_pD2dRenderTarget = nullptr;
		}

		if(m_pFontBrush){
			m_pFontBrush->Release();
			m_pFontBrush = nullptr;
		}

		if(m_pDwriteFactory){
			m_pDwriteFactory->Release();
			m_pDwriteFactory = nullptr;
		}
	}

	// D2Dリソース再生成
	void OnResizeRecreate(){
		Initialize2DResources();
	}
private:
	void Initialize2DResources();

	GraphicsContext* m_pGraphicsContext = nullptr;
	HWND m_Hwnd;

	ID2D1RenderTarget* m_pD2dRenderTarget = nullptr;
	ID2D1SolidColorBrush* m_pFontBrush = nullptr;
	IDWriteFactory* m_pDwriteFactory = nullptr;
	IDWriteTextFormat* m_pTextFormat = nullptr;

	std::wstring m_FontName= L"メイリオ";
	float m_FontSize= 24.0f;
	DWRITE_FONT_WEIGHT m_FontWeight= DWRITE_FONT_WEIGHT_NORMAL;
	DWRITE_FONT_STYLE m_FontStyle= DWRITE_FONT_STYLE_NORMAL;
	DWRITE_FONT_STRETCH m_FontStretch= DWRITE_FONT_STRETCH_NORMAL;
};
