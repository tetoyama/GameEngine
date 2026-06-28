#include <array>
#include <cassert>
#include <cstddef>
#include <iterator>

#include <d3d11.h>
#include <wrl/client.h>

#include "Engine/Scene/System/Render/StaticBatch/StaticBatchD3D11GBufferTargetBinding.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace {

struct DeviceContext {
	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
};

DeviceContext CreateWarpDevice(){
	DeviceContext result;
	D3D_FEATURE_LEVEL selected = D3D_FEATURE_LEVEL_11_0;
	constexpr D3D_FEATURE_LEVEL levels[]{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0
	};
	HRESULT hr = D3D11CreateDevice(
		nullptr,
		D3D_DRIVER_TYPE_WARP,
		nullptr,
		0,
		levels,
		static_cast<UINT>(std::size(levels)),
		D3D11_SDK_VERSION,
		result.device.GetAddressOf(),
		&selected,
		result.context.GetAddressOf()
	);
	if(hr == E_INVALIDARG){
		result.device.Reset();
		result.context.Reset();
		constexpr D3D_FEATURE_LEVEL fallback = D3D_FEATURE_LEVEL_11_0;
		hr = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_WARP,
			nullptr,
			0,
			&fallback,
			1,
			D3D11_SDK_VERSION,
			result.device.GetAddressOf(),
			&selected,
			result.context.GetAddressOf()
		);
	}
	assert(SUCCEEDED(hr));
	assert(result.device);
	assert(result.context);
	return result;
}

Microsoft::WRL::ComPtr<ID3D11RenderTargetView> CreateColorTarget(
	ID3D11Device& device,
	DXGI_FORMAT format,
	UINT width = 32,
	UINT height = 32
){
	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = format;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
	assert(SUCCEEDED(device.CreateTexture2D(
		&desc,
		nullptr,
		texture.GetAddressOf()
	)));
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> view;
	assert(SUCCEEDED(device.CreateRenderTargetView(
		texture.Get(),
		nullptr,
		view.GetAddressOf()
	)));
	return view;
}

Microsoft::WRL::ComPtr<ID3D11DepthStencilView> CreateDepthTarget(
	ID3D11Device& device,
	UINT width = 32,
	UINT height = 32
){
	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_D32_FLOAT;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
	assert(SUCCEEDED(device.CreateTexture2D(
		&desc,
		nullptr,
		texture.GetAddressOf()
	)));
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> view;
	assert(SUCCEEDED(device.CreateDepthStencilView(
		texture.Get(),
		nullptr,
		view.GetAddressOf()
	)));
	return view;
}

} // namespace

int main(){
	DeviceContext primary = CreateWarpDevice();
	DeviceContext foreign = CreateWarpDevice();

	std::array<
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView>,
		StaticBatchD3D11GBufferTargetBinding::ColorTargetCount
	> ownedTargets;
	for(std::size_t index = 0; index < 5; ++index){
		ownedTargets[index] = CreateColorTarget(
			*primary.device,
			DXGI_FORMAT_R16G16B16A16_FLOAT
		);
	}
	ownedTargets[5] = CreateColorTarget(
		*primary.device,
		DXGI_FORMAT_R32G32B32A32_UINT
	);
	const auto depth = CreateDepthTarget(*primary.device);

	std::array<
		ID3D11RenderTargetView*,
		StaticBatchD3D11GBufferTargetBinding::ColorTargetCount
	> targets{};
	for(std::size_t index = 0; index < targets.size(); ++index){
		targets[index] = ownedTargets[index].Get();
	}

	StaticBatchD3D11GBufferTargetBinding binding;
	assert(binding.Resolve(primary.device.Get(), targets, depth.Get()));
	assert(binding.IsValid());
	assert(binding.Width() == 32);
	assert(binding.Height() == 32);
	assert(binding.SampleCount() == 1);
	assert(binding.Bind(primary.context.Get()));

	std::array<
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView>,
		StaticBatchD3D11GBufferTargetBinding::ColorTargetCount
	> reboundTargets;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> reboundDepth;
	std::array<ID3D11RenderTargetView*, 6> rawRebound{};
	primary.context->OMGetRenderTargets(
		static_cast<UINT>(rawRebound.size()),
		rawRebound.data(),
		reboundDepth.GetAddressOf()
	);
	for(std::size_t index = 0; index < rawRebound.size(); ++index){
		reboundTargets[index].Attach(rawRebound[index]);
		assert(reboundTargets[index].Get() == ownedTargets[index].Get());
	}
	assert(reboundDepth.Get() == depth.Get());

	auto wrongSize = CreateColorTarget(
		*primary.device,
		DXGI_FORMAT_R16G16B16A16_FLOAT,
		16,
		32
	);
	targets[0] = wrongSize.Get();
	assert(!binding.Resolve(primary.device.Get(), targets, depth.Get()));
	assert(!binding.IsValid());

	targets[0] = ownedTargets[0].Get();
	auto foreignTarget = CreateColorTarget(
		*foreign.device,
		DXGI_FORMAT_R16G16B16A16_FLOAT
	);
	targets[1] = foreignTarget.Get();
	assert(!binding.Resolve(primary.device.Get(), targets, depth.Get()));
	assert(!binding.IsValid());
	return 0;
}
