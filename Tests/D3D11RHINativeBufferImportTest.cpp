#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <span>

#include <d3d11.h>
#include <wrl/client.h>

#include "Service/Graphics/RHI/D3D11/D3D11RHIDevice.h"
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
	D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
	constexpr D3D_FEATURE_LEVEL requestedLevels[]{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0
	};
	HRESULT createResult = D3D11CreateDevice(
		nullptr,
		D3D_DRIVER_TYPE_WARP,
		nullptr,
		0,
		requestedLevels,
		static_cast<UINT>(std::size(requestedLevels)),
		D3D11_SDK_VERSION,
		result.device.GetAddressOf(),
		&featureLevel,
		result.context.GetAddressOf()
	);
	if(createResult == E_INVALIDARG){
		result.device.Reset();
		result.context.Reset();
		constexpr D3D_FEATURE_LEVEL fallbackLevel = D3D_FEATURE_LEVEL_11_0;
		createResult = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_WARP,
			nullptr,
			0,
			&fallbackLevel,
			1,
			D3D11_SDK_VERSION,
			result.device.GetAddressOf(),
			&featureLevel,
			result.context.GetAddressOf()
		);
	}
	assert(SUCCEEDED(createResult));
	assert(result.device);
	assert(result.context);
	assert(featureLevel >= D3D_FEATURE_LEVEL_11_0);
	return result;
}

Microsoft::WRL::ComPtr<ID3D11Buffer> CreateBuffer(
	ID3D11Device& device,
	UINT byteSize,
	UINT bindFlags,
	const void* initialData
){
	D3D11_BUFFER_DESC desc{};
	desc.ByteWidth = byteSize;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = bindFlags;

	D3D11_SUBRESOURCE_DATA upload{};
	upload.pSysMem = initialData;

	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
	const HRESULT createResult = device.CreateBuffer(
		&desc,
		initialData ? &upload : nullptr,
		buffer.GetAddressOf()
	);
	assert(SUCCEEDED(createResult));
	assert(buffer);
	return buffer;
}

} // namespace

