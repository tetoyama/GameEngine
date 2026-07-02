#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

#include <windows.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include "Service/Graphics/RHI/D3D11/D3D11RHIBackend.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "user32.lib")

namespace {

LRESULT CALLBACK TriangleTestWindowProc(
	HWND window,
	UINT message,
	WPARAM wParam,
	LPARAM lParam
){
	return DefWindowProcW(window, message, wParam, lParam);
}

class TestWindow {
public:
	~TestWindow(){
		if(m_window) DestroyWindow(m_window);
		if(m_classAtom) UnregisterClassW(m_className, m_instance);
	}

	bool Create(){
		m_instance = GetModuleHandleW(nullptr);
		WNDCLASSW windowClass{};
		windowClass.lpfnWndProc = TriangleTestWindowProc;
		windowClass.hInstance = m_instance;
		windowClass.lpszClassName = m_className;
		m_classAtom = RegisterClassW(&windowClass);
		if(!m_classAtom) return false;

		m_window = CreateWindowExW(
			0,
			m_className,
			L"D3D11 RHI Real Triangle Test",
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			128,
			128,
			nullptr,
			nullptr,
			m_instance,
			nullptr
		);
		return m_window != nullptr;
	}

	HWND Get() const noexcept { return m_window; }

private:
	static constexpr const wchar_t* m_className =
		L"GameEngineD3D11RHIRealTriangleTestWindow";
	HINSTANCE m_instance = nullptr;
	ATOM m_classAtom = 0;
	HWND m_window = nullptr;
};

Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
	const char* source,
	const char* entryPoint,
	const char* target
){
	Microsoft::WRL::ComPtr<ID3DBlob> byteCode;
	Microsoft::WRL::ComPtr<ID3DBlob> errors;
	const HRESULT result = D3DCompile(
		source,
		std::strlen(source),
		nullptr,
		nullptr,
		nullptr,
		entryPoint,
		target,
		D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS,
		0,
		byteCode.GetAddressOf(),
		errors.GetAddressOf()
	);
	if(FAILED(result) && errors){
		OutputDebugStringA(static_cast<const char*>(errors->GetBufferPointer()));
	}
	assert(SUCCEEDED(result));
	return byteCode;
}

std::span<const std::byte> BlobBytes(ID3DBlob* blob){
	assert(blob);
	return {
		reinterpret_cast<const std::byte*>(blob->GetBufferPointer()),
		blob->GetBufferSize()
	};
}

struct Vertex {
	float position[2];
	float color[4];
};

} // namespace

