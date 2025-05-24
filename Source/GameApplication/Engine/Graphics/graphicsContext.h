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
	void Clear(const float clearColor[4]);
	void Present(bool vsync);

	ID3D11Device* GetDevice() const{
		return m_device.Get();
	}
	ID3D11DeviceContext* GetContext() const{
		return m_context.Get();
	}
	IDXGISwapChain* GetSwapChain() const{
		return m_swapChain.Get();
	}
	ID3D11RenderTargetView* GetRenderTargetView() const{
		return m_renderTargetView.Get();
	}
	ID3D11DepthStencilView* GetDepthStencilView() const{
		return m_depthStencilView.Get();
	}
	ID2D1Factory* GetD2DFactory() const{
		return m_d2dFactory.Get();
	}
	IDWriteFactory* GetDWriteFactory() const{
		return m_dwriteFactory.Get();
	}

	bool CreateVertexShader(
		const char* fileName,
		Microsoft::WRL::ComPtr<ID3D11VertexShader>& vertexShader,
		Microsoft::WRL::ComPtr<ID3D11InputLayout>& inputLayout
	);

	bool CreatePixelShader(
		const char* fileName,
		Microsoft::WRL::ComPtr<ID3D11PixelShader>& pixelShader);

	void Resize(UINT width, UINT height);

	Microsoft::WRL::ComPtr<ID3D11InputLayout> GetVertexLayout() const{
		return m_VertexLayout;
	}
	Microsoft::WRL::ComPtr<ID3D11PixelShader> GetPixelShader() const{
		return m_PixelShader;
	}
	Microsoft::WRL::ComPtr<ID3D11VertexShader> GetVertexShader() const{
		return m_VertexShader;
	}

    ID3D11Buffer* GetWorldConstantBuffer() {  
       return m_worldConstantBuffer.Get();  
    }

	void SetWorldMatrix(const DirectX::XMMATRIX& world);
	void SetViewMatrix(const DirectX::XMMATRIX& view);
	void SetProjectionMatrix(const DirectX::XMMATRIX& proj);
	void SetMaterial(const MATERIAL& material);
	void SetLight(const LIGHT& light);

	void SetWorldViewProjection2D(){
		SetWorldMatrix(DirectX::XMMatrixIdentity());
		SetViewMatrix(DirectX::XMMatrixIdentity());

		DirectX::XMMATRIX projection;
		projection = DirectX::XMMatrixOrthographicOffCenterLH(0.0f, 1820, 1080, 0.0f, 0.0f, 1.0f);
		SetProjectionMatrix(projection);
	}
private:
	bool CreateSwapChain(HWND hwnd, UINT width, UINT height);
	bool CreateRenderTarget();
	bool CreateDepthStencil(UINT width, UINT height);
	bool CreateD2DResources(HWND hwnd);

	bool ReadFileToBuffer(const char* fileName, std::vector<char>& buffer){
		std::ifstream file(fileName, std::ios::binary | std::ios::ate);
		if(!file) return false;
		std::streamsize size = file.tellg();
		file.seekg(0, std::ios::beg);
		buffer.resize(static_cast<size_t>(size));
		return file.read(buffer.data(), size).good();
	}

	Microsoft::WRL::ComPtr<ID3D11Device> m_device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
	Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_renderTargetView;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_depthStencilView;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> m_depthStencilBuffer;

	Microsoft::WRL::ComPtr<ID3D11VertexShader> m_VertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> m_PixelShader;

	Microsoft::WRL::ComPtr<ID3D11InputLayout> m_VertexLayout;

	Microsoft::WRL::ComPtr<ID3D11Buffer> m_worldConstantBuffer;

	DXGI_FORMAT m_backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT m_depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	Microsoft::WRL::ComPtr<ID2D1Factory> m_d2dFactory;
	Microsoft::WRL::ComPtr<IDWriteFactory> m_dwriteFactory;

	ID3D11Buffer* m_viewConstantBuffer = nullptr;
	ID3D11Buffer* m_projConstantBuffer = nullptr;
	ID3D11Buffer* m_materialConstantBuffer = nullptr;
	ID3D11Buffer* m_lightConstantBuffer = nullptr;

};
