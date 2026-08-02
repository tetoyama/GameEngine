#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>

#include <d3d11.h>
#include <wrl/client.h>

#include "Engine/Scene/System/Render/StaticBatch/StaticBatchD3D11GeometryBinding.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace {

struct NativeDeviceContext {
	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
};

NativeDeviceContext CreateWarpDevice(){
	NativeDeviceContext result;
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

RHI::D3D11RHIDevice CreateRhiDevice(const NativeDeviceContext& native){
	RHI::SwapChainDesc desc;
	desc.width = 64;
	desc.height = 64;
	desc.bufferCount = 2;
	desc.format = RHI::Format::RGBA8_UNorm;
	return RHI::D3D11RHIDevice(
		native.device.Get(),
		native.context.Get(),
		nullptr,
		desc
	);
}

} // namespace

int main(){
	NativeDeviceContext native = CreateWarpDevice();
	RHI::D3D11RHIDevice device = CreateRhiDevice(native);

	std::array<std::byte, 36> vertices{};
	std::array<std::uint32_t, 3> indices{0, 1, 2};

	StaticBatchD3D11GeometrySource revision1;
	revision1.vertexData = std::span<const std::byte>(vertices);
	revision1.indexData = std::as_bytes(
		std::span<const std::uint32_t>(indices)
	);
	revision1.vertexStride = 12;
	revision1.vertexCount = 3;
	revision1.indexCount = 3;
	revision1.indexFormat = RHI::IndexFormat::UInt32;
	revision1.geometryResourceKey = 77;
	revision1.sourceRevision = 1;
	assert(revision1.IsValid());

	StaticBatchD3D11GeometryBinding binding;
	assert(binding.Create(device, revision1));
	assert(binding.IsReady());
	assert(binding.SourceRevision() == 1);
	assert(binding.Matches(revision1));

	// Geometry Key、Stride、Vertex / Index Countが同一でも、Revisionが変われば
	// 古いGPU Bindingを再利用してはならない。
	StaticBatchD3D11GeometrySource revision2 = revision1;
	revision2.sourceRevision = 2;
	assert(!binding.Matches(revision2));

	assert(binding.Release(device));
	assert(binding.Create(device, revision2));
	assert(binding.SourceRevision() == 2);
	assert(binding.Matches(revision2));
	assert(!binding.Matches(revision1));

	binding.Abandon();
	assert(!binding.IsReady());
	assert(!binding.IsAllocated());
	assert(binding.SourceRevision() == 0);
	return 0;
}
