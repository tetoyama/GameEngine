#define _CRT_SECURE_NO_WARNINGS

#include "renderer.h"

//#include "../Platform/WindowSystem/window.h"
//#include "../../Services/system.h"
//#include "../../../Debug_/debugPrintf.h"

#include <io.h>

#pragma warning(push)			//警告の抑制の範囲の開始地点
#pragma warning(disable:4005)	//マクロの再定義の警告をなくす？
// Direct3Dの型・クラス・関数などを呼べるようにする
#include <d3dcompiler.h>
#define DIRECTINPUT_VERSION 0x0800
#include "dinput.h"
#include "mmsystem.h"
#pragma warning(pop)			//警告の抑制の範囲の終了地点

//テクスチャサポートライブラリ
//#include "../../../Library/DirectX11/DirectXTex.h"

//リンカーを使ってライブラリのリンクをしている
// 2D
#pragma comment(lib,"d2d1.lib")
#pragma comment(lib,"dwrite.lib")
// 3D
#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "d3dcompiler.lib")

// 固有IDライブラリ
#pragma comment (lib, "dxguid.lib")
// DirectInput
#pragma comment (lib, "dinput8.lib")

#define SAFE_RELEASE(p) if(p){ p->Release(); p=NULL; }

//#define ACPECTRATIO_MIN (16.0f / 9.0f)
#define ACPECTRATIO_MIN (4.0f / 3.0f)
//#define ACPECTRATIO_MAX (16.0f / 9.0f)
#define ACPECTRATIO_MAX (21.0f / 9.0f)

D3D_FEATURE_LEVEL		Renderer::m_FeatureLevel = D3D_FEATURE_LEVEL_11_0;

ID3D11Device* Renderer::m_Device{};
ID3D11DeviceContext* Renderer::m_DeviceContext{};
IDXGISwapChain* Renderer::m_SwapChain{};
ID3D11RenderTargetView* Renderer::m_RenderTargetView{};
ID3D11DepthStencilView* Renderer::m_DepthStencilView{};

ID3D11Buffer* Renderer::m_WorldBuffer{};
ID3D11Buffer* Renderer::m_ViewBuffer{};
ID3D11Buffer* Renderer::m_ProjectionBuffer{};
ID3D11Buffer* Renderer::m_MaterialBuffer{};
ID3D11Buffer* Renderer::m_LightBuffer{};


ID3D11DepthStencilState* Renderer::m_DepthStateEnable{};
ID3D11DepthStencilState* Renderer::m_DepthStateDisable{};


ID3D11BlendState* Renderer::m_BlendState{};
ID3D11BlendState* Renderer::m_BlendStateATC{};

ID2D1Factory* Renderer::m_pD2DFactory{};
IDWriteFactory* Renderer::m_pDWriteFactory{};
IDWriteTextFormat* Renderer::m_pTextFormat{};
ID2D1RenderTarget* Renderer::m_pRT{};
ID2D1SolidColorBrush* Renderer::m_pSolidBrush{};
IDXGISurface* Renderer::m_pBackBuffer{};

std::wstring			Renderer::m_FontName{};
std::wstring			Renderer::m_FontLocate{};
float					Renderer::m_FontSize{};
DWRITE_FONT_WEIGHT		Renderer::m_FontWeight{};
DWRITE_FONT_STYLE		Renderer::m_FontStyle{};
DWRITE_FONT_STRETCH		Renderer::m_FontStretch{};

XMFLOAT2				Renderer::m_ViewPortSize{};

