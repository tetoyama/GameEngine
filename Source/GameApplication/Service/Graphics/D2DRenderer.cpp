// =======================================================================
// 
// D2DRenderer.cpp
// 
// =======================================================================
#include "D2DRenderer.h"
#include "GraphicsContext.h"
#include <d2d1helper.h>
#include <dwrite.h>
#include <wrl/client.h>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

#define SAFE_RELEASE(p) if(p){ p->Release(); p = nullptr;}

D2DRenderer::D2DRenderer(GraphicsContext* context, HWND hwnd)
	: m_pGraphicsContext(context), m_hwnd(hwnd){
	Initialize2DResources();
}

D2DRenderer::~D2DRenderer(){
	SAFE_RELEASE(m_pD2dRenderTarget);
	SAFE_RELEASE(m_pDwriteFactory);
	SAFE_RELEASE(m_pFontBrush);
	SAFE_RELEASE(m_pTextFormat);
}

void D2DRenderer::Initialize2DResources(){
	// DXGIサーフェス取得
	Microsoft::WRL::ComPtr<IDXGISurface> m_DxgiSurface;

	HRESULT m_Hr= m_pGraphicsContext->GetSwapChain()->GetBuffer(0, IID_PPV_ARGS(&dxgiSurface));
	if(FAILED(hr)){
		OutputDebugStringA("DXGIサーフェスの取得に失敗しました。\n");
		return;
	}

	ID2D1Factory* d2dFactory = m_pGraphicsContext->GetD2DFactory();
	if(!d2dFactory){
		OutputDebugStringA("D2Dファクトリの取得に失敗しました。\n");
		return;
	}

	// DPI取得
	FLOAT m_Dpi= static_cast<FLOAT>(GetDpiForWindow(m_hwnd));

	// RenderTarget作成
	D2D1_RENDER_TARGET_PROPERTIES m_Props= D2D1::RenderTargetProperties(
		D2D1_RENDER_TARGET_TYPE_DEFAULT,
		D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
		dpi, dpi
	);
	hr = d2dFactory->CreateDxgiSurfaceRenderTarget(dxgiSurface.Get(), &props, &m_pD2dRenderTarget);
	if(FAILED(hr)){
		OutputDebugStringA("D2Dレンダーターゲットの作成に失敗しました。\n");
		return;
	}

	hr = m_pD2dRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &m_pFontBrush);
	if(FAILED(hr)){
		OutputDebugStringA("SolidColorBrushの作成に失敗しました。\n");
		return;
	}

	hr = DWriteCreateFactory(
		DWRITE_FACTORY_TYPE_SHARED,
		__uuidof(IDWriteFactory),
		reinterpret_cast<IUnknown**>(&m_pDwriteFactory)
	);
	if(FAILED(hr)){
		OutputDebugStringA("DirectWriteファクトリの作成に失敗しました。\n");
		return;
	}

	ReloadTextFormat();
}

void D2DRenderer::BeginDraw(){
	if(m_pD2dRenderTarget){
		m_pD2dRenderTarget->BeginDraw();
	}
}

void D2DRenderer::EndDraw(){
	if(m_pD2dRenderTarget){
		m_pD2dRenderTarget->EndDraw();
	}
}

void D2DRenderer::DrawText2D(const std::wstring& text, float x, float y, float fontSize, D2D1::ColorF color){
	if(!m_pD2dRenderTarget || !m_pFontBrush || !m_pDwriteFactory) return;

	m_pFontBrush->SetColor(color);

	Microsoft::WRL::ComPtr<IDWriteTextFormat> m_TextFormat;
	m_pDwriteFactory->CreateTextFormat(
		L"メイリオ", nullptr,
		DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
		fontSize, L"ja-jp", &textFormat);

	D2D1_SIZE_F m_RtSize= m_pD2dRenderTarget->GetSize();
	D2D1_RECT_F m_LayoutRect= D2D1::RectF(x, y, rtSize.width, rtSize.height);

	BeginDraw();
	m_pD2dRenderTarget->DrawTextW(
		text.c_str(), (UINT32)text.length(),
		textFormat.Get(), layoutRect, m_pFontBrush
	);
	EndDraw();
}

// テキストフォーマットの再生成
void D2DRenderer::ReloadTextFormat(){
	if(m_pTextFormat){
		m_pTextFormat->Release();
	}

	m_pGraphicsContext->GetDWriteFactory()->CreateTextFormat(
		m_fontName.c_str(),
		nullptr,
		m_fontWeight,
		m_fontStyle,
		m_fontStretch,
		m_fontSize,
		L"ja-jp",
		&m_pTextFormat
	);
}

// テキストレイアウトの作成
Microsoft::WRL::ComPtr<IDWriteTextLayout> D2DRenderer::CreateTextLayout(const std::wstring& text){
	D2D1_SIZE_F m_Size= m_pD2dRenderTarget->GetSize();
	Microsoft::WRL::ComPtr<IDWriteTextLayout> m_TextLayout;
	m_pGraphicsContext->GetDWriteFactory()->CreateTextLayout(
		text.c_str(),
		(UINT32)text.length(),
		m_pTextFormat,
		size.width,
		size.height,
		&textLayout
	);
	return m_TextLayout;
}

// テキストレイアウト描画
void D2DRenderer::DrawTextLayout(
	IDWriteTextLayout* textLayout,
	const DirectX::XMFLOAT2& pos,
	const DirectX::XMFLOAT4& color
){
	Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_Brush;
	m_pD2dRenderTarget->CreateSolidColorBrush(
		D2D1::ColorF(color.x, color.y, color.z, color.w),
		&brush
	);
	m_pD2dRenderTarget->BeginDraw();
	m_pD2dRenderTarget->DrawTextLayout(
		D2D1::Point2F(pos.x, pos.y),
		textLayout,
		brush.Get()
	);
	m_pD2dRenderTarget->EndDraw();
}

// テキスト描画（レイアウト生成＋描画）
void D2DRenderer::RenderText(
	const std::wstring& text,
	const DirectX::XMFLOAT2& pos,
	const DirectX::XMFLOAT4& color
){
	auto m_TextLayout= CreateTextLayout(text);
	if(textLayout){
		DrawTextLayout(textLayout.Get(), pos, color);
	}
}

// フォント設定
void D2DRenderer::SetTextSize(float size, bool reload){
	m_fontSize = size;
	if(reload) ReloadTextFormat();
}
void D2DRenderer::SetTextFont(const std::wstring& name, bool reload){
	m_fontName = name;
	if(reload) ReloadTextFormat();
}
void D2DRenderer::SetTextWeight(DWRITE_FONT_WEIGHT weight, bool reload){
	m_fontWeight = weight;
	if(reload) ReloadTextFormat();
}
void D2DRenderer::SetTextStyle(DWRITE_FONT_STYLE style, bool reload){
	m_fontStyle = style;
	if(reload) ReloadTextFormat();
}
void D2DRenderer::SetTextStretch(DWRITE_FONT_STRETCH stretch, bool reload){
	m_fontStretch = stretch;
	if(reload) ReloadTextFormat();
}
