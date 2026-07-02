#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include "Engine/Scene/System/Render/StaticBatch/StaticBatchDrawSubmission.h"
#include "Engine/Scene/System/Render/StaticBatch/StaticBatchShadowPipelineResources.h"
#include "Engine/Scene/System/Render/StaticBatch/StaticBatchShadowPixelState.h"
#include "Service/Graphics/RHI/D3D11/D3D11RHIDevice.h"
#include "Shader/common.hlsl"

namespace {

using Microsoft::WRL::ComPtr;

template<typename T, std::size_t N>
std::span<const std::byte> AsBytes(const std::array<T, N>& values){
	return std::as_bytes(std::span<const T>(values));
}

ComPtr<ID3DBlob> CompileShader(
	const char* source,
	const char* target
){
	ComPtr<ID3DBlob> byteCode;
	ComPtr<ID3DBlob> errors;
	const HRESULT result = D3DCompile(
		source,
		std::char_traits<char>::length(source),
		nullptr,
		nullptr,
		nullptr,
		"main",
		target,
		D3DCOMPILE_ENABLE_STRICTNESS,
		0,
		&byteCode,
		&errors
	);
	assert(SUCCEEDED(result));
	assert(byteCode);
	return byteCode;
}

std::span<const std::byte> BlobBytes(ID3DBlob& blob){
	return {
		reinterpret_cast<const std::byte*>(blob.GetBufferPointer()),
		blob.GetBufferSize()
	};
}

ComPtr<ID3D11ShaderResourceView> CreateColorTexture(
	ID3D11Device& device,
	std::uint32_t pixel
){
	D3D11_TEXTURE2D_DESC textureDesc{};
	textureDesc.Width = 1;
	textureDesc.Height = 1;
	textureDesc.MipLevels = 1;
	textureDesc.ArraySize = 1;
	textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.Usage = D3D11_USAGE_IMMUTABLE;
	textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA initialData{};
	initialData.pSysMem = &pixel;
	initialData.SysMemPitch = sizeof(pixel);

	ComPtr<ID3D11Texture2D> texture;
	assert(SUCCEEDED(device.CreateTexture2D(
		&textureDesc,
		&initialData,
		&texture
	)));

	ComPtr<ID3D11ShaderResourceView> view;
	assert(SUCCEEDED(device.CreateShaderResourceView(
		texture.Get(),
		nullptr,
		&view
	)));
	return view;
}

} // namespace

int main(){
	ComPtr<ID3D11Device> nativeDevice;
	ComPtr<ID3D11DeviceContext> nativeContext;
	D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
	const D3D_FEATURE_LEVEL requestedLevels[] = {
		D3D_FEATURE_LEVEL_11_0
	};
	const HRESULT createResult = D3D11CreateDevice(
		nullptr,
		D3D_DRIVER_TYPE_WARP,
		nullptr,
		0,
		requestedLevels,
		1,
		D3D11_SDK_VERSION,
		&nativeDevice,
		&featureLevel,
		&nativeContext
	);
	assert(SUCCEEDED(createResult));
	assert(nativeDevice);
	assert(nativeContext);
	assert(featureLevel == D3D_FEATURE_LEVEL_11_0);

	D3D11_SAMPLER_DESC samplerDesc{};
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	ComPtr<ID3D11SamplerState> materialSampler;
	assert(SUCCEEDED(nativeDevice->CreateSamplerState(
		&samplerDesc,
		&materialSampler
	)));

	ComPtr<ID3D11ShaderResourceView> originalTexture =
		CreateColorTexture(*nativeDevice.Get(), 0xff00ffffu);
	ComPtr<ID3D11ShaderResourceView> alphaTexture =
		CreateColorTexture(*nativeDevice.Get(), 0x0000ffffu);
	ID3D11ShaderResourceView* originalTexturePtr = originalTexture.Get();
	ID3D11SamplerState* materialSamplerPtr = materialSampler.Get();
	nativeContext->PSSetShaderResources(0, 1, &originalTexturePtr);
	nativeContext->PSSetSamplers(0, 1, &materialSamplerPtr);

	{
		StaticBatchShadowPixelState pixelState(nativeContext.Get(), 0);
		assert(pixelState.CanSampleDiffuseTexture());
		assert(pixelState.BindDiffuseTexture(alphaTexture.Get()));

		ComPtr<ID3D11ShaderResourceView> boundTexture;
		ComPtr<ID3D11SamplerState> boundSampler;
		nativeContext->PSGetShaderResources(
			0,
			1,
			boundTexture.ReleaseAndGetAddressOf()
		);
		nativeContext->PSGetSamplers(
			0,
			1,
			boundSampler.ReleaseAndGetAddressOf()
		);
		assert(boundTexture.Get() == alphaTexture.Get());
		assert(boundSampler.Get() == materialSampler.Get());
	}

	ComPtr<ID3D11ShaderResourceView> restoredTexture;
	ComPtr<ID3D11SamplerState> restoredSampler;
	nativeContext->PSGetShaderResources(
		0,
		1,
		restoredTexture.ReleaseAndGetAddressOf()
	);
	nativeContext->PSGetSamplers(
		0,
		1,
		restoredSampler.ReleaseAndGetAddressOf()
	);
	assert(restoredTexture.Get() == originalTexture.Get());
	assert(restoredSampler.Get() == materialSampler.Get());

	ID3D11SamplerState* nullSampler = nullptr;
	nativeContext->PSSetSamplers(0, 1, &nullSampler);
	{
		StaticBatchShadowPixelState pixelState(nativeContext.Get(), 0);
		assert(!pixelState.CanSampleDiffuseTexture());
	}
	nativeContext->PSSetSamplers(0, 1, &materialSamplerPtr);

	RHI::SwapChainDesc swapChainDesc;
	swapChainDesc.width = 64;
	swapChainDesc.height = 64;
	RHI::D3D11RHIDevice device(
		nativeDevice.Get(),
		nativeContext.Get(),
		nullptr,
		swapChainDesc
	);

	static constexpr const char* VertexShaderSource = R"(
struct VSInput {
	float3 position : POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float4 color : COLOR;
	float2 uv : TEXCOORD;
	float4 world0 : INSTANCEWORLD0;
	float4 world1 : INSTANCEWORLD1;
	float4 world2 : INSTANCEWORLD2;
	float4 world3 : INSTANCEWORLD3;
	uint4 objectInfo : INSTANCEOBJECT0;
};

float4 main(VSInput input) : SV_POSITION {
	float4x4 world = float4x4(
		input.world0,
		input.world1,
		input.world2,
		input.world3
	);
	return mul(float4(input.position, 1.0f), world);
}
)";

	static constexpr const char* PixelShaderSource = R"(
