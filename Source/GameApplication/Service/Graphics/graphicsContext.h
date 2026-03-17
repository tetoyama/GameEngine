// =======================================================================
// 
// graphicsContext.h
// 
// =======================================================================
#pragma once
#include <wrl/client.h>
#include <d3d11.h>
#include <d2d1.h>
#include <DirectXMath.h>
#include <dwrite.h>
#include <string>
#include <vector>
#include <fstream>

#include "Service/IService.h"
#include "Shader/Common.hlsl"

#include "Backends/Effekseer/Effekseer.h"
#include "Backends/Effekseer/EffekseerRendererDX11.h"

class RenderEffectSystem;
class DebugLogService;

enum class BlendMode
{
	None,		// ブレンド無し（GBuffer / Shadow）
	Alpha,		// 通常アルファブレンド
	Additive,	// 加算
	Subtract,	// 減算
	Multiply,	// 乗算
	Screen,		// スクリーン
	COUNT		// ブレンドモードの数
};

enum class DepthMode {
	Write,		// 不透明（深度書き込みあり・テストあり）
	ReadOnly,	// 半透明（深度書き込みなし・テストあり）
	Disable,	// UI 等（深度無効）
	COUNT		
};

enum class CullMode
{
	Back,   // 背面カリング（通常）
	Front,  // 前面カリング（シャドウマップ描画等）
	None    // カリングなし（両面描画）
};

// ポストエフェクト描画に使用するシェーダーセット（VS + PS + InputLayout）
class PostEffectShader {
public:
	Microsoft::WRL::ComPtr<ID3D11VertexShader> m_VS;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>  m_PS;
	Microsoft::WRL::ComPtr<ID3D11InputLayout>  m_InputLayout;

	// シェーダーと入力レイアウトをデバイスコンテキストにバインドする
	void Bind(ID3D11DeviceContext* context){
		context->VSSetShader(m_VS.Get(), nullptr, 0);
		context->PSSetShader(m_PS.Get(), nullptr, 0);
		context->IASetInputLayout(m_InputLayout.Get());
	}
};

// ポストプロセスパイプラインの 1 ノードを表す構造体（現在は未使用）
struct PostProcessNode {
	int id;
	std::string shaderPath;
	float resolutionScale = 1.0f;
	UINT outputWidth = 0;
	UINT outputHeight = 0;

	DirectX::XMFLOAT4 param = {0,0,0,0};

	std::vector<int> inputs; // 接続元ノードID
	std::unordered_map<std::string, float> parameters;

	// 実行リソース
	PostEffectShader shader;
	ID3D11Texture2D* tex;
	ID3D11RenderTargetView** rtv;
	ID3D11ShaderResourceView* srv;
};

struct CameraPostEffect;

// ポストプロセスバッファの識別子（ピンポンバッファ方式で 2 つを交互に使用）
enum class PostProcessBufferID {
	BufferA,
	BufferB
};

// DirectX 11 のデバイス・デバイスコンテキスト・スワップチェーンを管理する描画サービス
// ブレンドモード・デプスモード・カリングモードのステート管理と
// レンダーターゲット・デプスバッファの管理を担当する
class GraphicsContext : public IService {
public:
	explicit GraphicsContext(DebugLogService* debugLog = nullptr)
		: m_DebugLog(debugLog)
	{}

	// DirectX 11 デバイスとスワップチェーンを初期化する
	bool Initialize(HWND hwnd, UINT width, UINT height);

	// グラフィクスリソースを全て解放する
	void Shutdown() override;

	// バックバッファとデプスバッファを指定色でクリアする
	void Clear(const float clearColor[4]);

	// スワップチェーンに Present を呼び出してフレームを表示する
	void Present(bool vsync);

	// DirectX 11 デバイスを返す（バッファ・テクスチャ・シェーダーの生成に使用）
	ID3D11Device* GetDevice() const{return m_Device.Get();}

	// DirectX 11 デバイスコンテキストを返す（描画コマンドの発行に使用）
	ID3D11DeviceContext* GetDeviceContext() const{return m_DeviceContext.Get();}

	// DXGI スワップチェーンを返す
	IDXGISwapChain* GetSwapChain() const{return m_SwapChain.Get();}

	ID3D11RenderTargetView* GetRenderTargetView() {return m_RenderTargetView;}
	ID3D11RenderTargetView** GetpRenderTargetView(){return &m_RenderTargetView;}

	ID3D11ShaderResourceView* GetRenderTargetSRV() { return m_SRV.Get(); }

	ID3D11DepthStencilView* GetDepthStencilView() {return m_DepthStencilView;}

	ID2D1Factory* GetD2DFactory() const{return m_d2dFactory.Get();}
	IDWriteFactory* GetDWriteFactory() const{return m_dwriteFactory.Get();}

    ID3D11Buffer* GetWorldConstantBuffer() { return m_CbPerObject; }

	Effekseer::ManagerRef GetEffectManager();
	EffekseerRendererDX11::RendererRef GetEffectRenderer();

	LightBuffer* GetLight() { return &m_CbPerFrameData; }