void Renderer::Init() {
	HRESULT hr = S_OK;

	m_FontName = L"メイリオ";
	m_FontLocate = L"";
	m_FontSize = 20.0f;
	m_FontWeight = DWRITE_FONT_WEIGHT_NORMAL;
	m_FontStretch = DWRITE_FONT_STRETCH_NORMAL;
	m_FontStyle = DWRITE_FONT_STYLE_NORMAL;

	// デバイス、スワップチェーン作成
	DXGI_SWAP_CHAIN_DESC swapChainDesc{};
	swapChainDesc.BufferCount = 1;
	swapChainDesc.BufferDesc.Width = 1920;
	swapChainDesc.BufferDesc.Height = 1080;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	//swapChainDesc.OutputWindow = MainWindow::GetWindow();
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.Windowed = TRUE;

	hr = D3D11CreateDeviceAndSwapChain(NULL,
									   D3D_DRIVER_TYPE_HARDWARE,
									   NULL,
									   D3D11_CREATE_DEVICE_BGRA_SUPPORT,
									   NULL,
									   0,
									   D3D11_SDK_VERSION,
									   &swapChainDesc,
									   &m_SwapChain,
									   &m_Device,
									   &m_FeatureLevel,
									   &m_DeviceContext);
	assert(SUCCEEDED(hr));

	// ラスタライザステート設定
	D3D11_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_BACK;
	rasterizerDesc.DepthClipEnable = TRUE;
	rasterizerDesc.MultisampleEnable = FALSE;

	ID3D11RasterizerState* rs;
	hr = m_Device->CreateRasterizerState(&rasterizerDesc, &rs);

	m_DeviceContext->RSSetState(rs);




	// ブレンドステート設定
	D3D11_BLEND_DESC blendDesc{};
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	hr = m_Device->CreateBlendState(&blendDesc, &m_BlendState);
	assert(SUCCEEDED(hr));

	blendDesc.AlphaToCoverageEnable = TRUE;
	hr = m_Device->CreateBlendState(&blendDesc, &m_BlendStateATC);
	assert(SUCCEEDED(hr));

	float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	m_DeviceContext->OMSetBlendState(m_BlendState, blendFactor, 0xffffffff);





	// デプスステンシルステート設定
	D3D11_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	depthStencilDesc.StencilEnable = FALSE;

	hr = m_Device->CreateDepthStencilState(&depthStencilDesc, &m_DepthStateEnable);//深度有効ステート
	assert(SUCCEEDED(hr));

	//depthStencilDesc.DepthEnable = FALSE;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	hr = m_Device->CreateDepthStencilState(&depthStencilDesc, &m_DepthStateDisable);//深度無効ステート
	assert(SUCCEEDED(hr));

	m_DeviceContext->OMSetDepthStencilState(m_DepthStateEnable, NULL);




	// サンプラーステート設定
	D3D11_SAMPLER_DESC samplerDesc{};
	samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.MaxAnisotropy = 4;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	ID3D11SamplerState* samplerState{};
	hr = m_Device->CreateSamplerState(&samplerDesc, &samplerState);
	assert(SUCCEEDED(hr));

	m_DeviceContext->PSSetSamplers(0, 1, &samplerState);



	// 定数バッファ生成
	D3D11_BUFFER_DESC bufferDesc{};
	bufferDesc.ByteWidth = sizeof(XMFLOAT4X4);
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	bufferDesc.StructureByteStride = sizeof(float);

	hr = m_Device->CreateBuffer(&bufferDesc, NULL, &m_WorldBuffer);
	m_DeviceContext->VSSetConstantBuffers(0, 1, &m_WorldBuffer);
	assert(SUCCEEDED(hr));

	hr = m_Device->CreateBuffer(&bufferDesc, NULL, &m_ViewBuffer);
	m_DeviceContext->VSSetConstantBuffers(1, 1, &m_ViewBuffer);
	assert(SUCCEEDED(hr));

	hr = m_Device->CreateBuffer(&bufferDesc, NULL, &m_ProjectionBuffer);
	m_DeviceContext->VSSetConstantBuffers(2, 1, &m_ProjectionBuffer);
	assert(SUCCEEDED(hr));


	bufferDesc.ByteWidth = sizeof(MATERIAL);

	hr = m_Device->CreateBuffer(&bufferDesc, NULL, &m_MaterialBuffer);
	m_DeviceContext->VSSetConstantBuffers(3, 1, &m_MaterialBuffer);
	m_DeviceContext->PSSetConstantBuffers(3, 1, &m_MaterialBuffer);
	assert(SUCCEEDED(hr));


	bufferDesc.ByteWidth = sizeof(LIGHT);

	hr = m_Device->CreateBuffer(&bufferDesc, NULL, &m_LightBuffer);
	m_DeviceContext->VSSetConstantBuffers(4, 1, &m_LightBuffer);
	m_DeviceContext->PSSetConstantBuffers(4, 1, &m_LightBuffer);
	assert(SUCCEEDED(hr));





	// ライト初期化
	LIGHT light{};
	light.Enable = true;
	light.Direction = XMFLOAT4(0.0f, -1.0f, 0.0f, 0.0f);
	light.Ambient = XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);
	light.Diffuse = XMFLOAT4(1.5f, 1.5f, 1.5f, 1.0f);
	SetLight(light);

	// マテリアル初期化
	MATERIAL material{};
	material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.Ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	SetMaterial(material);

	CreateNewViewPort(XMFLOAT2(1920, 1080), XMFLOAT2(1920, 1080));

}

