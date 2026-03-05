// =======================================================================
// 
// graphicsContext.cpp
// 
// =======================================================================
#include "graphicsContext.h"


#include <stdexcept>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <windows.h> // GetDpiForWindowを使用するために必要

#include "renderEffectSystem.h"

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

#define SAFE_RELEASE(p) if(p){ p->Release(); p = nullptr;}

bool GraphicsContext::Initialize(HWND hwnd, UINT width, UINT height){

	if(!CreateDeviceAndSwapChain(hwnd, width, height)){return false;}

	if(!CreateRasterizerState()){return false;}

	if(!CreateBlendState()){return false;}

	if(!CreateDepthStencilState()){return false;}

	if(!CreateSamplerState()){return false;}

	if(!CreateConstantBuffers()){return false;}

	if(!CreateRenderTargetView()){return false;}

	if(!CreateDepthStencilBufferAndView(width, height)){return false;}

	if(!CreateD2DResources(hwnd)){return false;}

	if (!CreateComputeSkinningShader()) { return false; }

	if (!CreateEffectSystem()) { return false; }

	if(!CreateFullScreenQuad()){ return false;}

	Resize(width, height);

	m_DeviceContext->OMSetRenderTargets(1, &m_RenderTargetView, m_DepthStencilView);

	return true;
}

void GraphicsContext::Shutdown(){

	m_EffectSystem->Shutdown();
	delete m_EffectSystem;
	m_EffectSystem = nullptr;

	SAFE_RELEASE(m_CbPerFrame);
	SAFE_RELEASE(m_CbPerCamera);
	SAFE_RELEASE(m_CbPerObject);

	SAFE_RELEASE(m_RenderTargetView);
	SAFE_RELEASE(m_DepthStencilView);
	for(int i = 0; i < (int)DepthMode::COUNT; i++){
		SAFE_RELEASE(m_DepthStates[i]);
	}
	SAFE_RELEASE(csSkinning);
	
	m_DeviceContext->ClearState();  // すべてのバインドリソースを解除
	m_DeviceContext->Flush();       // GPU キューを空にする

	m_d2dFactory.Reset();
	m_dwriteFactory.Reset();

	m_DeviceContext.Reset();
	m_SwapChain.Reset();
	m_Device.Reset();


}

Effekseer::ManagerRef GraphicsContext::GetEffectManager() {
	if (m_EffectSystem) {
		return m_EffectSystem->manager;
	}
	return nullptr;
}

EffekseerRendererDX11::RendererRef GraphicsContext::GetEffectRenderer(){
	if(m_EffectSystem){
		return m_EffectSystem->renderer;
	}
	return nullptr;
}

void GraphicsContext::SetDepthMode(const DepthMode& mode){
	ID3D11DepthStencilState* state = m_DepthStates[(int)mode];
	m_DeviceContext->OMSetDepthStencilState(state, 0);
}

void GraphicsContext::SetBlendMode(const BlendMode& mode){
	if(m_CurrentBlendMode == mode){
		return;
	}
	m_CurrentBlendMode = mode;
	float blendFactor[4] = {0, 0, 0, 0};
	m_DeviceContext->OMSetBlendState(m_BlendStates[(int)mode].Get(), blendFactor, 0xffffffff);
}

// --- CbPerObject セッター ---
// 各セッターは CPU ミラーの該当フィールドを更新してから CbPerObject 全体をアップロードする。
// 同一フレーム内で複数フィールドを変更する場合は呼び出し順に注意すること。
void GraphicsContext::SetWorldMatrix(const DirectX::XMMATRIX& world){
	XMStoreFloat4x4(&m_CbPerObjectData.World, XMMatrixTranspose(world));
	m_DeviceContext->UpdateSubresource(m_CbPerObject, 0, nullptr, &m_CbPerObjectData, 0, 0);
}