int main(){
	TestWindow window;
	assert(window.Create());

	RHI::D3D11RHIBackend backend;
	const std::vector<RHI::AdapterInfo> adapters = backend.EnumerateAdapters();
	assert(!adapters.empty());

	const RHI::AdapterInfo* selected = nullptr;
	for(const RHI::AdapterInfo& adapter : adapters){
		if(!adapter.software){
			selected = &adapter;
			break;
		}
	}
	if(!selected) selected = &adapters.front();

	RHI::DeviceCreateDesc deviceDesc;
	deviceDesc.nativeWindow.window = window.Get();
	deviceDesc.adapterIndex = selected->index;
	deviceDesc.allowSoftwareAdapter = selected->software;
	deviceDesc.swapChain.width = 64;
	deviceDesc.swapChain.height = 64;
	deviceDesc.swapChain.bufferCount = 2;
	deviceDesc.swapChain.format = RHI::Format::RGBA8_UNorm;
	deviceDesc.swapChain.allowTearing = false;
	deviceDesc.swapChain.debugName = "D3D11 RHI Real Triangle Test";

	std::unique_ptr<RHI::IRHIDevice> device = backend.CreateDevice(deviceDesc);
	assert(device);
	assert(device->GetBackendType() == RHI::BackendType::Direct3D11);

	constexpr const char* shaderSource = R"(
struct VSInput {
	float2 position : POSITION;
	float4 color : COLOR;
};

struct VSOutput {
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

VSOutput VSMain(VSInput input){
	VSOutput output;
	output.position = float4(input.position, 0.0f, 1.0f);
	output.color = input.color;
	return output;
}

float4 PSMain(VSOutput input) : SV_TARGET {
	return input.color;
}
)";

	const auto vertexByteCode = CompileShader(shaderSource, "VSMain", "vs_5_0");
	const auto pixelByteCode = CompileShader(shaderSource, "PSMain", "ps_5_0");

	RHI::ShaderDesc vertexShaderDesc;
	vertexShaderDesc.stage = RHI::ShaderStage::Vertex;
	vertexShaderDesc.entryPoint = "VSMain";
	vertexShaderDesc.debugName = "TriangleVS";
	const RHI::ShaderHandle vertexShader = device->CreateShader(
		vertexShaderDesc,
		BlobBytes(vertexByteCode.Get())
	);
	assert(vertexShader);

	RHI::ShaderDesc pixelShaderDesc;
	pixelShaderDesc.stage = RHI::ShaderStage::Pixel;
	pixelShaderDesc.entryPoint = "PSMain";
	pixelShaderDesc.debugName = "TrianglePS";
	const RHI::ShaderHandle pixelShader = device->CreateShader(
		pixelShaderDesc,
		BlobBytes(pixelByteCode.Get())
	);
	assert(pixelShader);

	constexpr std::array<Vertex, 3> vertices{{
		{{-0.8f, -0.8f}, {1.0f, 0.0f, 0.0f, 1.0f}},
		{{ 0.0f,  0.8f}, {1.0f, 0.0f, 0.0f, 1.0f}},
		{{ 0.8f, -0.8f}, {1.0f, 0.0f, 0.0f, 1.0f}},
	}};

	RHI::BufferDesc vertexBufferDesc;
	vertexBufferDesc.byteSize = static_cast<uint32_t>(sizeof(vertices));
	vertexBufferDesc.stride = static_cast<uint32_t>(sizeof(Vertex));
	vertexBufferDesc.usage = RHI::ResourceUsage::Immutable;
	vertexBufferDesc.bindFlags = RHI::BufferBindFlags::Vertex;
	vertexBufferDesc.initialState = RHI::ResourceState::VertexBuffer;
	vertexBufferDesc.debugName = "TriangleVertexBuffer";
	const RHI::BufferHandle vertexBuffer = device->CreateBuffer(
		vertexBufferDesc,
		std::as_bytes(std::span<const Vertex>(vertices))
	);
	assert(vertexBuffer);

	RHI::PipelineStateDesc pipelineDesc;
	pipelineDesc.vertexShader = vertexShader;
	pipelineDesc.pixelShader = pixelShader;
	pipelineDesc.inputLayout = {
		{"POSITION", 0, RHI::Format::RG32_Float, 0, 0, false, 0},
		{"COLOR", 0, RHI::Format::RGBA32_Float, 0, 8, false, 0},
	};
	pipelineDesc.topology = RHI::PrimitiveTopology::TriangleList;
	pipelineDesc.rasterizer.cullMode = RHI::CullMode::None;
	pipelineDesc.depthStencil.depthEnable = false;
	pipelineDesc.depthStencil.depthWriteEnable = false;
	pipelineDesc.renderTargets.colorAttachmentCount = 1;
	pipelineDesc.renderTargets.colorFormats[0] = deviceDesc.swapChain.format;
	pipelineDesc.renderTargets.sampleCount = 1;
	pipelineDesc.debugName = "TrianglePipeline";
	const RHI::PipelineStateHandle pipeline =
		device->CreatePipelineState(pipelineDesc);
	assert(pipeline);

	RHI::IRHISwapChain* swapChain = device->GetSwapChain();
	assert(swapChain);
	const RHI::TextureHandle backBuffer = swapChain->GetCurrentImage();
	assert(backBuffer);

	RHI::TextureViewDesc renderTargetViewDesc;
	renderTargetViewDesc.texture = backBuffer;
	renderTargetViewDesc.type = RHI::TextureViewType::RenderTarget;
	renderTargetViewDesc.format = deviceDesc.swapChain.format;
	renderTargetViewDesc.debugName = "TriangleBackBufferRTV";
	const RHI::TextureViewHandle renderTargetView =
		device->CreateTextureView(renderTargetViewDesc);
	assert(renderTargetView);

	std::unique_ptr<RHI::IRHICommandList> commandList = device->CreateCommandList({
		RHI::CommandQueueType::Graphics,
		false
	});
	assert(commandList);

	commandList->Begin();
	const RHI::ResourceBarrierDesc toRenderTarget{
		RHI::ResourceBarrierType::Transition,
		{},
		backBuffer,
		RHI::ResourceState::Present,
		RHI::ResourceState::RenderTarget,
		RHI::AllSubresources
	};
	assert(commandList->ResourceBarrier(std::span(&toRenderTarget, 1)));

	RHI::RenderPassDesc renderPass;
	RHI::ColorAttachmentDesc colorAttachment;
	colorAttachment.view = renderTargetView;
	colorAttachment.loadOperation = RHI::LoadOperation::Clear;
	colorAttachment.storeOperation = RHI::StoreOperation::Store;
	colorAttachment.clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
	renderPass.colorAttachments.push_back(colorAttachment);
	renderPass.debugName = "TriangleRenderPass";
	assert(commandList->BeginRenderPass(renderPass));
	assert(commandList->SetPipelineState(pipeline));
	commandList->SetViewport({0.0f, 0.0f, 64.0f, 64.0f, 0.0f, 1.0f});
	assert(commandList->SetVertexBuffer(
		0,
		vertexBuffer,
		static_cast<uint32_t>(sizeof(Vertex)),
		0
	));
	commandList->Draw(3, 0);
	commandList->EndRenderPass();

	const RHI::ResourceBarrierDesc toPresent{
		RHI::ResourceBarrierType::Transition,
		{},
		backBuffer,
		RHI::ResourceState::RenderTarget,
		RHI::ResourceState::Present,
		RHI::AllSubresources
	};
	assert(commandList->ResourceBarrier(std::span(&toPresent, 1)));
	commandList->End();

	RHI::IRHICommandQueue* graphicsQueue =
		device->GetQueue(RHI::CommandQueueType::Graphics);
	assert(graphicsQueue);
	std::array<RHI::IRHICommandList*, 1> commandLists{commandList.get()};
	RHI::QueueSubmitDesc submit;
	submit.commandLists = commandLists;
	assert(graphicsQueue->Submit(submit));
	device->WaitIdle();

	auto* d3d11Device = dynamic_cast<RHI::D3D11RHIDevice*>(device.get());
	assert(d3d11Device);
	Microsoft::WRL::ComPtr<ID3D11Texture2D> nativeBackBuffer;
	assert(SUCCEEDED(d3d11Device->NativeSwapChain()->GetBuffer(
		0,
		IID_PPV_ARGS(nativeBackBuffer.GetAddressOf())
	)));

	D3D11_TEXTURE2D_DESC stagingDesc{};
	nativeBackBuffer->GetDesc(&stagingDesc);
	stagingDesc.BindFlags = 0;
	stagingDesc.MiscFlags = 0;
	stagingDesc.Usage = D3D11_USAGE_STAGING;
	stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
	assert(SUCCEEDED(d3d11Device->NativeDevice()->CreateTexture2D(
		&stagingDesc,
		nullptr,
		staging.GetAddressOf()
	)));
	d3d11Device->NativeContext()->CopyResource(staging.Get(), nativeBackBuffer.Get());
	d3d11Device->NativeContext()->Flush();

	D3D11_MAPPED_SUBRESOURCE mapped{};
	assert(SUCCEEDED(d3d11Device->NativeContext()->Map(
		staging.Get(),
		0,
		D3D11_MAP_READ,
		0,
		&mapped
	)));
	const auto* row = static_cast<const uint8_t*>(mapped.pData) +
		(stagingDesc.Height / 2u) * mapped.RowPitch;
	const auto* pixel = row + (stagingDesc.Width / 2u) * 4u;
	const bool triangleVisible =
		pixel[0] >= 192u && pixel[1] <= 32u && pixel[2] <= 32u && pixel[3] >= 192u;
	d3d11Device->NativeContext()->Unmap(staging.Get(), 0);
	assert(triangleVisible);
	assert(device->PresentSwapChain(false));

	assert(device->DestroyTextureView(renderTargetView));
	assert(device->DestroyPipelineState(pipeline));
	assert(device->DestroyBuffer(vertexBuffer));
	assert(device->DestroyShader(pixelShader));
	assert(device->DestroyShader(vertexShader));
	return 0;
}
