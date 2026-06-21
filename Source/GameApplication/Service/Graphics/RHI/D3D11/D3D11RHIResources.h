#pragma once

#include <cstddef>
#include <vector>
#include <d3d11.h>
#include <wrl/client.h>

#include "Service/Graphics/RHI/RHIDescriptors.h"

namespace RHI {

struct D3D11BufferResource {
	BufferDesc desc;
	ResourceState state = ResourceState::Common;
	Microsoft::WRL::ComPtr<ID3D11Buffer> object;
};

struct D3D11TextureResource {
	TextureDesc desc;
	std::vector<ResourceState> subresourceStates;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> object;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderView;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderView;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthView;
};

struct D3D11ShaderResource {
	ShaderDesc desc;
	std::vector<std::byte> byteCode;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel;
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> compute;
};

struct D3D11PipelineStateResource {
	PipelineStateDesc desc;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthStencil;
	Microsoft::WRL::ComPtr<ID3D11BlendState> blend;
};

} // namespace RHI