void Renderer::Uninit() {
	m_WorldBuffer->Release();
	m_ViewBuffer->Release();
	m_ProjectionBuffer->Release();
	m_LightBuffer->Release();
	m_MaterialBuffer->Release();

	m_DeviceContext->ClearState();
	m_RenderTargetView->Release();
	m_SwapChain->Release();
	m_DeviceContext->Release();
	m_Device->Release();
}

void Renderer::Begin() {
	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	m_DeviceContext->ClearRenderTargetView(m_RenderTargetView, clearColor);
	m_DeviceContext->ClearDepthStencilView(m_DepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void Renderer::End() {
	//if (System::GetSetting().vSinc) {
	//	m_SwapChain->Present(1, 0);
	//} else {
		m_SwapChain->Present(0, 0);
	//}
}

void Renderer::CreateNewViewPort(XMFLOAT2 SetSize, XMFLOAT2 WindowSize) {

	//DebugPrintf("Resize(%f,%f)\n %.1ff : 9.0f\n\n", SetSize.x, SetSize.y, 9.0f * SetSize.x / SetSize.y);
	m_ViewPortSize = SetSize;

	HRESULT hr;

	// レンダーターゲットビュー作成
	ID3D11Texture2D* renderTarget{};
	m_SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&renderTarget);
	if (renderTarget) {
		hr = m_Device->CreateRenderTargetView(renderTarget, NULL, &m_RenderTargetView);
		renderTarget->Release();
	}
	// デプスステンシルバッファ作成
	ID3D11Texture2D* depthStencile{};
	D3D11_TEXTURE2D_DESC textureDesc{};
	textureDesc.Width = (UINT)WindowSize.x;
	textureDesc.Height = (UINT)WindowSize.y;
	textureDesc.MipLevels = 1;
	textureDesc.ArraySize = 1;
	textureDesc.Format = DXGI_FORMAT_D16_UNORM;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;
	textureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	textureDesc.CPUAccessFlags = 0;
	textureDesc.MiscFlags = 0;
	hr = m_Device->CreateTexture2D(&textureDesc, NULL, &depthStencile);

	// デプスステンシルビュー作成
	D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc{};
	depthStencilViewDesc.Format = textureDesc.Format;
	depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depthStencilViewDesc.Flags = 0;
	if (depthStencile) {
		hr = m_Device->CreateDepthStencilView(depthStencile, &depthStencilViewDesc, &m_DepthStencilView);
		depthStencile->Release();
	}

	m_DeviceContext->OMSetRenderTargets(1, &m_RenderTargetView, m_DepthStencilView);

	//------------------------------------------------
	// Direct2D,DirectWriteの初期化
	//------------------------------------------------
	hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);

	hr = m_SwapChain->GetBuffer(0, IID_PPV_ARGS(&m_pBackBuffer));
	//hr = (ID3D11Texture2D)->QueryInterface(IID_PPV_ARGS(&m_pBackBuffer));
	FLOAT dpiX;
	FLOAT dpiY;

	#pragma warning(suppress : 4996)	//警告解除 "非推奨" とされた関数、クラス メンバー、変数、または typedef がコードで使用されています。
	m_pD2DFactory->GetDesktopDpi(&dpiX, &dpiY);

	D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED), dpiX, dpiY);

	hr = m_pD2DFactory->CreateDxgiSurfaceRenderTarget(m_pBackBuffer, &props, &m_pRT);

	hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&m_pDWriteFactory));

	ReloadTextFormat();

	//関数SetTextAlignment()
	//第1引数：テキストの配置（DWRITE_TEXT_ALIGNMENT_LEADING：前, DWRITE_TEXT_ALIGNMENT_TRAILING：後, DWRITE_TEXT_ALIGNMENT_CENTER：中央,
	//                         DWRITE_TEXT_ALIGNMENT_JUSTIFIED：行いっぱい）
	hr = m_pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);

	//関数CreateSolidColorBrush()
	//第1引数：フォント色（D2D1::ColorF(D2D1::ColorF::Black)：黒, D2D1::ColorF(D2D1::ColorF(0.0f, 0.2f, 0.9f, 1.0f))：RGBA指定）
	//hr = m_pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &m_pSolidBrush);

	XMFLOAT2 Start = XMFLOAT2((WindowSize.x - SetSize.x) * 0.5f, (WindowSize.y - SetSize.y) * 0.5f);
	// ビューポート設定
	D3D11_VIEWPORT viewport{};
	viewport.Width = (FLOAT)SetSize.x;
	viewport.Height = (FLOAT)SetSize.y;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = (FLOAT)Start.x;
	viewport.TopLeftY = (FLOAT)Start.y;
	m_DeviceContext->RSSetViewports(1, &viewport);
}

