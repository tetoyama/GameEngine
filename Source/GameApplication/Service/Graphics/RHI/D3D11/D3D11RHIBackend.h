// =======================================================================
// D3D11RHIBackend.h
//
// Direct3D 11 Device / SwapChain生成をGraphicsContextから分離するBackend。
// =======================================================================
#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <d3d11.h>
#include <dxgi1_5.h>
#include <wrl/client.h>

#include "Service/Graphics/RHI/RHIBackend.h"
#include "D3D11RHIDevice.h"
#include "D3D11RHIConversions.h"

namespace RHI {

class D3D11RHIBackend final : public IRHIBackend {
public:
	BackendType GetType() const noexcept override {
		return BackendType::Direct3D11;
	}

	std::string_view GetName() const noexcept override {
		return "Direct3D 11";
	}

	bool IsSupported() const override {
#if defined(_WIN32)
		Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
		return SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
#else
		return false;
#endif
	}

	std::vector<AdapterInfo> EnumerateAdapters() const override {
		std::vector<AdapterInfo> result;
		Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
		if(FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))){
			return result;
		}

		for(UINT index = 0;; ++index){
			Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
			if(factory->EnumAdapters1(index, &adapter) == DXGI_ERROR_NOT_FOUND){
				break;
			}
			if(!adapter) continue;

			DXGI_ADAPTER_DESC1 desc{};
			if(FAILED(adapter->GetDesc1(&desc))) continue;

			AdapterInfo info;
			info.index = index;
			info.name = WideToUtf8(desc.Description);
			info.dedicatedVideoMemory = desc.DedicatedVideoMemory;
			info.sharedSystemMemory = desc.SharedSystemMemory;
			info.software = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
			result.push_back(std::move(info));
		}
		return result;
	}

	std::unique_ptr<IRHIDevice> CreateDevice(
		const DeviceCreateDesc& desc
	) override {
		HWND window = static_cast<HWND>(desc.nativeWindow.window);
		if(!window || desc.swapChain.width == 0 || desc.swapChain.height == 0){
			return nullptr;
		}

		Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
		if(FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))){
			return nullptr;
		}

		Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
		if(FAILED(factory->EnumAdapters1(desc.adapterIndex, &adapter)) || !adapter){
			return nullptr;
		}

		DXGI_ADAPTER_DESC1 adapterDesc{};
		if(FAILED(adapter->GetDesc1(&adapterDesc))){
			return nullptr;
		}
		const bool softwareAdapter =
			(adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
		if(softwareAdapter && !desc.allowSoftwareAdapter){
			return nullptr;
		}

		bool tearingSupported = false;
		Microsoft::WRL::ComPtr<IDXGIFactory5> factory5;
		if(SUCCEEDED(factory.As(&factory5))){
			BOOL allowTearing = FALSE;
			tearingSupported = SUCCEEDED(factory5->CheckFeatureSupport(
				DXGI_FEATURE_PRESENT_ALLOW_TEARING,
				&allowTearing,
				sizeof(allowTearing)
			)) && allowTearing;
		}

		SwapChainDesc resolvedSwapChain = desc.swapChain;
		resolvedSwapChain.allowTearing =
			desc.swapChain.allowTearing && tearingSupported;

		DXGI_SWAP_CHAIN_DESC nativeSwapChain{};
		nativeSwapChain.BufferCount = resolvedSwapChain.bufferCount;
		nativeSwapChain.BufferDesc.Width = resolvedSwapChain.width;
		nativeSwapChain.BufferDesc.Height = resolvedSwapChain.height;
		nativeSwapChain.BufferDesc.Format = D3D11Detail::ToDXGIFormat(
			resolvedSwapChain.format
		);
		if(nativeSwapChain.BufferDesc.Format == DXGI_FORMAT_UNKNOWN){
			return nullptr;
		}
		nativeSwapChain.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		nativeSwapChain.OutputWindow = window;
		nativeSwapChain.SampleDesc.Count = 1;
		nativeSwapChain.Windowed = TRUE;
		nativeSwapChain.SwapEffect = resolvedSwapChain.allowTearing
			? DXGI_SWAP_EFFECT_FLIP_DISCARD
			: DXGI_SWAP_EFFECT_DISCARD;
		nativeSwapChain.Flags = resolvedSwapChain.allowTearing
			? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING
			: 0;

		UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
		if(desc.enableDebugLayer){
			creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
		}

		constexpr D3D_FEATURE_LEVEL featureLevels[] = {
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_1,
			D3D_FEATURE_LEVEL_10_0,
		};
		constexpr UINT featureLevelCount = static_cast<UINT>(
			sizeof(featureLevels) / sizeof(featureLevels[0])
		);

		Microsoft::WRL::ComPtr<ID3D11Device> device;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
		Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
		D3D_FEATURE_LEVEL createdFeatureLevel{};

		HRESULT createResult = D3D11CreateDeviceAndSwapChain(
			adapter.Get(),
			D3D_DRIVER_TYPE_UNKNOWN,
			nullptr,
			creationFlags,
			featureLevels,
			featureLevelCount,
			D3D11_SDK_VERSION,
			&nativeSwapChain,
			&swapChain,
			&device,
			&createdFeatureLevel,
			&context
		);

		if(FAILED(createResult) && desc.enableDebugLayer){
			creationFlags &= ~D3D11_CREATE_DEVICE_DEBUG;
			createResult = D3D11CreateDeviceAndSwapChain(
				adapter.Get(),
				D3D_DRIVER_TYPE_UNKNOWN,
				nullptr,
				creationFlags,
				featureLevels,
				featureLevelCount,
				D3D11_SDK_VERSION,
				&nativeSwapChain,
				&swapChain,
				&device,
				&createdFeatureLevel,
				&context
			);
		}

		if(FAILED(createResult)) return nullptr;

		factory->MakeWindowAssociation(
			window,
			DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_ALT_ENTER
		);

		return std::make_unique<D3D11RHIDevice>(
			device.Get(),
			context.Get(),
			swapChain.Get(),
			resolvedSwapChain
		);
	}

private:
	static std::string WideToUtf8(const wchar_t* text){
		if(!text || *text == L'\0') return {};

		const int sizeWithTerminator = WideCharToMultiByte(
			CP_UTF8,
			0,
			text,
			-1,
			nullptr,
			0,
			nullptr,
			nullptr
		);
		if(sizeWithTerminator <= 1) return {};

		std::string result(
			static_cast<size_t>(sizeWithTerminator),
			'\0'
		);
		const int written = WideCharToMultiByte(
			CP_UTF8,
			0,
			text,
			-1,
			result.data(),
			sizeWithTerminator,
			nullptr,
			nullptr
		);
		if(written <= 1) return {};
		result.resize(static_cast<size_t>(written - 1));
		return result;
	}
};

inline std::unique_ptr<IRHIBackend> CreateD3D11RHIBackend(){
	return std::make_unique<D3D11RHIBackend>();
}

inline bool RegisterD3D11RHIBackend(BackendRegistry& registry){
	return registry.Register(
		BackendType::Direct3D11,
		&CreateD3D11RHIBackend
	);
}

} // namespace RHI
