#pragma once
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <string>
#include <DirectXMath.h>

// GraphicsContext クラスの前方宣言
class GraphicsContext;

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
		if(m_d2dRenderTarget){
			m_d2dRenderTarget->Release();
		}

		if(m_fontBrush){
			m_fontBrush->Release();
		}

		if(m_dwriteFactory){
			m_dwriteFactory->Release();
		}
	}

	// D2Dリソース再生成
	void OnResizeRecreate(){
		Initialize2DResources();
	}
private:
	void Initialize2DResources();

	GraphicsContext* m_graphicsContext = nullptr;
	HWND m_hwnd;

	ID2D1RenderTarget* m_d2dRenderTarget = nullptr;
	ID2D1SolidColorBrush* m_fontBrush = nullptr;
	IDWriteFactory* m_dwriteFactory = nullptr;
	IDWriteTextFormat* m_textFormat = nullptr;

	std::wstring m_fontName = L"メイリオ";
	float m_fontSize = 24.0f;
	DWRITE_FONT_WEIGHT m_fontWeight = DWRITE_FONT_WEIGHT_NORMAL;
	DWRITE_FONT_STYLE m_fontStyle = DWRITE_FONT_STYLE_NORMAL;
	DWRITE_FONT_STRETCH m_fontStretch = DWRITE_FONT_STRETCH_NORMAL;
};