void Renderer::ResizeRenderer(XMFLOAT2 SetSize) {

	XMFLOAT2 WindowSize = SetSize;
	float Ratio = SetSize.x / SetSize.y;
	float RatioMin = ACPECTRATIO_MIN;
	float RatioMax = ACPECTRATIO_MAX;

	if (ACPECTRATIO_MAX < Ratio) {
		SetSize.x = SetSize.y * ACPECTRATIO_MAX;
	}
	if (ACPECTRATIO_MIN > Ratio) {
		SetSize.y = SetSize.x / ACPECTRATIO_MIN;
	}

	if (m_SwapChain) {

		SAFE_RELEASE(m_RenderTargetView)
			SAFE_RELEASE(m_DepthStencilView)

			SAFE_RELEASE(m_pBackBuffer)
			SAFE_RELEASE(m_pSolidBrush)
			SAFE_RELEASE(m_pTextFormat)
			SAFE_RELEASE(m_pDWriteFactory)
			SAFE_RELEASE(m_pD2DFactory)
			SAFE_RELEASE(m_pRT)

			HRESULT hr = m_SwapChain->ResizeBuffers(
				0, (UINT)WindowSize.x, (UINT)WindowSize.y, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);

		CreateNewViewPort(SetSize, WindowSize);

	}
}

void Renderer::SetDepthEnable(bool Enable) {
	if (Enable) {
		m_DeviceContext->OMSetDepthStencilState(m_DepthStateEnable, NULL);
	} else {
		m_DeviceContext->OMSetDepthStencilState(m_DepthStateDisable, NULL);
	}
}

