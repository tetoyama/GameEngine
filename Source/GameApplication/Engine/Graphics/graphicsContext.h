#pragma once
#include <wrl/client.h>
#include <d3d11.h>
#include <d2d1.h>
#include <dwrite.h>
#include <string>
#include <vector>
#include <fstream>
#include "D2DRenderer.h"

class GraphicsContext {
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
		){
		std::vector<char> buffer;
		if(!ReadFileToBuffer(fileName, buffer)) return false;

		HRESULT hr = m_device->CreateVertexShader(buffer.data(), buffer.size(), nullptr, vertexShader.GetAddressOf());
		if(FAILED(hr)) return false;

		D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 4 * 3, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 4 * 6, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 4 * 10, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};
		UINT numElements = ARRAYSIZE(layout);

		hr = m_device->CreateInputLayout(
			layout, numElements,
			buffer.data(), buffer.size(),
			inputLayout.GetAddressOf());

		return SUCCEEDED(hr);
	}

	bool CreatePixelShader(
		const char* fileName,
		Microsoft::WRL::ComPtr<ID3D11PixelShader>& pixelShader){
		std::vector<char> buffer;
		if(!ReadFileToBuffer(fileName, buffer)) return false;

		HRESULT hr = m_device->CreatePixelShader(buffer.data(), buffer.size(), nullptr, pixelShader.GetAddressOf());
		return SUCCEEDED(hr);
	}
	void Resize(UINT width, UINT height);

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

	DXGI_FORMAT m_backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT m_depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	Microsoft::WRL::ComPtr<ID2D1Factory> m_d2dFactory;
	Microsoft::WRL::ComPtr<IDWriteFactory> m_dwriteFactory;
};