int main(){
	NativeDeviceContext primary = CreateWarpDevice();
	NativeDeviceContext foreign = CreateWarpDevice();
	assert(primary.device.Get() != foreign.device.Get());

	RHI::SwapChainDesc swapChainDesc;
	swapChainDesc.width = 64;
	swapChainDesc.height = 64;
	swapChainDesc.bufferCount = 2;
	swapChainDesc.format = RHI::Format::RGBA8_UNorm;

	RHI::D3D11RHIDevice rhiDevice(
		primary.device.Get(),
		primary.context.Get(),
		nullptr,
		swapChainDesc
	);

	constexpr std::array<float, 9> vertices{
		-1.0f, -1.0f, 0.0f,
		 0.0f,  1.0f, 0.0f,
		 1.0f, -1.0f, 0.0f
	};
	constexpr std::array<std::uint32_t, 3> indices{0, 1, 2};
	constexpr std::uint32_t vertexStride =
		static_cast<std::uint32_t>(sizeof(float) * 3u);

	auto nativeVertexBuffer = CreateBuffer(
		*primary.device,
		static_cast<UINT>(sizeof(vertices)),
		D3D11_BIND_VERTEX_BUFFER,
		vertices.data()
	);
	auto nativeIndexBuffer = CreateBuffer(
		*primary.device,
		static_cast<UINT>(sizeof(indices)),
		D3D11_BIND_INDEX_BUFFER,
		indices.data()
	);

	assert(!rhiDevice.ImportNativeBuffer(
		nativeVertexBuffer.Get(),
		0,
		RHI::ResourceState::VertexBuffer,
		"Invalid Zero Stride Vertex Buffer"
	));
	assert(!rhiDevice.ImportNativeBuffer(
		nativeVertexBuffer.Get(),
		vertexStride,
		RHI::ResourceState::IndexBuffer,
		"Invalid Vertex As Index Buffer"
	));

	const RHI::BufferHandle vertexHandle = rhiDevice.ImportNativeBuffer(
		nativeVertexBuffer.Get(),
		vertexStride,
		RHI::ResourceState::VertexBuffer,
		"Imported Native Vertex Buffer"
	);
	const RHI::BufferHandle indexHandle = rhiDevice.ImportNativeBuffer(
		nativeIndexBuffer.Get(),
		static_cast<std::uint32_t>(sizeof(std::uint32_t)),
		RHI::ResourceState::IndexBuffer,
		"Imported Native Index Buffer"
	);
	assert(vertexHandle);
	assert(indexHandle);

	const RHI::BufferDesc* vertexDesc = rhiDevice.GetBufferDesc(vertexHandle);
	const RHI::BufferDesc* indexDesc = rhiDevice.GetBufferDesc(indexHandle);
	assert(vertexDesc);
	assert(indexDesc);
	assert(vertexDesc->byteSize == sizeof(vertices));
	assert(vertexDesc->stride == vertexStride);
	assert(vertexDesc->usage == RHI::ResourceUsage::Default);
	assert(vertexDesc->initialState == RHI::ResourceState::VertexBuffer);
	assert(RHI::HasAnyFlag(vertexDesc->bindFlags, RHI::BufferBindFlags::Vertex));
	assert(indexDesc->byteSize == sizeof(indices));
	assert(indexDesc->stride == sizeof(std::uint32_t));
	assert(indexDesc->initialState == RHI::ResourceState::IndexBuffer);
	assert(RHI::HasAnyFlag(indexDesc->bindFlags, RHI::BufferBindFlags::Index));

	nativeVertexBuffer.Reset();
	nativeIndexBuffer.Reset();

	std::unique_ptr<RHI::IRHICommandList> commandList =
		rhiDevice.CreateCommandList({RHI::CommandQueueType::Graphics, false});
	assert(commandList);
	commandList->Begin();
	assert(commandList->SetVertexBuffer(0, vertexHandle, vertexStride, 0));
	assert(commandList->SetIndexBuffer(
		indexHandle,
		RHI::IndexFormat::UInt32,
		0
	));
	commandList->End();

	auto foreignVertexBuffer = CreateBuffer(
		*foreign.device,
		static_cast<UINT>(sizeof(vertices)),
		D3D11_BIND_VERTEX_BUFFER,
		vertices.data()
	);
	assert(!rhiDevice.ImportNativeBuffer(
		foreignVertexBuffer.Get(),
		vertexStride,
		RHI::ResourceState::VertexBuffer,
		"Foreign Device Vertex Buffer"
	));

	assert(rhiDevice.DestroyBuffer(indexHandle));
	assert(rhiDevice.DestroyBuffer(vertexHandle));
	assert(rhiDevice.GetBufferDesc(indexHandle) == nullptr);
	assert(rhiDevice.GetBufferDesc(vertexHandle) == nullptr);

	// Native Buffer互換Fallback。
	auto bindingVertexBuffer = CreateBuffer(
		*primary.device,
		static_cast<UINT>(sizeof(vertices)),
		D3D11_BIND_VERTEX_BUFFER,
		vertices.data()
	);
	auto bindingIndexBuffer = CreateBuffer(
		*primary.device,
		static_cast<UINT>(sizeof(indices)),
		D3D11_BIND_INDEX_BUFFER,
		indices.data()
	);

	StaticBatchD3D11GeometrySource nativeSource;
	nativeSource.vertexBuffer = bindingVertexBuffer.Get();
	nativeSource.indexBuffer = bindingIndexBuffer.Get();
	nativeSource.vertexStride = vertexStride;
	nativeSource.vertexCount = 3;
	nativeSource.indexCount = 3;
	nativeSource.indexFormat = RHI::IndexFormat::UInt32;
	nativeSource.geometryResourceKey = 0x12345678ull;
	assert(nativeSource.IsValid());
	assert(!nativeSource.HasCpuData());
	assert(nativeSource.HasNativeBuffers());

	StaticBatchD3D11GeometryBinding nativeBinding;
	StaticBatchD3D11GeometrySource oversizedNativeSource = nativeSource;
	oversizedNativeSource.vertexCount = 4;
	assert(!nativeBinding.Create(rhiDevice, oversizedNativeSource));
	assert(!nativeBinding.IsAllocated());
	assert(nativeBinding.Create(rhiDevice, nativeSource));
	assert(nativeBinding.IsReady());
	assert(!nativeBinding.WasCreatedFromCpuData());
	assert(nativeBinding.Matches(nativeSource));
	assert(nativeBinding.VertexStride() == vertexStride);
	assert(nativeBinding.VertexCount() == 3);
	assert(nativeBinding.IndexCount() == 3);
	assert(nativeBinding.IndexFormat() == RHI::IndexFormat::UInt32);
	assert(nativeBinding.GeometryResourceKey() == nativeSource.geometryResourceKey);

	bindingVertexBuffer.Reset();
	bindingIndexBuffer.Reset();

	commandList = rhiDevice.CreateCommandList({
		RHI::CommandQueueType::Graphics,
		false
	});
	assert(commandList);
	commandList->Begin();
	assert(nativeBinding.Bind(*commandList));
	commandList->End();
	assert(nativeBinding.Release(rhiDevice));
	assert(!nativeBinding.IsAllocated());
	assert(!nativeBinding.IsReady());

	// CPU SnapshotからImmutable RHI Bufferを直接生成する主経路。
	StaticBatchD3D11GeometrySource cpuSource;
	cpuSource.vertexData = std::as_bytes(std::span<const float>(vertices));
	cpuSource.indexData = std::as_bytes(
		std::span<const std::uint32_t>(indices)
	);
	cpuSource.vertexStride = vertexStride;
	cpuSource.vertexCount = 3;
	cpuSource.indexCount = 3;
	cpuSource.indexFormat = RHI::IndexFormat::UInt32;
	cpuSource.geometryResourceKey = 0x87654321ull;
	assert(cpuSource.IsValid());
	assert(cpuSource.HasCpuData());
	assert(!cpuSource.HasNativeBuffers());

	StaticBatchD3D11GeometryBinding cpuBinding;
	StaticBatchD3D11GeometrySource oversizedCpuSource = cpuSource;
	oversizedCpuSource.indexCount = 4;
	assert(!oversizedCpuSource.IsValid());
	assert(!cpuBinding.Create(rhiDevice, oversizedCpuSource));
	assert(cpuBinding.Create(rhiDevice, cpuSource));
	assert(cpuBinding.IsReady());
	assert(cpuBinding.WasCreatedFromCpuData());
	assert(cpuBinding.Matches(cpuSource));

	const RHI::BufferDesc* cpuVertexDesc =
		rhiDevice.GetBufferDesc(cpuBinding.VertexBuffer());
	const RHI::BufferDesc* cpuIndexDesc =
		rhiDevice.GetBufferDesc(cpuBinding.IndexBuffer());
	assert(cpuVertexDesc);
	assert(cpuIndexDesc);
	assert(cpuVertexDesc->byteSize == sizeof(vertices));
	assert(cpuVertexDesc->stride == vertexStride);
	assert(cpuVertexDesc->usage == RHI::ResourceUsage::Immutable);
	assert(cpuVertexDesc->initialState == RHI::ResourceState::VertexBuffer);
	assert(RHI::HasAnyFlag(
		cpuVertexDesc->bindFlags,
		RHI::BufferBindFlags::Vertex
	));
	assert(cpuIndexDesc->byteSize == sizeof(indices));
	assert(cpuIndexDesc->stride == sizeof(std::uint32_t));
	assert(cpuIndexDesc->usage == RHI::ResourceUsage::Immutable);
	assert(cpuIndexDesc->initialState == RHI::ResourceState::IndexBuffer);
	assert(RHI::HasAnyFlag(
		cpuIndexDesc->bindFlags,
		RHI::BufferBindFlags::Index
	));

	commandList = rhiDevice.CreateCommandList({
		RHI::CommandQueueType::Graphics,
		false
	});
	assert(commandList);
	commandList->Begin();
	assert(cpuBinding.Bind(*commandList));
	commandList->End();
	assert(cpuBinding.Release(rhiDevice));
	assert(!cpuBinding.IsAllocated());
	assert(!cpuBinding.IsReady());
	return 0;
}