void Renderer::SetATCEnable(bool Enable) {
	float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	if (Enable) {
		m_DeviceContext->OMSetBlendState(m_BlendStateATC, blendFactor, 0xffffffff);
	} else {
		m_DeviceContext->OMSetBlendState(m_BlendState, blendFactor, 0xffffffff);
	}

}

void Renderer::SetWorldViewProjection2D() {
	SetWorldMatrix(XMMatrixIdentity());
	SetViewMatrix(XMMatrixIdentity());

	XMMATRIX projection;
	projection = XMMatrixOrthographicOffCenterLH(0.0f, 1920, 1080, 0.0f, 0.0f, 1.0f);
	SetProjectionMatrix(projection);
}

void Renderer::SetWorldMatrix(XMMATRIX WorldMatrix) {
	XMFLOAT4X4 worldf;
	XMStoreFloat4x4(&worldf, XMMatrixTranspose(WorldMatrix));
	m_DeviceContext->UpdateSubresource(m_WorldBuffer, 0, NULL, &worldf, 0, 0);
}

void Renderer::SetViewMatrix(XMMATRIX ViewMatrix) {
	XMFLOAT4X4 viewf;
	XMStoreFloat4x4(&viewf, XMMatrixTranspose(ViewMatrix));
	m_DeviceContext->UpdateSubresource(m_ViewBuffer, 0, NULL, &viewf, 0, 0);
}

void Renderer::SetProjectionMatrix(XMMATRIX ProjectionMatrix) {
	XMFLOAT4X4 projectionf;
	XMStoreFloat4x4(&projectionf, XMMatrixTranspose(ProjectionMatrix));
	m_DeviceContext->UpdateSubresource(m_ProjectionBuffer, 0, NULL, &projectionf, 0, 0);

}

void Renderer::SetMaterial(MATERIAL Material) {
	m_DeviceContext->UpdateSubresource(m_MaterialBuffer, 0, NULL, &Material, 0, 0);
}

void Renderer::SetLight(LIGHT Light) {
	m_DeviceContext->UpdateSubresource(m_LightBuffer, 0, NULL, &Light, 0, 0);
}

IDWriteTextLayout* Renderer::CreateTextLayout(WCHAR* pWCText) {

	D2D1_SIZE_F oTargetSize = m_pRT->GetSize();

	//	テキストレイアウトを作成
	IDWriteTextLayout* pTextLayout = nullptr;
	// IDWriteTextLayoutの取得
	m_pDWriteFactory->CreateTextLayout(
		pWCText,					// 文字列
		(UINT32)wcslen(pWCText),	// 表示する文字数
		m_pTextFormat,				// DWriteTextFormat
		oTargetSize.width,			// 枠の幅
		oTargetSize.height,			// 枠の高さ
		&pTextLayout
	);

	return pTextLayout;
}

void Renderer::DrawTextLayout(IDWriteTextLayout* pTextLayout, XMFLOAT2 StartPos, XMFLOAT4 Color) {

	if (m_pSolidBrush) {
		m_pSolidBrush->Release();
		m_pSolidBrush = nullptr;
	}

	m_pRT->CreateSolidColorBrush(D2D1::ColorF(Color.x, Color.y, Color.z, Color.w), &m_pSolidBrush);

	//	表示する個所を指定
	D2D1_POINT_2F points{};
	points.x = StartPos.x;
	points.y = StartPos.y;

	if (m_pSolidBrush) {
		//	テキストを表示
		m_pRT->BeginDraw();
		m_pRT->DrawTextLayout(points, pTextLayout, m_pSolidBrush);
		m_pRT->EndDraw();

		m_pSolidBrush->Release();
		m_pSolidBrush = nullptr;
	}
}