void GraphicsContext::SetViewMatrix(const DirectX::XMMATRIX& view){
	XMStoreFloat4x4(&m_CbPerCameraData.View, XMMatrixTranspose(view));
	m_DeviceContext->UpdateSubresource(m_CbPerCamera, 0, nullptr, &m_CbPerCameraData, 0, 0);
}

void GraphicsContext::SetProjectionMatrix(const DirectX::XMMATRIX& proj){
	XMStoreFloat4x4(&m_CbPerCameraData.Projection, XMMatrixTranspose(proj));
	m_DeviceContext->UpdateSubresource(m_CbPerCamera, 0, nullptr, &m_CbPerCameraData, 0, 0);
}

void GraphicsContext::SetUVMatrixBuffer(const UVMatrixBuffer& uv) {
	m_CbPerObjectData.UVStart = uv.UVStart;
	m_CbPerObjectData.UVEnd   = uv.UVEnd;
	m_DeviceContext->UpdateSubresource(m_CbPerObject, 0, nullptr, &m_CbPerObjectData, 0, 0);
}

void GraphicsContext::SetMaterial(const MATERIAL& material){
	m_CbPerObjectData.Material = material;
	m_DeviceContext->UpdateSubresource(m_CbPerObject, 0, nullptr, &m_CbPerObjectData, 0, 0);
}

void GraphicsContext::SetLight(LightBuffer* light){
	m_CbPerFrameData = *light;
	m_DeviceContext->UpdateSubresource(m_CbPerFrame, 0, nullptr, &m_CbPerFrameData, 0, 0);
}

void GraphicsContext::SetCameraPosition(const float4& cameraPosition){
	m_CbPerCameraData.CameraPosition = cameraPosition;
	m_DeviceContext->UpdateSubresource(m_CbPerCamera, 0, nullptr, &m_CbPerCameraData, 0, 0);
}

void GraphicsContext::SetParameter(const float4& param){
	m_CbPerObjectData.Parameter = param;
	m_DeviceContext->UpdateSubresource(m_CbPerObject, 0, nullptr, &m_CbPerObjectData, 0, 0);
}

void GraphicsContext::SetObjectInfo(const ObjectInfo& objectInfo) {
	m_CbPerObjectData.SceneID  = objectInfo.SceneID;
	m_CbPerObjectData.ObjectID = objectInfo.ObjectID;
	m_CbPerObjectData.ShaderID = objectInfo.ShaderID;
	m_DeviceContext->UpdateSubresource(m_CbPerObject, 0, nullptr, &m_CbPerObjectData, 0, 0);
}

void GraphicsContext::ResetViewport(){
	D3D11_VIEWPORT vp{};
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	vp.Width = static_cast<float>(m_width);
	vp.Height = static_cast<float>(m_height);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	m_DeviceContext->RSSetViewports(1, &vp);
}

void GraphicsContext::SetWorldViewProjection2D(){
	SetWorldMatrix(DirectX::XMMatrixIdentity());
	SetViewMatrix(DirectX::XMMatrixIdentity());

	DirectX::XMMATRIX projection;
	projection = DirectX::XMMatrixOrthographicOffCenterLH(0.0f, (float)m_width, (float)m_height, 0, 0.0f, 1.0f);
	SetProjectionMatrix(projection);
	SetDepthMode(DepthMode::Disable);
}