float4 main() : SV_TARGET {
	return float4(1.0f, 1.0f, 1.0f, 1.0f);
}
)";

	ComPtr<ID3DBlob> vertexByteCode =
		CompileShader(VertexShaderSource, "vs_5_0");
	ComPtr<ID3DBlob> pixelByteCode =
		CompileShader(PixelShaderSource, "ps_5_0");

	RHI::ShaderDesc vertexShaderDesc;
	vertexShaderDesc.stage = RHI::ShaderStage::Vertex;
	vertexShaderDesc.entryPoint = "main";
	vertexShaderDesc.debugName = "Static Shadow Interop VS";
	const RHI::ShaderHandle vertexShader = device.CreateShader(
		vertexShaderDesc,
		BlobBytes(*vertexByteCode.Get())
	);
	assert(vertexShader);

	StaticBatchShadowPipelineResources shadowPipeline;
	assert(shadowPipeline.Create(
		device,
		vertexShader,
		BlobBytes(*pixelByteCode.Get())
	));
	assert(shadowPipeline.IsReady());

	std::array<VERTEX_3D, 3> vertices{};
	vertices[0].Position = DirectX::XMFLOAT3(-0.5f, -0.5f, 0.5f);
	vertices[1].Position = DirectX::XMFLOAT3(0.0f, 0.5f, 0.5f);
	vertices[2].Position = DirectX::XMFLOAT3(0.5f, -0.5f, 0.5f);
	for(VERTEX_3D& vertex : vertices){
		vertex.Normal = DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f);
		vertex.Tangent = DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f);
		vertex.Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex.TexCoord = DirectX::XMFLOAT2(0.0f, 0.0f);
	}
	constexpr std::array<std::uint32_t, 3> indices{0, 1, 2};

	std::array<StaticBatchInstanceData, 2> instances{};
	for(StaticBatchInstanceData& instance : instances){
		instance.worldMatrix[0] = 1.0f;
		instance.worldMatrix[5] = 1.0f;
		instance.worldMatrix[10] = 1.0f;
		instance.worldMatrix[15] = 1.0f;
	}
	instances[0].entityIndex = 1;
	instances[1].worldMatrix[12] = 0.1f;
	instances[1].entityIndex = 2;

	RHI::BufferDesc vertexBufferDesc;
	vertexBufferDesc.byteSize = static_cast<std::uint32_t>(sizeof(vertices));
	vertexBufferDesc.stride = static_cast<std::uint32_t>(sizeof(VERTEX_3D));
	vertexBufferDesc.usage = RHI::ResourceUsage::Immutable;
	vertexBufferDesc.bindFlags = RHI::BufferBindFlags::Vertex;
	vertexBufferDesc.initialState = RHI::ResourceState::VertexBuffer;
	vertexBufferDesc.debugName = "Static Shadow Interop Vertex Buffer";
	const RHI::BufferHandle vertexBuffer = device.CreateBuffer(
		vertexBufferDesc,
		AsBytes(vertices)
	);
	assert(vertexBuffer);

	RHI::BufferDesc indexBufferDesc;
	indexBufferDesc.byteSize = static_cast<std::uint32_t>(sizeof(indices));
	indexBufferDesc.stride = static_cast<std::uint32_t>(sizeof(std::uint32_t));
	indexBufferDesc.usage = RHI::ResourceUsage::Immutable;
	indexBufferDesc.bindFlags = RHI::BufferBindFlags::Index;
	indexBufferDesc.initialState = RHI::ResourceState::IndexBuffer;
	indexBufferDesc.debugName = "Static Shadow Interop Index Buffer";
	const RHI::BufferHandle indexBuffer = device.CreateBuffer(
		indexBufferDesc,
		AsBytes(indices)
	);
	assert(indexBuffer);

	RHI::BufferDesc instanceBufferDesc;
	instanceBufferDesc.byteSize = static_cast<std::uint32_t>(sizeof(instances));
	instanceBufferDesc.stride =
		static_cast<std::uint32_t>(sizeof(StaticBatchInstanceData));
	instanceBufferDesc.usage = RHI::ResourceUsage::Immutable;
	instanceBufferDesc.bindFlags = RHI::BufferBindFlags::Vertex;
	instanceBufferDesc.initialState = RHI::ResourceState::VertexBuffer;
	instanceBufferDesc.debugName = "Static Shadow Interop Instance Buffer";
	const RHI::BufferHandle instanceBuffer = device.CreateBuffer(
		instanceBufferDesc,
		AsBytes(instances)
	);
	assert(instanceBuffer);

	RHI::TextureDesc depthTextureDesc;
	depthTextureDesc.width = 64;
	depthTextureDesc.height = 64;
	depthTextureDesc.format = RHI::Format::D32_Float;
	depthTextureDesc.bindFlags = RHI::TextureBindFlags::DepthStencil;
	depthTextureDesc.initialState = RHI::ResourceState::DepthWrite;
	depthTextureDesc.debugName = "Static Shadow Interop Depth";
	const RHI::TextureHandle depthTexture =
		device.CreateTexture(depthTextureDesc, {}, 0);
	assert(depthTexture);

	RHI::TextureViewDesc depthViewDesc;
	depthViewDesc.texture = depthTexture;
	depthViewDesc.type = RHI::TextureViewType::DepthStencil;
	depthViewDesc.format = RHI::Format::D32_Float;
	depthViewDesc.debugName = "Static Shadow Interop DSV";
	const RHI::TextureViewHandle depthView =
		device.CreateTextureView(depthViewDesc);
	assert(depthView);

	RHI::CommandListCreateDesc commandListDesc;
	commandListDesc.queueType = RHI::CommandQueueType::Graphics;
	std::unique_ptr<RHI::IRHICommandList> commandList =
		device.CreateCommandList(commandListDesc);
	assert(commandList);

	commandList->Begin();
	RHI::RenderPassDesc renderPass;
	renderPass.hasDepthAttachment = true;
	renderPass.depthAttachment.view = depthView;
	renderPass.depthAttachment.depthLoadOperation = RHI::LoadOperation::Clear;
	renderPass.depthAttachment.depthStoreOperation = RHI::StoreOperation::Store;
	renderPass.depthAttachment.clearDepth = 1.0f;
	renderPass.debugName = "Static Shadow Interop Pass";
	assert(commandList->BeginRenderPass(renderPass));

	RHI::Viewport viewport;
	viewport.width = 64.0f;
	viewport.height = 64.0f;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	commandList->SetViewport(viewport);

	StaticBatchDrawSubmissionDesc draw;
	draw.pipeline = shadowPipeline.PipelineState();
	draw.vertexBuffer = vertexBuffer;
	draw.indexBuffer = indexBuffer;
	draw.instanceBuffer = instanceBuffer;
	draw.vertexStride = static_cast<std::uint32_t>(sizeof(VERTEX_3D));
	draw.indexFormat = RHI::IndexFormat::UInt32;
	draw.indexCount = 3;
	draw.instanceCount = 2;
	assert(SubmitStaticBatchDraw(*commandList, draw));

	commandList->EndRenderPass();
	commandList->End();

	RHI::IRHICommandQueue* queue =
		device.GetQueue(RHI::CommandQueueType::Graphics);
	assert(queue);
	std::array<RHI::IRHICommandList*, 1> commandLists{commandList.get()};
	RHI::QueueSubmitDesc submitDesc;
	submitDesc.commandLists = commandLists;
	assert(queue->Submit(submitDesc));
	queue->WaitIdle();

	assert(device.DestroyTextureView(depthView));
	assert(device.DestroyTexture(depthTexture));
	assert(device.DestroyBuffer(instanceBuffer));
	assert(device.DestroyBuffer(indexBuffer));
	assert(device.DestroyBuffer(vertexBuffer));
	assert(shadowPipeline.Release(device));
	assert(device.DestroyShader(vertexShader));
	return 0;
}
