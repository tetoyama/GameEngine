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
#include "Shader/CommonBuffer.h"

#include "Backends/Effekseer/Effekseer.h"
#include "Backends/Effekseer/EffekseerRendererDX11.h"

class RenderEffectSystem;

enum class BlendMode
{
	Alpha,		// 通常アルファブレンド
	Additive,	// 加算
	Subtract,	// 減算
	Multiply,	// 乗算
	Screen,		// スクリーン
	COUNT		// ブレンドモードの数
};

enum class CullMode
{
	Back,	
	Front,
	None
};
class PostEffectShader {
public:
	Microsoft::WRL::ComPtr<ID3D11VertexShader> m_VS;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>  m_PS;
	Microsoft::WRL::ComPtr<ID3D11InputLayout>  m_InputLayout;

	void Bind(ID3D11DeviceContext* context){
		context->VSSetShader(m_VS.Get(), nullptr, 0);
		context->PSSetShader(m_PS.Get(), nullptr, 0);
		context->IASetInputLayout(m_InputLayout.Get());
	}
};
enum class PostProcessBufferID {
	BufferA,
	BufferB
};
class GraphicsContext : public IService {
public:
	bool Initialize(HWND hwnd, UINT width, UINT height);
	void Shutdown() override;
	void Clear(const float clearColor[4]);
	void Present(bool vsync);

	ID3D11Device* GetDevice() const{return m_Device.Get();}
	ID3D11DeviceContext* GetDeviceContext() const{return m_DeviceContext.Get();}
	IDXGISwapChain* GetSwapChain() const{return m_SwapChain.Get();}

	ID3D11RenderTargetView* GetRenderTargetView() {return m_RenderTargetView;}
	ID3D11RenderTargetView** GetpRenderTargetView(){return &m_RenderTargetView;}

	ID3D11DepthStencilView* GetDepthStencilView() {return m_DepthStencilView;}

	ID2D1Factory* GetD2DFactory() const{return m_d2dFactory.Get();}
	IDWriteFactory* GetDWriteFactory() const{return m_dwriteFactory.Get();}

    ID3D11Buffer* GetWorldConstantBuffer() {return m_WorldBuffer;}

	Effekseer::ManagerRef GetEffectManager();
	EffekseerRendererDX11::RendererRef GetEffectRenderer();
	// セッター
	void SetDepthEnable(const bool& Enable);
	void SetBlendMode(const BlendMode& mode);
	void SetWorldMatrix(const DirectX::XMMATRIX& WorldMatrix);
	void SetViewMatrix(const DirectX::XMMATRIX& ViewMatrix);
	void SetProjectionMatrix(const DirectX::XMMATRIX& ProjectionMatrix);
	void SetUVMatrix(const UVMatrix& uv);
	void SetMaterial(const MATERIAL& Material);
	void SetLight(const LIGHT& Light);
	void SetCamera(const CAMERA& Camera);
	void SetParameter(const Parameter& Parameter);
	void ResetViewport();
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
		return m_pComputeSkinningShader;
	}
	// 描画
	void ResetPingPongBuffer(const float clearColor[4]);
	void ApplyPostProcessChain(
		const std::vector<PostEffectShader>& effects){
		for(size_t i = 0; i < effects.size(); i++){
			PostEffectShader shader = effects[i];

			// 入力は現在の SRV
			ID3D11ShaderResourceView* inputSRV = GetCurrentSRV();

			// 出力先はもう一方のバッファ
			m_CurrentBuffer = (m_CurrentBuffer == PostProcessBufferID::BufferA)
				? PostProcessBufferID::BufferB
				: PostProcessBufferID::BufferA;
			SwitchRenderTarget(m_CurrentBuffer);

			// クアッドを描画してシェーダーを適用
			DrawQuad(&shader, inputSRV);
		}
	}
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
	void SwitchRenderTarget(PostProcessBufferID id);
	ID3D11ShaderResourceView* GetCurrentSRV() const;

	UINT m_width = 0;
	UINT m_height = 0;
private:
	bool CreateDeviceAndSwapChain(HWND hwnd, UINT width, UINT height);
	bool CreateDepthStencilstate();
	bool CreateSamplerstate();
	bool CreateConstantBuffers();
	bool CreateRasterizerState();
	bool CreateRenderTargetView();
	bool CreateBlendState();
	bool CreateDepthStencilBufferAndView(UINT width, UINT height);
	bool CreateComputeSkinningShader();
	bool CreateD2DResources(HWND hwnd);
	bool CreateFullScreenQuad();
	bool CreatePingPongBuffers(UINT width, UINT height);

	bool CreateEffectSystem();

	bool ReadFileToBuffer(const char* fileName, std::vector<char>& buffer);

	Microsoft::WRL::ComPtr<ID3D11Device>			m_Device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext>		m_DeviceContext;
	Microsoft::WRL::ComPtr<IDXGISwapChain>			m_SwapChain;
	ID3D11RenderTargetView*							m_RenderTargetView;
	ID3D11DepthStencilView*							m_DepthStencilView;

	ID3D11Buffer* m_WorldBuffer = nullptr;
	ID3D11Buffer* m_ViewBuffer = nullptr;
	ID3D11Buffer* m_ProjectionBuffer = nullptr;
	ID3D11Buffer* m_UVMatrixBuffer = nullptr;
	ID3D11Buffer* m_MaterialBuffer = nullptr;
	ID3D11Buffer* m_LightBuffer = nullptr;
	ID3D11Buffer* m_CameraBuffer = nullptr;
	ID3D11Buffer* m_ParameterBuffer = nullptr;

	ID3D11ComputeShader* m_pComputeSkinningShader = nullptr;

	ID3D11DepthStencilState*	m_DepthStateEnable;
	ID3D11DepthStencilState*	m_DepthStateDisable;

	Microsoft::WRL::ComPtr<ID3D11BlendState> m_BlendStates[(int)BlendMode::COUNT];
	BlendMode m_CurrentBlendMode = BlendMode::COUNT;

	DXGI_FORMAT m_backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT m_depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	Microsoft::WRL::ComPtr<ID2D1Factory> m_d2dFactory;
	Microsoft::WRL::ComPtr<IDWriteFactory> m_dwriteFactory;

	// PostProcess用バッファ
	Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_PostBufferA;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   m_PostRTV_A;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_PostSRV_A;

	Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_PostBufferB;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   m_PostRTV_B;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_PostSRV_B;

	Microsoft::WRL::ComPtr<ID3D11Buffer> m_FullScreenVB;
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_FullScreenIB;

	PostProcessBufferID m_CurrentBuffer = PostProcessBufferID::BufferA;

	RenderEffectSystem* m_EffectSystem = nullptr;
};