bool GraphicsContext::CreateDeviceAndSwapChain(HWND hwnd, UINT width, UINT height) {
	IDXGIFactory* pFactory = nullptr;
	HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&pFactory);
	if (FAILED(hr)) {
		OutputDebugStringA("DXGIFactoryの作成に失敗しました。\n");
		return false;
	}

	IDXGIAdapter* pSelectedAdapter = nullptr;
	SIZE_T maxDedicatedVideoMemory = 0;
	UINT i = 0;
	IDXGIAdapter* pAdapter = nullptr;

	while(pFactory->EnumAdapters(i, &pAdapter) != DXGI_ERROR_NOT_FOUND){
		DXGI_ADAPTER_DESC desc;
		if(SUCCEEDED(pAdapter->GetDesc(&desc))){
			if(desc.DedicatedVideoMemory > maxDedicatedVideoMemory){
				if(pSelectedAdapter){
					pSelectedAdapter->Release();
				}
				pSelectedAdapter = pAdapter; // 新しい最大メモリアダプタ
				maxDedicatedVideoMemory = desc.DedicatedVideoMemory;
				// pAdapterはpSelectedAdapterに保持されるのでReleaseしない
			} else{
				pAdapter->Release();
			}
		} else{
			pAdapter->Release();
		}
		++i;
	}

	if(!pSelectedAdapter){
		// fallback: use default adapter
		if(SUCCEEDED(pFactory->EnumAdapters(0, &pSelectedAdapter))){
			// ok
		} else{
			OutputDebugStringA("アダプタの取得に失敗しました。\n");
			return false;
		}
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
	desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	desc.Flags = 0;

	UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
	#if defined(_DEBUG)
	flags |= D3D11_CREATE_DEVICE_DEBUG;
	#endif

	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0
	};
	D3D_FEATURE_LEVEL featureLevel;

	hr = D3D11CreateDeviceAndSwapChain(
		pSelectedAdapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags,
		featureLevels, _countof(featureLevels),
		D3D11_SDK_VERSION, &desc, m_SwapChain.GetAddressOf(),
		m_Device.GetAddressOf(), &featureLevel, m_DeviceContext.GetAddressOf()
	);

	pSelectedAdapter->Release();
	pFactory->Release();

	// ALT + Enterで排他的フルスクリーンモード切り替えを無効にする
	IDXGIFactory* pfac = nullptr;
	hr = m_SwapChain->GetParent(__uuidof(IDXGIFactory), (void**)&pfac);
	pfac->MakeWindowAssociation(hwnd, DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_ALT_ENTER);
	pfac->Release();

	if (FAILED(hr)) {
		OutputDebugStringA("スワップチェーンの作成に失敗しました。\n");
		return false;
	}
	return true;
}

bool GraphicsContext::CreateDepthStencilState(){
	HRESULT hr;
	D3D11_DEPTH_STENCIL_DESC desc{};
	desc.StencilEnable = FALSE;

	// =========================
	// Write : 不透明
	// =========================
	desc.DepthEnable = TRUE;
	desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

	hr = m_Device->CreateDepthStencilState(
		&desc,
		&m_DepthStates[(int)DepthMode::Write]
	);
	if(FAILED(hr)) return false;

	// =========================
	// ReadOnly : 半透明
	// =========================
	desc.DepthEnable = TRUE;
	desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

	hr = m_Device->CreateDepthStencilState(
		&desc,
		&m_DepthStates[(int)DepthMode::ReadOnly]
	);
	if(FAILED(hr)) return false;

	// =========================
	// Disable : UI / Fullscreen
	// =========================
	desc.DepthEnable = FALSE;
	desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = D3D11_COMPARISON_ALWAYS;

	hr = m_Device->CreateDepthStencilState(
		&desc,
		&m_DepthStates[(int)DepthMode::Disable]
	);
	if(FAILED(hr)) return false;

	return true;
}

bool GraphicsContext::CreateSamplerState(){
	// サンプラーステート設定
	D3D11_SAMPLER_DESC samplerDesc{};
	samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.MaxAnisotropy = 4;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState;
	HRESULT hr = m_Device->CreateSamplerState(&samplerDesc, &samplerState);
	m_DeviceContext->PSSetSamplers(0, 1, samplerState.GetAddressOf());
	samplerState.Reset();

	if(FAILED(hr)){
		OutputDebugStringA("サンプラーステートの作成に失敗しました。\n");
		return false;
	}

	return SUCCEEDED(hr);
}

