#pragma once

#include <cstdint>
#include <limits>

#include "Service/Graphics/RHI/D3D11/D3D11RHIDevice.h"
#include "System/Render/StaticBatch/StaticBatchD3D11GeometrySource.h"

class StaticBatchD3D11GeometryBinding {
public:
	StaticBatchD3D11GeometryBinding() = default;
	StaticBatchD3D11GeometryBinding(
		const StaticBatchD3D11GeometryBinding&
	) = delete;
	StaticBatchD3D11GeometryBinding& operator=(
		const StaticBatchD3D11GeometryBinding&
	) = delete;
	StaticBatchD3D11GeometryBinding(
		StaticBatchD3D11GeometryBinding&&
	) = delete;
	StaticBatchD3D11GeometryBinding& operator=(
		StaticBatchD3D11GeometryBinding&&
	) = delete;

	bool Create(
		RHI::IRHIDevice& device,
		const StaticBatchD3D11GeometrySource& source
	){
		if(IsAllocated() || !source.IsValid() ||
			device.GetBackendType() != RHI::BackendType::Direct3D11){
			return false;
		}

		if(source.HasCpuData()){
			if(!CreateFromCpuData(device, source)) return false;
			m_createdFromCpuData = true;
		}else{
			if(!CreateFromNativeBuffers(device, source)) return false;
			m_createdFromCpuData = false;
		}

		const RHI::BufferDesc* vertexDesc =
			device.GetBufferDesc(m_vertexBuffer);
		const RHI::BufferDesc* indexDesc =
			device.GetBufferDesc(m_indexBuffer);
		if(!vertexDesc || !indexDesc ||
			source.RequiredVertexBytes() > vertexDesc->byteSize ||
			source.RequiredIndexBytes() > indexDesc->byteSize){
			Release(device);
			return false;
		}

		m_nativeVertexBuffer = source.HasNativeBuffers()
			? source.vertexBuffer
			: nullptr;
		m_nativeIndexBuffer = source.HasNativeBuffers()
			? source.indexBuffer
			: nullptr;
		m_vertexStride = source.vertexStride;
		m_vertexCount = source.vertexCount;
		m_indexCount = source.indexCount;
		m_indexFormat = source.indexFormat;
		m_geometryResourceKey = source.geometryResourceKey;
		m_sourceRevision = source.sourceRevision;
		return true;
	}

	bool Bind(RHI::IRHICommandList& commandList) const {
		if(!IsReady()) return false;
		if(!commandList.SetVertexBuffer(
			0,
			m_vertexBuffer,
			m_vertexStride,
			0
		)){
			return false;
		}
		return commandList.SetIndexBuffer(
			m_indexBuffer,
			m_indexFormat,
			0
		);
	}

	bool Release(RHI::IRHIDevice& device) noexcept {
		bool released = true;
		if(m_indexBuffer){
			if(device.DestroyBuffer(m_indexBuffer)){
				m_indexBuffer = {};
			}else{
				released = false;
			}
		}
		if(m_vertexBuffer){
			if(device.DestroyBuffer(m_vertexBuffer)){
				m_vertexBuffer = {};
			}else{
				released = false;
			}
		}
		if(!IsAllocated()){
			m_nativeVertexBuffer = nullptr;
			m_nativeIndexBuffer = nullptr;
			m_createdFromCpuData = false;
			m_vertexStride = 0;
			m_vertexCount = 0;
			m_indexCount = 0;
			m_indexFormat = RHI::IndexFormat::UInt32;
			m_geometryResourceKey = 0;
			m_sourceRevision = 0;
		}
		return released && !IsAllocated();
	}

	// Device側Resource Poolが先に破棄された場合の非Destroy経路。
	void Abandon() noexcept {
		m_vertexBuffer = {};
		m_indexBuffer = {};
		m_nativeVertexBuffer = nullptr;
		m_nativeIndexBuffer = nullptr;
		m_createdFromCpuData = false;
		m_vertexStride = 0;
		m_vertexCount = 0;
		m_indexCount = 0;
		m_indexFormat = RHI::IndexFormat::UInt32;
		m_geometryResourceKey = 0;
		m_sourceRevision = 0;
	}

	bool IsReady() const noexcept {
		const bool sourceReady = m_createdFromCpuData ||
			(m_nativeVertexBuffer != nullptr && m_nativeIndexBuffer != nullptr);
		return static_cast<bool>(m_vertexBuffer) &&
			static_cast<bool>(m_indexBuffer) &&
			sourceReady &&
			m_vertexStride != 0 &&
			m_vertexCount != 0 &&
			m_indexCount != 0 &&
			m_geometryResourceKey != 0;
	}

	bool IsAllocated() const noexcept {
		return static_cast<bool>(m_vertexBuffer) ||
			static_cast<bool>(m_indexBuffer);
	}

