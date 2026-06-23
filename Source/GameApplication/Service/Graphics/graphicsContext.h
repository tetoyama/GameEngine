// =======================================================================
//
// graphicsContext.h
//
// =======================================================================
#pragma once

#include <array>
#include <cstdint>
#include <wrl/client.h>
#include <d3d11.h>
#include <d2d1.h>
#include <DirectXMath.h>
#include <dwrite.h>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "Service/IService.h"
#include "GpuFrameTiming.h"
#include "Service/Graphics/RHI/RHIService.h"
#include "Shader/Common.hlsl"

#include "Backends/Effekseer/Effekseer.h"
#include "Backends/Effekseer/EffekseerRendererDX11.h"

class RenderEffectSystem;
class DebugLogService;

enum class BlendMode {
	None,
	Alpha,
	Additive,
	Subtract,
	Multiply,
	Screen,
	COUNT
};

enum class DepthMode {
	Write,
	ReadOnly,
	Disable,
	COUNT
};

enum class CullMode {
	Back,
	Front,
	None
};

class PostEffectShader {
public:
	Microsoft::WRL::ComPtr<ID3D11VertexShader> m_VS;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> m_PS;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> m_InputLayout;

	void Bind(ID3D11DeviceContext* context){
		context->VSSetShader(m_VS.Get(), nullptr, 0);
		context->PSSetShader(m_PS.Get(), nullptr, 0);
		context->IASetInputLayout(m_InputLayout.Get());
	}
};

struct PostProcessNode {
	int id;
	std::string shaderPath;
	float resolutionScale = 1.0f;
	UINT outputWidth = 0;
	UINT outputHeight = 0;
	int mipLevels = 1;
	DirectX::XMFLOAT4 param = {0,0,0,0};
	std::vector<int> inputs;
	std::unordered_map<std::string, float> parameters;
	PostEffectShader shader;
	ID3D11Texture2D* tex;
	ID3D11RenderTargetView** rtv;
	ID3D11ShaderResourceView* srv;
};

struct CameraPostEffect;

enum class PostProcessBufferID {
	BufferA,
	BufferB
};

class GraphicsContext : public IService {
public:
	explicit GraphicsContext(
		DebugLogService* debugLog = nullptr,
		RHI::RenderHardwareInterfaceService* rhiService = nullptr
	)
		: m_DebugLog(debugLog),
		  m_RHIService(rhiService)
	{}

	bool Initialize(HWND hwnd, UINT width, UINT height);
	void Shutdown() override;

	void Clear(const float clearColor[4]);
	void WaitForFrameLatency();
	void Present(bool vsync);
	bool IsFrameLatencyWaitableObjectEnabled() const noexcept {
		return m_FrameLatencyWaitableObjectEnabled;
	}
	uint64_t GetFrameLatencyWaitTimeoutCount() const noexcept {
		return m_FrameLatencyWaitTimeoutCount;
	}

	// GPU Timestamp QueryはFrame Serial付きで非同期回収する。
	// Consumeは未消費結果だけを提出順に一度だけ返す。
	void BeginGpuFrameTiming(uint64_t frameSerial);
	void EndGpuFrameTiming();
	std::vector<GpuFrameTimingResult> ConsumeResolvedGpuFrameTimings();
	bool IsTearingSupported() const noexcept { return m_TearingSupported; }

	RHI::BackendType GetBackendType() const noexcept {
		return m_RHIService
			? m_RHIService->GetSelectedBackend()
			: RHI::BackendType::Direct3D11;
	}

	RHI::RenderHardwareInterfaceService* GetRHIService() const noexcept {
		return m_RHIService;
	}

	ID3D11Device* GetDevice() const{return m_Device.Get();}
	ID3D11DeviceContext* GetDeviceContext() const{return m_DeviceContext.Get();}
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

	void ApplyPostProcessChain(
		std::vector<PostProcessNode>& effects,
		ID3D11ShaderResourceView* initialSRV,
		ID3D11ShaderResourceView* const* gbufferSRVs = nullptr,
		int gbufferCount = 0
	);

	ID3D11ShaderResourceView* GetPostProcessResultSRV() const{
		return GetCurrentSRV();
	}