bool GraphicsContext::CreateConstantBuffers(){
	D3D11_BUFFER_DESC bufferDesc{};
	bufferDesc.Usage          = D3D11_USAGE_DEFAULT;
	bufferDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags      = 0;

	HRESULT hr;

	// b0: CbPerFrame — フレームごとに更新 (ライト情報)
	bufferDesc.ByteWidth = sizeof(CbPerFrame);
	hr = m_Device->CreateBuffer(&bufferDesc, nullptr, &m_CbPerFrame);
	assert(SUCCEEDED(hr));
	m_DeviceContext->VSSetConstantBuffers(0, 1, &m_CbPerFrame);
	m_DeviceContext->PSSetConstantBuffers(0, 1, &m_CbPerFrame);

	// b1: CbPerCamera — カメラ/パスごとに更新 (View・Projection・CameraPosition)
	bufferDesc.ByteWidth = sizeof(CbPerCamera);
	hr = m_Device->CreateBuffer(&bufferDesc, nullptr, &m_CbPerCamera);
	assert(SUCCEEDED(hr));
	m_DeviceContext->VSSetConstantBuffers(1, 1, &m_CbPerCamera);
	m_DeviceContext->PSSetConstantBuffers(1, 1, &m_CbPerCamera);

	// b2: CbPerObject — オブジェクトごとに更新 (World・Material・UV・Parameter・ObjectInfo)
	bufferDesc.ByteWidth = sizeof(CbPerObject);
	hr = m_Device->CreateBuffer(&bufferDesc, nullptr, &m_CbPerObject);
	assert(SUCCEEDED(hr));
	m_DeviceContext->VSSetConstantBuffers(2, 1, &m_CbPerObject);
	m_DeviceContext->PSSetConstantBuffers(2, 1, &m_CbPerObject);

	// ライト初期化
	LightBuffer light{};
	light.Lights[0].Enable    = true;
	light.Lights[0].Direction = DirectX::XMFLOAT4(0.0f, -1.0f, 0.0f, 0.0f);
	light.Lights[0].Ambient   = DirectX::XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);
	light.Lights[0].Diffuse   = DirectX::XMFLOAT4(1.5f, 1.5f, 1.5f, 1.0f);
	light.Lights[0].Position  = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	SetLight(&light);

	// マテリアル初期化
	MATERIAL material{};
	material.BaseColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	SetMaterial(material);

	// UV 初期化 (フルテクスチャ範囲)
	UVMatrixBuffer uv{};
	uv.UVEnd = DirectX::XMFLOAT2(1.0f, 1.0f);
	SetUVMatrixBuffer(uv);

	// カメラ初期化
	SetCameraPosition(DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));

	return SUCCEEDED(hr);
}

bool GraphicsContext::CreateRasterizerState(){
	// ラスタライザステート設定
	D3D11_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_BACK;
	//rasterizerDesc.CullMode = D3D11_CULL_NONE;
	rasterizerDesc.DepthClipEnable = TRUE;
	rasterizerDesc.MultisampleEnable = FALSE;

	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rs;
	HRESULT hr = m_Device->CreateRasterizerState(&rasterizerDesc, &rs);

	m_DeviceContext->RSSetState(rs.Get());

	rs.Reset();

	return SUCCEEDED(hr);
}

void GraphicsContext::SetCullMode(CullMode set){
	// ラスタライザステート設定
	D3D11_RASTERIZER_DESC rasterizerDesc{};
	switch(set){
		case CullMode::Back:
			rasterizerDesc.CullMode = D3D11_CULL_BACK;
			break;
		case CullMode::Front:
			rasterizerDesc.CullMode = D3D11_CULL_FRONT;
			break;
		case CullMode::None:
			rasterizerDesc.CullMode = D3D11_CULL_NONE;
			break;
		default:
			rasterizerDesc.CullMode = D3D11_CULL_BACK;
			break;
	}
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;

	rasterizerDesc.DepthClipEnable = TRUE;
	rasterizerDesc.MultisampleEnable = FALSE;

	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rs;
	HRESULT hr = m_Device->CreateRasterizerState(&rasterizerDesc, &rs);

	m_DeviceContext->RSSetState(rs.Get());

	rs.Reset();

	assert(SUCCEEDED(hr));
}