void Renderer::RenderText(WCHAR* pWCText, XMFLOAT2 StartPos, XMFLOAT4 Color) {

	if (m_pSolidBrush) {
		m_pSolidBrush->Release();
		m_pSolidBrush = nullptr;
	}

	D2D1_SIZE_F oTargetSize = m_pRT->GetSize();

	//	テキストレイアウトを作成
	IDWriteTextLayout* pTextLayout = nullptr;
	// IDWriteTextLayoutの取得
	m_pDWriteFactory->CreateTextLayout(
		pWCText,						// 文字列
		(UINT32)wcslen(pWCText),	// 表示する文字数
		m_pTextFormat,				// DWriteTextFormat
		oTargetSize.width,			// 枠の幅
		oTargetSize.height,			// 枠の高さ
		&pTextLayout
	);

	m_pRT->CreateSolidColorBrush(D2D1::ColorF(Color.x, Color.y, Color.z, Color.w), &m_pSolidBrush);

	//	表示する個所を指定
	D2D1_POINT_2F points{};
	points.x = StartPos.x;
	points.y = StartPos.y;

	if (pTextLayout && m_pSolidBrush) {
		//	テキストを表示
		m_pRT->BeginDraw();
		m_pRT->DrawTextLayout(points, pTextLayout, m_pSolidBrush);
		m_pRT->EndDraw();
	}

	//	テキストレイアウトの破棄
	(pTextLayout)->Release();

	if (m_pSolidBrush) {
		m_pSolidBrush->Release();
		m_pSolidBrush = nullptr;
	}
}

void Renderer::SetTextSize(float Size, bool Reload) {
	m_FontSize = Size;

	if (Reload) {
		ReloadTextFormat();
	}
}

void Renderer::SetTextFont(std::wstring SetName, bool Reload) {
	m_FontName = SetName;

	if (Reload) {
		ReloadTextFormat();
	}
}

void Renderer::SetTextWeight(DWRITE_FONT_WEIGHT SetWeight, bool Reload) {

	m_FontWeight = SetWeight;

	if (Reload) {
		ReloadTextFormat();
	}
}

void Renderer::SetTextStyle(DWRITE_FONT_STYLE SetStyle, bool Reload) {

	m_FontStyle = SetStyle;

	if (Reload) {
		ReloadTextFormat();
	}
}

void Renderer::SetTextStretch(DWRITE_FONT_STRETCH SetStretch, bool Reload) {

	m_FontStretch = SetStretch;

	if (Reload) {
		ReloadTextFormat();
	}
}

void Renderer::ReloadTextFormat() {

	if (m_pTextFormat) {
		m_pTextFormat->Release();
		m_pTextFormat = nullptr;
	}

	m_pDWriteFactory->CreateTextFormat(
		m_FontName.c_str(),
		nullptr,
		m_FontWeight,
		m_FontStyle,
		m_FontStretch,
		(FLOAT)m_FontSize,
		L"",
		&m_pTextFormat
	);
}

void Renderer::CreateVertexShader(ID3D11VertexShader** VertexShader, ID3D11InputLayout** VertexLayout, const char* FileName) {

	FILE* file;
	long int fsize;

	file = fopen(FileName, "rb");
	assert(file);

	fsize = _filelength(_fileno(file));
	unsigned char* buffer = new unsigned char[fsize];
	fread(buffer, fsize, 1, file);
	fclose(file);

	m_Device->CreateVertexShader(buffer, fsize, NULL, VertexShader);


	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 4 * 3, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 4 * 6, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 4 * 10, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	UINT numElements = ARRAYSIZE(layout);

	m_Device->CreateInputLayout(layout,
								numElements,
								buffer,
								fsize,
								VertexLayout);

	delete[] buffer;
}

void Renderer::CreatePixelShader(ID3D11PixelShader** PixelShader, const char* FileName) {
	FILE* file;
	long int fsize;

	file = fopen(FileName, "rb");
	assert(file);

	fsize = _filelength(_fileno(file));
	unsigned char* buffer = new unsigned char[fsize];
	fread(buffer, fsize, 1, file);
	fclose(file);

	m_Device->CreatePixelShader(buffer, fsize, NULL, PixelShader);

	delete[] buffer;
}
