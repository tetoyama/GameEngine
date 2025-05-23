#include "graphicsContext.h"


#include <stdexcept>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <windows.h> // GetDpiForWindowを使用するために必要

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
#include "Backends/DirectX11/DirectXTex.h"

//リンカーを使ってライブラリのリンクをしている
// 2D
#pragma comment(lib,"d2d1.lib")
#pragma comment(lib,"dwrite.lib")
// 3D
#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "d3dcompiler.lib")

// 固有IDライブラリ
#pragma comment (lib, "dxguid.lib")
#pragma comment (lib, "dxgi.lib")
// DirectInput
#pragma comment (lib, "dinput8.lib")

bool GraphicsContext::Initialize(HWND hwnd, UINT width, UINT height){
	if(!CreateSwapChain(hwnd, width, height)){
		return false;
	}
	if(!CreateRenderTarget()){
		return false;
	}
	if(!CreateDepthStencil(width, height)){
		return false;
	}
	if(!CreateD2DResources(hwnd)){
		return false;
	}
	m_context->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), m_depthStencilView.Get());

	CreateVertexShader("shader\\unlitTextureVS.cso", m_VertexShader, m_VertexLayout);
	CreatePixelShader("shader\\unlitTexturePS.cso", m_PixelShader);

	return true;
}

bool GraphicsContext::CreateSwapChain(HWND hwnd, UINT width, UINT height){

	IDXGIFactory* pFactory = nullptr;
	CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&pFactory);

	IDXGIAdapter* pSelectedAdapter = nullptr;
	UINT i = 0;
	IDXGIAdapter* pAdapter = nullptr;

	while (pFactory->EnumAdapters(i, &pAdapter) != DXGI_ERROR_NOT_FOUND) {
		DXGI_ADAPTER_DESC desc;
		pAdapter->GetDesc(&desc);

		std::wstring descStr(desc.Description);
		if (descStr.find(L"NVIDIA") != std::wstring::npos) {
			pSelectedAdapter = pAdapter; // Use this adapter
			break;
		}

		pAdapter->Release();
		++i;
	}

	if (!pSelectedAdapter) {
		// fallback: use default adapter
		pFactory->EnumAdapters(0, &pSelectedAdapter);
	}

	DXGI_SWAP_CHAIN_DESC desc = {};
	desc.BufferCount = 1;
	desc.BufferDesc.Width = width;
	desc.BufferDesc.Height = height;
	desc.BufferDesc.Format = m_backBufferFormat;
	desc.BufferDesc.RefreshRate.Numerator = 60;
	desc.BufferDesc.RefreshRate.Denominator = 1;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.OutputWindow = hwnd;
	desc.SampleDesc.Count = 1;
	desc.Windowed = TRUE;

	UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
	flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_FEATURE_LEVEL featureLevel;
	HRESULT hr = D3D11CreateDeviceAndSwapChain(
		pSelectedAdapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags, nullptr, 0,
		D3D11_SDK_VERSION, &desc, m_swapChain.GetAddressOf(),
		m_device.GetAddressOf(), &featureLevel, m_context.GetAddressOf()
	);
	if (FAILED(hr)) {
		OutputDebugStringA("スワップチェーンの作成に失敗しました。\n");
		return false;
	}

	if (pSelectedAdapter) {
		pSelectedAdapter->Release();
	}
	return SUCCEEDED(hr);
}

bool GraphicsContext::CreateRenderTarget(){
	Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
	if(FAILED(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) return false;
	return SUCCEEDED(m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, m_renderTargetView.ReleaseAndGetAddressOf()));
}

bool GraphicsContext::CreateDepthStencil(UINT width, UINT height){
	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = m_depthStencilFormat;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	if(FAILED(m_device->CreateTexture2D(&desc, nullptr, m_depthStencilBuffer.ReleaseAndGetAddressOf())))
		return false;
	return SUCCEEDED(m_device->CreateDepthStencilView(m_depthStencilBuffer.Get(), nullptr, m_depthStencilView.ReleaseAndGetAddressOf()));
}

bool GraphicsContext::CreateD2DResources(HWND hwnd){
	// D2Dファクトリ生成（初回のみ）
	if(!m_d2dFactory){
		HRESULT hr = D2D1CreateFactory(
			D2D1_FACTORY_TYPE_SINGLE_THREADED,
			IID_PPV_ARGS(&m_d2dFactory)
		);
		if(FAILED(hr)) return false;
	}

	// DWriteファクトリ生成（初回のみ）
	if(!m_dwriteFactory){
		HRESULT hr = DWriteCreateFactory(
			DWRITE_FACTORY_TYPE_SHARED,
			__uuidof(IDWriteFactory),
			reinterpret_cast<IUnknown**>(m_dwriteFactory.GetAddressOf())
		);
		if(FAILED(hr)) return false;
	}

	return true;
}

void GraphicsContext::Clear(const float clearColor[4]){

	m_context->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);
	m_context->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	m_context->IASetInputLayout(m_VertexLayout.Get());
	m_context->VSSetShader(m_VertexShader.Get(), NULL, 0);
	m_context->PSSetShader(m_PixelShader.Get(), NULL, 0);
}

void GraphicsContext::Present(bool vsync){
	m_swapChain->Present(vsync, 0);
}

void GraphicsContext::Resize(UINT width, UINT height){
	if(m_swapChain){
		m_context->OMSetRenderTargets(0, nullptr, nullptr);
		m_renderTargetView.Reset();
		m_depthStencilView.Reset();

		HRESULT hr = m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
		if(FAILED(hr)){
			OutputDebugStringA("スワップチェーンのリサイズに失敗しました。\n");
			return;
		}
		CreateRenderTarget();
		CreateDepthStencil(width, height);

		m_context->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), m_depthStencilView.Get());

	}
}