bool GraphicsContext::CreateRenderTargetView(){
	Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
	if(FAILED(m_SwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) return false;
	HRESULT hr = (m_Device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_RenderTargetView));
	backBuffer.Reset();
	return SUCCEEDED(hr);
}

bool GraphicsContext::CreateBlendState() {
	HRESULT hr = S_OK;

	// -------------------------
	// 共通初期化
	// -------------------------
	D3D11_BLEND_DESC desc{};
	desc.AlphaToCoverageEnable = FALSE;
	desc.IndependentBlendEnable = TRUE;

	// =========================
	// None（GBuffer / Shadow）
	// =========================
	for (int i = 0; i < 8; i++) {
		auto& rt = desc.RenderTarget[i];
		rt.BlendEnable = FALSE;
		rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	}

	hr = m_Device->CreateBlendState(
		&desc,
		m_BlendStates[(int)BlendMode::None].ReleaseAndGetAddressOf()
	);
	if (FAILED(hr)) return false;

	// =========================
	// Alpha / Add / etc
	// =========================
	for (int i = 1; i < 8; i++) {
		desc.RenderTarget[i].BlendEnable = FALSE;
		desc.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	}

	auto& rt0 = desc.RenderTarget[0];

	// ---- Alpha ----
	rt0.BlendEnable = TRUE;
	rt0.SrcBlend = D3D11_BLEND_SRC_ALPHA;
	rt0.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	rt0.BlendOp = D3D11_BLEND_OP_ADD;
	rt0.SrcBlendAlpha = D3D11_BLEND_ZERO;
	rt0.DestBlendAlpha = D3D11_BLEND_ONE;
	rt0.BlendOpAlpha = D3D11_BLEND_OP_ADD;

	hr = m_Device->CreateBlendState(
		&desc,
		m_BlendStates[(int)BlendMode::Alpha].ReleaseAndGetAddressOf()
	);
	if (FAILED(hr)) return false;

	// ---- Additive ----
	rt0.SrcBlend = D3D11_BLEND_SRC_ALPHA;
	rt0.DestBlend = D3D11_BLEND_ONE;

	hr = m_Device->CreateBlendState(
		&desc,
		m_BlendStates[(int)BlendMode::Additive].ReleaseAndGetAddressOf()
	);
	if (FAILED(hr)) return false;

	// ---- Subtract ----
	rt0.BlendOp = D3D11_BLEND_OP_REV_SUBTRACT;

	hr = m_Device->CreateBlendState(
		&desc,
		m_BlendStates[(int)BlendMode::Subtract].ReleaseAndGetAddressOf()
	);
	if (FAILED(hr)) return false;

	// ---- Multiply ----
	rt0.BlendOp = D3D11_BLEND_OP_ADD;
	rt0.SrcBlend = D3D11_BLEND_DEST_COLOR;
	rt0.DestBlend = D3D11_BLEND_ZERO;

	hr = m_Device->CreateBlendState(
		&desc,
		m_BlendStates[(int)BlendMode::Multiply].ReleaseAndGetAddressOf()
	);
	if (FAILED(hr)) return false;

	// ---- Screen ----
	rt0.SrcBlend = D3D11_BLEND_ONE;
	rt0.DestBlend = D3D11_BLEND_INV_DEST_COLOR;

	hr = m_Device->CreateBlendState(
		&desc,
		m_BlendStates[(int)BlendMode::Screen].ReleaseAndGetAddressOf()
	);
	if (FAILED(hr)) return false;

	// 初期状態
	SetBlendMode(BlendMode::None);

	return true;
}

