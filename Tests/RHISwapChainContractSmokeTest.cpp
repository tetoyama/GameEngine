#include <cassert>
#include <cstdint>

#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>

#include "Service/Graphics/RHI/Null/NullRHIBackend.h"
#include "Service/Graphics/RHI/D3D11/D3D11RHIDevice.h"

namespace {

void ValidateSwapChainContract(RHI::IRHIDevice& device){
	auto* swapChain = device.GetSwapChain();
	assert(swapChain);
	assert(swapChain->GetImageCount() > 0);
	assert(swapChain->GetPresentQueueType() == RHI::CommandQueueType::Graphics);

	const RHI::TextureHandle oldImage = swapChain->GetCurrentImage();
	assert(oldImage);
	const RHI::TextureDesc* oldDesc = device.GetTextureDesc(oldImage);
	assert(oldDesc && oldDesc->initialState == RHI::ResourceState::Present);
	assert(!device.DestroyTexture(oldImage));

	RHI::TextureViewDesc viewDesc;
	viewDesc.texture = oldImage;
	viewDesc.type = RHI::TextureViewType::RenderTarget;
	const RHI::TextureViewHandle liveView = device.CreateTextureView(viewDesc);
	assert(liveView);
	assert(!device.ResizeSwapChain(800, 450));
	assert(device.GetTextureDesc(oldImage));
	assert(device.DestroyTextureView(liveView));

	assert(device.ResizeSwapChain(800, 450));
	assert(device.GetTextureDesc(oldImage) == nullptr);
	const RHI::TextureHandle newImage = swapChain->GetCurrentImage();
	assert(newImage && newImage != oldImage);
	const RHI::TextureDesc* newDesc = device.GetTextureDesc(newImage);
	assert(newDesc && newDesc->width == 800 && newDesc->height == 450);

	auto* computeQueue = device.GetQueue(RHI::CommandQueueType::Compute);
	assert(computeQueue && !computeQueue->Present(*swapChain, false));
	assert(device.PresentSwapChain(false));
}

void TestNullSwapChain(){
	RHI::DeviceCreateDesc desc;
	desc.swapChain.width = 640;
	desc.swapChain.height = 360;
	RHI::NullRHIDevice device(desc);
	ValidateSwapChainContract(device);
}

LRESULT CALLBACK TestWindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam){
	return DefWindowProcW(window, message, wParam, lParam);
}

HWND CreateTestWindow(){
	HINSTANCE instance = GetModuleHandleW(nullptr);
	const wchar_t* className = L"GameEngineRHIContractTestWindow";
	WNDCLASSW windowClass{};
	windowClass.lpfnWndProc = TestWindowProcedure;
	windowClass.hInstance = instance;
	windowClass.lpszClassName = className;
	RegisterClassW(&windowClass);
	return CreateWindowExW(
		0, className, L"RHI Contract Test", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 640, 360,
		nullptr, nullptr, instance, nullptr
	);
}

void TestD3D11SwapChain(){
	HWND window = CreateTestWindow();
	assert(window);

	DXGI_SWAP_CHAIN_DESC nativeSwapChain{};
	nativeSwapChain.BufferCount = 1;
	nativeSwapChain.BufferDesc.Width = 640;
	nativeSwapChain.BufferDesc.Height = 360;
	nativeSwapChain.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	nativeSwapChain.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	nativeSwapChain.OutputWindow = window;
	nativeSwapChain.SampleDesc.Count = 1;
	nativeSwapChain.Windowed = TRUE;
	nativeSwapChain.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	Microsoft::WRL::ComPtr<ID3D11Device> nativeDevice;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> nativeContext;
	Microsoft::WRL::ComPtr<IDXGISwapChain> nativeSwapChainObject;
	D3D_FEATURE_LEVEL featureLevel{};
	const HRESULT result = D3D11CreateDeviceAndSwapChain(
		nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
		nullptr, 0, D3D11_SDK_VERSION,
		&nativeSwapChain, nativeSwapChainObject.GetAddressOf(),
		nativeDevice.GetAddressOf(), &featureLevel,
		nativeContext.GetAddressOf()
	);
	assert(SUCCEEDED(result));

	{
		RHI::SwapChainDesc desc;
		desc.width = 640;
		desc.height = 360;
		desc.bufferCount = 1;
		RHI::D3D11RHIDevice device(
			nativeDevice.Get(), nativeContext.Get(),
			nativeSwapChainObject.Get(), desc
		);
		ValidateSwapChainContract(device);
	}

	nativeSwapChainObject.Reset();
	nativeContext.Reset();
	nativeDevice.Reset();
	DestroyWindow(window);
}

} // namespace

int main(){
	TestNullSwapChain();
	TestD3D11SwapChain();
	return 0;
}