	void BlitToBackBuffer(PostEffectShader* copyShader){
		m_DeviceContext->OMSetRenderTargets(1, &m_RenderTargetView, nullptr);
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
	HRESULT CreateSelectedD3D11DeviceAndSwapChain(
		IDXGIAdapter* adapter,
		D3D_DRIVER_TYPE driverType,
		HMODULE software,
		UINT flags,
		const D3D_FEATURE_LEVEL* featureLevels,
		UINT featureLevelsCount,
		UINT sdkVersion,
		const DXGI_SWAP_CHAIN_DESC* swapChainDesc,
		IDXGISwapChain** swapChain,
		ID3D11Device** device,
		D3D_FEATURE_LEVEL* featureLevel,
		ID3D11DeviceContext** immediateContext
	){
		if(GetBackendType() != RHI::BackendType::Direct3D11){
			return DXGI_ERROR_UNSUPPORTED;
		}
		return ::D3D11CreateDeviceAndSwapChain(
			adapter,
			driverType,
			software,
			flags,
			featureLevels,
			featureLevelsCount,
			sdkVersion,
			swapChainDesc,
			swapChain,
			device,
			featureLevel,
			immediateContext
		);
	}

	bool CreateDeviceAndSwapChain(HWND hwnd, UINT width, UINT height);
	bool CreateDepthStencilState();
	bool CreateSamplerState();
	bool CreateConstantBuffers();
	bool CreateRasterizerState();
	bool CreateRenderTargetView();
	bool CreateBlendState();
	bool CreateDepthStencilBufferAndView(UINT width, UINT height);
	bool CreateComputeSkinningShader();
	bool CreateD2DResources(HWND hwnd);
	bool CreateFullScreenQuad();
	bool CreateBuffer(UINT width, UINT height);
	bool CreateEffectSystem();
	bool ReadFileToBuffer(const char* fileName, std::vector<char>& buffer);

	struct GpuFrameTimingQuerySet {
		Microsoft::WRL::ComPtr<ID3D11Query> disjoint;
		Microsoft::WRL::ComPtr<ID3D11Query> beginTimestamp;
		Microsoft::WRL::ComPtr<ID3D11Query> endTimestamp;
		uint64_t submittedFrameSerial = 0;
		bool pending = false;
	};

	static constexpr size_t kGpuTimingBufferedFrames = 4;
	bool CreateGpuFrameTimingQueries();
	void ResolveGpuFrameTimingQueries();

	Microsoft::WRL::ComPtr<ID3D11Device> m_Device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_DeviceContext;
	Microsoft::WRL::ComPtr<IDXGISwapChain> m_SwapChain;
	ID3D11RenderTargetView* m_RenderTargetView = nullptr;
	ID3D11DepthStencilView* m_DepthStencilView = nullptr;

	ID3D11Buffer* m_CbPerFrame = nullptr;
	ID3D11Buffer* m_CbPerCamera = nullptr;
	ID3D11Buffer* m_CbPerObject = nullptr;
	DebugLogService* m_DebugLog = nullptr;
	RHI::RenderHardwareInterfaceService* m_RHIService = nullptr;

	CbPerFrame m_CbPerFrameData{};
	CbPerCamera m_CbPerCameraData{};
	CbPerObject m_CbPerObjectData{};

	ID3D11ComputeShader* csSkinning = nullptr;
	ID3D11DepthStencilState* m_DepthStates[(int)DepthMode::COUNT] = {};
	Microsoft::WRL::ComPtr<ID3D11BlendState> m_BlendStates[(int)BlendMode::COUNT];
	BlendMode m_CurrentBlendMode = BlendMode::COUNT;

	DXGI_FORMAT m_backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT m_depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	Microsoft::WRL::ComPtr<ID2D1Factory> m_d2dFactory;
	Microsoft::WRL::ComPtr<IDWriteFactory> m_dwriteFactory;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> m_Buffer;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_RTV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_SRV;
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_FullScreenVB;
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_FullScreenIB;

	std::array<GpuFrameTimingQuerySet, kGpuTimingBufferedFrames> m_GpuTimingQueries{};
	size_t m_GpuTimingWriteIndex = 0;
	int m_ActiveGpuTimingIndex = -1;
	bool m_GpuTimingAvailable = false;
	std::vector<GpuFrameTimingResult> m_ResolvedGpuFrameTimings;

	RenderEffectSystem* m_EffectSystem = nullptr;
	bool m_TearingSupported = false;

	// Flip-model SwapChain pacing. The actual creation flags are retained so
	// ResizeBuffers always receives exactly the same flag set.
	UINT m_SwapChainFlags = 0;
	HANDLE m_FrameLatencyWaitableObject = nullptr;
	bool m_FrameLatencyWaitableObjectEnabled = false;
	bool m_FrameLatencyWaitWarningLogged = false;
	uint64_t m_FrameLatencyWaitTimeoutCount = 0;
	static constexpr DWORD kFrameLatencyWaitTimeoutMilliseconds = 100;
};
