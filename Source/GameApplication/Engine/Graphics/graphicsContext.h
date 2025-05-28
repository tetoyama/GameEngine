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


struct VERTEX_3D
{
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT4 Diffuse;
	DirectX::XMFLOAT2 TexCoord;
};

struct MATERIAL
{
	DirectX::XMFLOAT4	Ambient;
	DirectX::XMFLOAT4	Diffuse;
	DirectX::XMFLOAT4	Specular;
	DirectX::XMFLOAT4	Emission;
	float		Shininess;
	BOOL		TextureEnable;
	float		Dummy[2];
};

struct LIGHT
{
	BOOL		Enable;
	BOOL		Dummy[3];
	DirectX::XMFLOAT4	Direction;
	DirectX::XMFLOAT4	Diffuse;
	DirectX::XMFLOAT4	Ambient;
};


class GraphicsContext : public IService {
public:
	bool Initialize(HWND hwnd, UINT width, UINT height);
	void Shutdown() override;
	void Clear(const float clearColor[4]);
	void Present(bool vsync);

	ID3D11Device* GetDevice() const{return m_Device.Get();}
	ID3D11DeviceContext* GetContext() const{return m_DeviceContext.Get();}
	IDXGISwapChain* GetSwapChain() const{return m_SwapChain.Get();}

	ID3D11RenderTargetView* GetRenderTargetView() const{return m_RenderTargetView;}
	ID3D11DepthStencilView* GetDepthStencilView() const{return m_DepthStencilView;}

	ID2D1Factory* GetD2DFactory() const{return m_d2dFactory.Get();}
	IDWriteFactory* GetDWriteFactory() const{return m_dwriteFactory.Get();}

	ID3D11InputLayout* GetVertexLayout() const{return m_VertexLayout;}
	ID3D11PixelShader* GetPixelShader() const{return m_PixelShader;}
	ID3D11VertexShader* GetVertexShader() const{return m_VertexShader;}

    ID3D11Buffer* GetWorldConstantBuffer() {return m_WorldBuffer;}

	// セッター
	void SetDepthEnable(const bool& Enable);
	void SetATCEnable(const bool& Enable);
	void SetWorldMatrix(const DirectX::XMMATRIX& WorldMatrix);
	void SetViewMatrix(const DirectX::XMMATRIX& ViewMatrix);
	void SetProjectionMatrix(const DirectX::XMMATRIX& ProjectionMatrix);
	void SetMaterial(const MATERIAL& Material);
	void SetLight(const LIGHT& Light);

	void SetWorldViewProjection2D();

	void Resize(UINT width, UINT height);

	bool CreateVertexShader(
		const char* fileName,
		ID3D11VertexShader* vertexShader,
		ID3D11InputLayout* inputLayout
	);

	bool CreatePixelShader(
		const char* fileName,
		ID3D11PixelShader* pixelShader
	);

private:
	bool CreateDeviceAndSwapChain(HWND hwnd, UINT width, UINT height);
	bool CreateDepthStencilstate();
	bool CreateSamplerstate();
	bool CreateConstantBuffers();
	bool CreateRasterizerState();
	bool CreateRenderTargetView();
	bool CreateBlendState();
	bool CreateDepthStencilBufferAndView(UINT width, UINT height);
	bool CreateD2DResources(HWND hwnd);

	bool ReadFileToBuffer(const char* fileName, std::vector<char>& buffer);

	Microsoft::WRL::ComPtr<ID3D11Device>			m_Device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext>		m_DeviceContext;
	Microsoft::WRL::ComPtr<IDXGISwapChain>			m_SwapChain;
	ID3D11RenderTargetView*							m_RenderTargetView;
	ID3D11DepthStencilView*							m_DepthStencilView;

	ID3D11Buffer*			m_WorldBuffer = nullptr;
	ID3D11Buffer*			m_ViewBuffer = nullptr;
	ID3D11Buffer*			m_ProjectionBuffer = nullptr;
	ID3D11Buffer*			m_MaterialBuffer = nullptr;
	ID3D11Buffer*			m_LightBuffer = nullptr;

	ID3D11DepthStencilState*	m_DepthStateEnable;
	ID3D11DepthStencilState*	m_DepthStateDisable;

	ID3D11BlendState*			m_BlendState;
	ID3D11BlendState*			m_BlendStateATC;

	ID3D11VertexShader*			m_VertexShader;
	ID3D11PixelShader*			m_PixelShader;

	ID3D11InputLayout*		m_VertexLayout;


	DXGI_FORMAT m_backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT m_depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	Microsoft::WRL::ComPtr<ID2D1Factory> m_d2dFactory;
	Microsoft::WRL::ComPtr<IDWriteFactory> m_dwriteFactory;



};
