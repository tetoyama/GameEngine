#include "D2DRenderer.h"
#include "GraphicsContext.h"
#include <d2d1helper.h>
#include <dwrite.h>
#include <wrl/client.h>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

#define SAFE_RELEASE(p) if(p){ p->Release(); p = nullptr;}

D2DRenderer::D2DRenderer(GraphicsContext* context, HWND hwnd)
	: m_graphicsContext(context), m_hwnd(hwnd){
	Initialize2DResources();
}

D2DRenderer::~D2DRenderer(){
	SAFE_RELEASE(m_d2dRenderTarget);
	SAFE_RELEASE(m_dwriteFactory);
	SAFE_RELEASE(m_fontBrush);
	SAFE_RELEASE(m_textFormat);
}

void D2DRenderer::Initialize2DResources(){
	// DXGIサーフェス取得
	Microsoft::WRL::ComPtr<IDXGISurface> dxgiSurface;

	HRESULT hr = m_graphicsContext->GetSwapChain()->GetBuffer(0, IID_PPV_ARGS(&dxgiSurface));
	if(FAILED(hr)){
		OutputDebugStringA("DXGIサーフェスの取得に失敗しました。\n");
		return;
	}

	ID2D1Factory* d2dFactory = m_graphicsContext->GetD2DFactory();
	if(!d2dFactory){
		OutputDebugStringA("D2Dファクトリの取得に失敗しました。\n");
		return;
	}

	// DPI取得
	UINT dpi = GetDpiForWindow(m_hwnd);
	FLOAT dpiX = static_cast<FLOAT>(dpi);
	FLOAT dpiY = static_cast<FLOAT>(dpi);

	// RenderTarget作成
	D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
		D2D1_RENDER_TARGET_TYPE_DEFAULT,
		D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
		dpiX, dpiY
	);
	hr = d2dFactory->CreateDxgiSurfaceRenderTarget(dxgiSurface.Get(), &props, &m_d2dRenderTarget);
	if(FAILED(hr)){
		OutputDebugStringA("D2Dレンダーターゲットの作成に失敗しました。\n");
		return;
	}

	hr = m_d2dRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &m_fontBrush);
	if(FAILED(hr)){
		OutputDebugStringA("SolidColorBrushの作成に失敗しました。\n");
		return;
	}

	hr = DWriteCreateFactory(
		DWRITE_FACTORY_TYPE_SHARED,
		__uuidof(IDWriteFactory),
		reinterpret_cast<IUnknown**>(&m_dwriteFactory)
	);
	if(FAILED(hr)){
		OutputDebugStringA("DirectWriteファクトリの作成に失敗しました。\n");
		return;
	}

	ReloadTextFormat();
}

void D2DRenderer::BeginDraw(){
	if(m_d2dRenderTarget){
		m_d2dRenderTarget->BeginDraw();
	}
}

void D2DRenderer::EndDraw(){
	if(m_d2dRenderTarget){
		m_d2dRenderTarget->EndDraw();
	}
}

void D2DRenderer::DrawText2D(const std::wstring& text, float x, float y, float fontSize, D2D1::ColorF color){
	if(!m_d2dRenderTarget || !m_fontBrush || !m_dwriteFactory) return;

	m_fontBrush->SetColor(color);

	Microsoft::WRL::ComPtr<IDWriteTextFormat> textFormat;
	m_dwriteFactory->CreateTextFormat(
		L"メイリオ", nullptr,
		DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
		fontSize, L"ja-jp", &textFormat);

	D2D1_SIZE_F rtSize = m_d2dRenderTarget->GetSize();
	D2D1_RECT_F layoutRect = D2D1::RectF(x, y, rtSize.width, rtSize.height);

	BeginDraw();
	m_d2dRenderTarget->DrawTextW(
		text.c_str(), (UINT32)text.length(),
		textFormat.Get(), layoutRect, m_fontBrush
	);
	EndDraw();
}

// テキストフォーマットの再生成
void D2DRenderer::ReloadTextFormat(){
	if(m_textFormat){
		m_textFormat->Release();
	}

	m_graphicsContext->GetDWriteFactory()->CreateTextFormat(
		m_fontName.c_str(),
		nullptr,
		m_fontWeight,
		m_fontStyle,
		m_fontStretch,
		m_fontSize,
		L"ja-jp",
		&m_textFormat
	);
}

// テキストレイアウトの作成
Microsoft::WRL::ComPtr<IDWriteTextLayout> D2DRenderer::CreateTextLayout(const std::wstring& text){
	D2D1_SIZE_F size = m_d2dRenderTarget->GetSize();
	Microsoft::WRL::ComPtr<IDWriteTextLayout> textLayout;
	m_graphicsContext->GetDWriteFactory()->CreateTextLayout(
		text.c_str(),
		(UINT32)text.length(),
		m_textFormat,
		size.width,
		size.height,
		&textLayout
	);
	return textLayout;
}

// テキストレイアウト描画
void D2DRenderer::DrawTextLayout(
	IDWriteTextLayout* textLayout,
	const DirectX::XMFLOAT2& pos,
	const DirectX::XMFLOAT4& color
){
	Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
	m_d2dRenderTarget->CreateSolidColorBrush(
		D2D1::ColorF(color.x, color.y, color.z, color.w),
		&brush
	);
	m_d2dRenderTarget->BeginDraw();
	m_d2dRenderTarget->DrawTextLayout(
		D2D1::Point2F(pos.x, pos.y),
		textLayout,
		brush.Get()
	);
	m_d2dRenderTarget->EndDraw();
}

// テキスト描画（レイアウト生成＋描画）
void D2DRenderer::RenderText(
	const std::wstring& text,
	const DirectX::XMFLOAT2& pos,
	const DirectX::XMFLOAT4& color
){
	auto textLayout = CreateTextLayout(text);
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