bool GraphicsContext::CreateDepthStencilBufferAndView(UINT width, UINT height){

	// デプスステンシルバッファ作成
	ID3D11Texture2D* depthStencile{};
	D3D11_TEXTURE2D_DESC textureDesc{};
	textureDesc.Width = (UINT)width;
	textureDesc.Height = (UINT)height;
	textureDesc.MipLevels = 1;
	textureDesc.ArraySize = 1;
	textureDesc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;
	textureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	textureDesc.CPUAccessFlags = 0;
	textureDesc.MiscFlags = 0;
	HRESULT hr = m_Device->CreateTexture2D(&textureDesc, NULL, &depthStencile);
	if(FAILED(hr)){
		return false;
	}

	// デプスステンシルビュー作成
	D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc{};
	depthStencilViewDesc.Format = textureDesc.Format;
	depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depthStencilViewDesc.Flags = 0;
	if(depthStencile){
		hr = m_Device->CreateDepthStencilView(depthStencile, &depthStencilViewDesc, &m_DepthStencilView);
		depthStencile->Release();

		if(FAILED(hr)){
			return false;
		}
		m_DeviceContext->OMSetRenderTargets(1, &m_RenderTargetView, m_DepthStencilView);

	} else{
		return false;
	}
	return true;
}

bool GraphicsContext::CreateComputeSkinningShader() {
	ID3DBlob* csBlob = nullptr;
	HRESULT hr = D3DReadFileToBlob(L"Asset\\Shader\\SkinningCS.cso", &csBlob);
	if (FAILED(hr)) {
		OutputDebugStringA("Failed to load SkinningCS.cso\n");
		return false;
	}

	hr = m_Device->CreateComputeShader(csBlob->GetBufferPointer(), csBlob->GetBufferSize(), nullptr, &csSkinning);
	csBlob->Release();
	if (FAILED(hr)) {
		OutputDebugStringA("Failed to create compute shader\n");
		return false;
	}

	return true;
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

bool GraphicsContext::CreateFullScreenQuad(){
	VERTEX_3D fullscreenVertices[] = {
	{ {-1,-1,0}, {0,0,0}, {0,0,0}, {1,1,1,1}, {0,1} },
	{ {-1, 3,0}, {0,0,0}, {0,0,0}, {1,1,1,1}, {0,-1} },
	{ { 3,-1,0}, {0,0,0}, {0,0,0}, {1,1,1,1}, {2,1} },
	};
	UINT fullscreenIndices[] = {0,1,2};

	// VB
	D3D11_BUFFER_DESC vbDesc = {};
	vbDesc.Usage = D3D11_USAGE_DEFAULT;
	vbDesc.ByteWidth = sizeof(fullscreenVertices);
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA vbData = {fullscreenVertices};
	if(FAILED(m_Device->CreateBuffer(&vbDesc, &vbData, &m_FullScreenVB))) return false;

	// IB
	D3D11_BUFFER_DESC ibDesc = {};
	ibDesc.Usage = D3D11_USAGE_DEFAULT;
	ibDesc.ByteWidth = sizeof(fullscreenIndices);
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	D3D11_SUBRESOURCE_DATA ibData = {fullscreenIndices};
	if(FAILED(m_Device->CreateBuffer(&ibDesc, &ibData, &m_FullScreenIB))) return false;

	return true;
}

bool GraphicsContext::CreateEffectSystem() {

	m_EffectSystem = new RenderEffectSystem();
	
	bool result = m_EffectSystem->Initialize(m_Device.Get(), m_DeviceContext.Get());

	return result;
}

bool GraphicsContext::ReadFileToBuffer(const char* fileName, std::vector<char>& buffer){
	std::ifstream file(fileName, std::ios::binary | std::ios::ate);
	if(!file) return false;
	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);
	buffer.resize(static_cast<size_t>(size));
	return file.read(buffer.data(), size).good();
}