	// セッター
	void SetDepthMode(const DepthMode& mode);
	void SetBlendMode(const BlendMode& mode);
	void SetWorldMatrix(const DirectX::XMMATRIX& WorldMatrix);
	void SetViewMatrix(const DirectX::XMMATRIX& ViewMatrix);
	void SetProjectionMatrix(const DirectX::XMMATRIX& ProjectionMatrix);
	void SetUVMatrixBuffer(const UVMatrixBuffer& uv);
	void SetMaterial(const MATERIAL& Material);
	void SetLight(LightBuffer* Light);
	void SetCameraPosition(const float4& cameraPosition);
	void SetParameter(const float4& param);
	void SetObjectInfo(const ObjectInfo& ObjectInfo);

	void ResetViewport();
	void ResetBuffer(const float clearColor[4]);
	void SetWorldViewProjection2D();
	void SetCullMode(CullMode set);

	void Resize(UINT width, UINT height);

	bool CreateVertexShader(
		const char* fileName, 
		ID3D11VertexShader** vertexShader, 
		ID3D11InputLayout** inputLayout
	);

	bool CreatePixelShader(
		const char* fileName,
		ID3D11PixelShader** pixelShader
	);

	ID3D11ComputeShader* GetSkinningShader() {
		return csSkinning;
	}
	ID3D11ComputeShader* GetParticleUpdateShader() {
		return csParticleUpdate.Get();
	}
	// 描画
	void ApplyPostProcessChain(std::vector<PostProcessNode>& effects,
	                           ID3D11ShaderResourceView* initialSRV,
	                           ID3D11ShaderResourceView* const* gbufferSRVs = nullptr,
	                           int gbufferCount = 0);

	ID3D11ShaderResourceView* GetPostProcessResultSRV() const{
		return GetCurrentSRV();
	}
	void BlitToBackBuffer(PostEffectShader* copyShader){
		// バックバッファを描画先に設定
		m_DeviceContext->OMSetRenderTargets(1, &m_RenderTargetView, nullptr);

		// 現在の SRV をフルスクリーン描画
		ID3D11ShaderResourceView* inputSRV = GetCurrentSRV();
		DrawQuad(copyShader, inputSRV);
	}
	void DrawQuad(PostEffectShader* shader, ID3D11ShaderResourceView* inputSRV);
	void DrawQuad();
	ID3D11ShaderResourceView* GetCurrentSRV() const;

	UINT m_width = 0;
	UINT m_height = 0;
	ID3D11ShaderResourceView* m_CurrentSRV = nullptr;
	ID3D11RenderTargetView** m_CurrentRTV = nullptr;

private:
	bool CreateDeviceAndSwapChain(HWND hwnd, UINT width, UINT height);
	bool CreateDepthStencilState();
	bool CreateSamplerState();
	bool CreateConstantBuffers();
	bool CreateRasterizerState();
	bool CreateRenderTargetView();
	bool CreateBlendState();
	bool CreateDepthStencilBufferAndView(UINT width, UINT height);
	bool CreateComputeSkinningShader();
	bool CreateComputeParticleShader();
	bool CreateD2DResources(HWND hwnd);
	bool CreateFullScreenQuad();
	bool CreateBuffer(UINT width, UINT height);

	bool CreateEffectSystem();

	bool ReadFileToBuffer(const char* fileName, std::vector<char>& buffer);

	Microsoft::WRL::ComPtr<ID3D11Device>			m_Device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext>		m_DeviceContext;
	Microsoft::WRL::ComPtr<IDXGISwapChain>			m_SwapChain;
	ID3D11RenderTargetView*							m_RenderTargetView;
	ID3D11DepthStencilView*							m_DepthStencilView;

	ID3D11Buffer* m_CbPerFrame  = nullptr;   // b0: フレームごと (ライト)
	ID3D11Buffer* m_CbPerCamera = nullptr;   // b1: カメラ/パスごと (View, Proj, CamPos)
	ID3D11Buffer* m_CbPerObject = nullptr;   // b2: オブジェクトごと (World, Material, UV, Param, ObjInfo)
	DebugLogService* m_DebugLog = nullptr;

	CbPerFrame  m_CbPerFrameData{};
	CbPerCamera m_CbPerCameraData{};
	CbPerObject m_CbPerObjectData{};

	ID3D11ComputeShader* csSkinning = nullptr;
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> csParticleUpdate;

	ID3D11DepthStencilState*	m_DepthStates[(int)DepthMode::COUNT];

	Microsoft::WRL::ComPtr<ID3D11BlendState> m_BlendStates[(int)BlendMode::COUNT];
	BlendMode m_CurrentBlendMode = BlendMode::COUNT;

	DXGI_FORMAT m_backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT m_depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	Microsoft::WRL::ComPtr<ID2D1Factory> m_d2dFactory;
	Microsoft::WRL::ComPtr<IDWriteFactory> m_dwriteFactory;

	Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_Buffer;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   m_RTV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_SRV;

	Microsoft::WRL::ComPtr<ID3D11Buffer> m_FullScreenVB;
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_FullScreenIB;

	RenderEffectSystem* m_EffectSystem = nullptr;
};