	bool Matches(
		const StaticBatchD3D11GeometrySource& source
	) const noexcept {
		if(!IsReady() || !source.IsValid() ||
			source.vertexStride != m_vertexStride ||
			source.vertexCount != m_vertexCount ||
			source.indexCount != m_indexCount ||
			source.indexFormat != m_indexFormat ||
			source.geometryResourceKey != m_geometryResourceKey ||
			source.sourceRevision != m_sourceRevision){
			return false;
		}
		if(source.HasCpuData()){
			return m_createdFromCpuData;
		}
		return !m_createdFromCpuData && source.HasNativeBuffers() &&
			source.vertexBuffer == m_nativeVertexBuffer &&
			source.indexBuffer == m_nativeIndexBuffer;
	}

	RHI::BufferHandle VertexBuffer() const noexcept { return m_vertexBuffer; }
	RHI::BufferHandle IndexBuffer() const noexcept { return m_indexBuffer; }
	std::uint32_t VertexStride() const noexcept { return m_vertexStride; }
	std::uint32_t VertexCount() const noexcept { return m_vertexCount; }
	std::uint32_t IndexCount() const noexcept { return m_indexCount; }
	RHI::IndexFormat IndexFormat() const noexcept { return m_indexFormat; }
	std::uint64_t GeometryResourceKey() const noexcept {
		return m_geometryResourceKey;
	}
	std::uint64_t SourceRevision() const noexcept {
		return m_sourceRevision;
	}
	bool WasCreatedFromCpuData() const noexcept {
		return m_createdFromCpuData;
	}

private:
	bool CreateFromCpuData(
		RHI::IRHIDevice& device,
		const StaticBatchD3D11GeometrySource& source
	){
		if(source.vertexData.size() >
				(static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)())) ||
			source.indexData.size() >
				(static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)()))){
			return false;
		}

		RHI::BufferDesc vertexDesc;
		vertexDesc.byteSize =
			static_cast<std::uint32_t>(source.vertexData.size());
		vertexDesc.stride = source.vertexStride;
		vertexDesc.usage = RHI::ResourceUsage::Immutable;
		vertexDesc.bindFlags = RHI::BufferBindFlags::Vertex;
		vertexDesc.initialState = RHI::ResourceState::VertexBuffer;
		vertexDesc.debugName = "Static Batch CPU Vertex Buffer";
		m_vertexBuffer = device.CreateBuffer(vertexDesc, source.vertexData);
		if(!m_vertexBuffer) return false;

		RHI::BufferDesc indexDesc;
		indexDesc.byteSize =
			static_cast<std::uint32_t>(source.indexData.size());
		indexDesc.stride = source.IndexElementSize();
		indexDesc.usage = RHI::ResourceUsage::Immutable;
		indexDesc.bindFlags = RHI::BufferBindFlags::Index;
		indexDesc.initialState = RHI::ResourceState::IndexBuffer;
		indexDesc.debugName = "Static Batch CPU Index Buffer";
		m_indexBuffer = device.CreateBuffer(indexDesc, source.indexData);
		if(!m_indexBuffer){
			Release(device);
			return false;
		}
		return true;
	}

	bool CreateFromNativeBuffers(
		RHI::IRHIDevice& device,
		const StaticBatchD3D11GeometrySource& source
	){
		auto* d3d11Device = dynamic_cast<RHI::D3D11RHIDevice*>(&device);
		if(!d3d11Device || !source.HasNativeBuffers()) return false;

		m_vertexBuffer = d3d11Device->ImportNativeBuffer(
			source.vertexBuffer,
			source.vertexStride,
			RHI::ResourceState::VertexBuffer,
			"Static Batch Imported Vertex Buffer"
		);
		if(!m_vertexBuffer) return false;

		m_indexBuffer = d3d11Device->ImportNativeBuffer(
			source.indexBuffer,
			source.IndexElementSize(),
			RHI::ResourceState::IndexBuffer,
			"Static Batch Imported Index Buffer"
		);
		if(!m_indexBuffer){
			Release(device);
			return false;
		}
		return true;
	}

	RHI::BufferHandle m_vertexBuffer;
	RHI::BufferHandle m_indexBuffer;
	ID3D11Buffer* m_nativeVertexBuffer = nullptr;
	ID3D11Buffer* m_nativeIndexBuffer = nullptr;
	bool m_createdFromCpuData = false;
	std::uint32_t m_vertexStride = 0;
	std::uint32_t m_vertexCount = 0;
	std::uint32_t m_indexCount = 0;
	RHI::IndexFormat m_indexFormat = RHI::IndexFormat::UInt32;
	std::uint64_t m_geometryResourceKey = 0;
	std::uint64_t m_sourceRevision = 0;
};