void GraphicsContext::Clear(const float clearColor[4]){

	m_DeviceContext->ClearRenderTargetView(m_RenderTargetView, clearColor);
	m_DeviceContext->ClearDepthStencilView(m_DepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	m_DeviceContext->OMSetRenderTargets(1, &m_RenderTargetView, m_DepthStencilView);
}

void GraphicsContext::Present(bool vsync){
	m_SwapChain->Present(vsync, 0);
}

bool GraphicsContext::CreateVertexShader(const char* fileName, ID3D11VertexShader** vertexShader, ID3D11InputLayout** inputLayout){
	std::vector<char> buffer;
	if(!ReadFileToBuffer(fileName, buffer)) return false;

	HRESULT hr = m_Device->CreateVertexShader(buffer.data(), buffer.size(), nullptr, vertexShader);
	if(FAILED(hr)) return false;

	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",		0, DXGI_FORMAT_R32G32B32_FLOAT,		0, 4 * 3, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TANGENT",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0, 4 * 6, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",		0, DXGI_FORMAT_R32G32B32A32_FLOAT,	0, 4 * 9, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD",	0, DXGI_FORMAT_R32G32_FLOAT,		0, 4 * 13, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	UINT numElements = ARRAYSIZE(layout);

	hr = m_Device->CreateInputLayout(
		layout, numElements,
		buffer.data(), buffer.size(),
		inputLayout);

	return SUCCEEDED(hr);
}

bool GraphicsContext::CreatePixelShader(const char* fileName,ID3D11PixelShader** pixelShader){
	std::vector<char> buffer;
	if(!ReadFileToBuffer(fileName, buffer)) return false;

	HRESULT hr = m_Device->CreatePixelShader(buffer.data(), buffer.size(), nullptr, pixelShader);
	return SUCCEEDED(hr);
}

void GraphicsContext::Resize(UINT width, UINT height){

	if(m_SwapChain){
		m_DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
		if(m_RenderTargetView){
			m_RenderTargetView->Release();
			m_RenderTargetView = nullptr;
		}
		if(m_DepthStencilView){
			m_DepthStencilView->Release();
			m_DepthStencilView = nullptr;
		}
		
		HRESULT hr = m_SwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
		if(FAILED(hr)){
			OutputDebugStringA("スワップチェーンのリサイズに失敗しました。\n");
			return;
		}
		if(!CreateRenderTargetView()){
			OutputDebugStringA("スワップチェーンのリサイズに失敗しました。\n");
			return;
		}
		if(!CreateDepthStencilBufferAndView(width, height)){
			OutputDebugStringA("スワップチェーンのリサイズに失敗しました。\n");
			return;
		}

		CreateBuffer(width, height);

		m_DeviceContext->OMSetRenderTargets(1, &m_RenderTargetView, m_DepthStencilView);

		m_width = width;
		m_height = height;

		// ビューポートの設定
		ResetViewport();
	}
}

void GraphicsContext::ResetBuffer(const float clearColor[4]){
	m_DeviceContext->ClearRenderTargetView(m_RTV.Get(), clearColor);
	m_DeviceContext->ClearDepthStencilView(m_DepthStencilView,
										   D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
										   1.0f, 0);
	m_DeviceContext->OMSetRenderTargets(1, m_RTV.GetAddressOf(), m_DepthStencilView);
}

void GraphicsContext::ApplyPostProcessChain(std::vector<PostProcessNode>& effects, ID3D11ShaderResourceView* initialSRV,
                                             ID3D11ShaderResourceView* const* gbufferSRVs, int gbufferCount){
	for(auto& node : effects){

		ID3D11ShaderResourceView* nullSRV[TextureSlot_Max] = {nullptr};
		m_DeviceContext->PSSetShaderResources(0, 8, nullSRV); // まず全解除

		m_DeviceContext->OMSetRenderTargets(1, node.rtv, nullptr);

		// ダウンサンプリング: レンダーターゲットのサイズに合わせてビューポートを設定
		if(node.tex){
			D3D11_TEXTURE2D_DESC desc;
			node.tex->GetDesc(&desc);
			D3D11_VIEWPORT vp{};
			vp.TopLeftX = 0.0f;
			vp.TopLeftY = 0.0f;
			vp.Width    = static_cast<float>(desc.Width);
			vp.Height   = static_cast<float>(desc.Height);
			vp.MinDepth = 0.0f;
			vp.MaxDepth = 1.0f;
			m_DeviceContext->RSSetViewports(1, &vp);
		}

		for(size_t i = 0; i < node.inputs.size(); ++i){
			ID3D11ShaderResourceView* inputSRV = nullptr;
			if(node.inputs[i] == -2){
				inputSRV = initialSRV;
			} else if(node.inputs[i] >= 0 && node.inputs[i] < static_cast<int>(effects.size())){
				inputSRV = effects[node.inputs[i]].srv;
			}
			m_DeviceContext->PSSetShaderResources(static_cast<UINT>(i), 1, &inputSRV);
		}

		// GBuffer を固定スロット (PostEffectGBufferSlot_Start 以降) にバインド
		if(gbufferSRVs && gbufferCount > 0){
			for(int g = 0; g < gbufferCount; ++g){
				m_DeviceContext->PSSetShaderResources(PostEffectGBufferSlot_Start + g, 1, &gbufferSRVs[g]);
			}
		}

		SetParameter(node.param);

		DrawQuad(&node.shader, nullptr); // SRV はすでに PSSetShaderResources でセット済み
	}

	// GBuffer スロットを解除
	if(gbufferSRVs && gbufferCount > 0){
		ID3D11ShaderResourceView* nullGBuf[PostEffectGBufferSlot_Count] = {nullptr};
		int unbindCount = (gbufferCount < PostEffectGBufferSlot_Count) ? gbufferCount : PostEffectGBufferSlot_Count;
		m_DeviceContext->PSSetShaderResources(PostEffectGBufferSlot_Start, unbindCount, nullGBuf);
	}

	if(!effects.empty()){
		m_CurrentSRV = effects.back().srv;
		m_CurrentRTV = effects.back().rtv;
	}

	// ポストプロセス後にビューポートをフルスクリーンに戻す
	ResetViewport();
}





bool GraphicsContext::CreateBuffer(UINT width, UINT height){
	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	m_Device->CreateTexture2D(&desc, nullptr, &m_Buffer);
	m_Device->CreateRenderTargetView(m_Buffer.Get(), nullptr, &m_RTV);
	m_Device->CreateShaderResourceView(m_Buffer.Get(), nullptr, &m_SRV);

	return true;
}

ID3D11ShaderResourceView* GraphicsContext::GetCurrentSRV() const{
	return m_SRV.Get();
}

void GraphicsContext::DrawQuad(PostEffectShader* shader, ID3D11ShaderResourceView* inputSRV){
	auto* context = m_DeviceContext.Get();

	shader->Bind(context);

	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	context->IASetVertexBuffers(0, 1, m_FullScreenVB.GetAddressOf(), &stride, &offset);
	context->IASetIndexBuffer(m_FullScreenIB.Get(), DXGI_FORMAT_R32_UINT, 0);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	if (inputSRV){
	context->PSSetShaderResources(0, 1, &inputSRV);
	}
	context->DrawIndexed(3, 0, 0);

	ID3D11ShaderResourceView* nullSRV[1] = {nullptr};
	context->PSSetShaderResources(0, 1, nullSRV);
}


void GraphicsContext::DrawQuad() {
	auto* context = m_DeviceContext.Get();

	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	context->IASetVertexBuffers(0, 1, m_FullScreenVB.GetAddressOf(), &stride, &offset);
	context->IASetIndexBuffer(m_FullScreenIB.Get(), DXGI_FORMAT_R32_UINT, 0);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	context->DrawIndexed(3, 0, 0);
}
