#pragma once

#include <cstdint>

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

		auto* d3d11Device = dynamic_cast<RHI::D3D11RHIDevice*>(&device);
		if(!d3d11Device) return false;

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

		const RHI::BufferDesc* vertexDesc =
			device.GetBufferDesc(m_vertexBuffer);
		const RHI::BufferDesc* indexDesc =
			device.GetBufferDesc(m_indexBuffer);
		if(!vertexDesc || !indexDesc){
			Release(device);
			return false;
		}

		const std::uint64_t requiredVertexBytes =
			static_cast<std::uint64_t>(source.vertexCount) *
			source.vertexStride;
		const std::uint64_t requiredIndexBytes =
			static_cast<std::uint64_t>(source.indexCount) *
			source.IndexElementSize();
		if(requiredVertexBytes > vertexDesc->byteSize ||
			requiredIndexBytes > indexDesc->byteSize){
			Release(device);
			return false;
		}

		m_nativeVertexBuffer = source.vertexBuffer;
		m_nativeIndexBuffer = source.indexBuffer;
		m_vertexStride = source.vertexStride;
		m_vertexCount = source.vertexCount;
		m_indexCount = source.indexCount;
		m_indexFormat = source.indexFormat;
		m_geometryResourceKey = source.geometryResourceKey;
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
			m_vertexStride = 0;
			m_vertexCount = 0;
			m_indexCount = 0;
			m_indexFormat = RHI::IndexFormat::UInt32;
			m_geometryResourceKey = 0;
		}
		return released && !IsAllocated();
	}

	bool IsReady() const noexcept {
		return static_cast<bool>(m_vertexBuffer) &&
			static_cast<bool>(m_indexBuffer) &&
			m_nativeVertexBuffer != nullptr &&
			m_nativeIndexBuffer != nullptr &&
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
		return IsReady() && source.IsValid() &&
			source.vertexBuffer == m_nativeVertexBuffer &&
			source.indexBuffer == m_nativeIndexBuffer &&
			source.vertexStride == m_vertexStride &&
			source.vertexCount == m_vertexCount &&
			source.indexCount == m_indexCount &&
			source.indexFormat == m_indexFormat &&
			source.geometryResourceKey == m_geometryResourceKey;
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

private:
	RHI::BufferHandle m_vertexBuffer;
	RHI::BufferHandle m_indexBuffer;
	ID3D11Buffer* m_nativeVertexBuffer = nullptr;
	ID3D11Buffer* m_nativeIndexBuffer = nullptr;
	std::uint32_t m_vertexStride = 0;
	std::uint32_t m_vertexCount = 0;
	std::uint32_t m_indexCount = 0;
	RHI::IndexFormat m_indexFormat = RHI::IndexFormat::UInt32;
	std::uint64_t m_geometryResourceKey = 0;
};
