#pragma once

#include <cstdint>
#include <limits>

#include "Scene/Component/meshRendererComponent.h"
#include "Service/Graphics/RHI/D3D11/D3D11RHIDevice.h"
#include "Shader/common.hlsl"

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

	bool Create(RHI::IRHIDevice& device, const MeshData& mesh){
		if(IsAllocated() || device.GetBackendType() != RHI::BackendType::Direct3D11){
			return false;
		}
		if(!mesh.m_VertexBuffer || !mesh.m_IndexBuffer ||
			mesh.meshCount <= 0 || mesh.indexCount <= 0 ||
			mesh.geometryResourceKey == 0){
			return false;
		}

		auto& d3d11Device = static_cast<RHI::D3D11RHIDevice&>(device);
		m_vertexBuffer = d3d11Device.ImportNativeBuffer(
			mesh.m_VertexBuffer.Get(),
			static_cast<std::uint32_t>(sizeof(VERTEX_3D)),
			RHI::ResourceState::VertexBuffer,
			"Static Batch Imported Vertex Buffer"
		);
		if(!m_vertexBuffer) return false;

		m_indexBuffer = d3d11Device.ImportNativeBuffer(
			mesh.m_IndexBuffer.Get(),
			static_cast<std::uint32_t>(sizeof(std::uint32_t)),
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
			static_cast<std::uint64_t>(mesh.meshCount) * sizeof(VERTEX_3D);
		const std::uint64_t requiredIndexBytes =
			static_cast<std::uint64_t>(mesh.indexCount) * sizeof(std::uint32_t);
		if(requiredVertexBytes > vertexDesc->byteSize ||
			requiredIndexBytes > indexDesc->byteSize ||
			static_cast<std::uint64_t>(mesh.indexCount) >
				(std::numeric_limits<std::uint32_t>::max)()){
			Release(device);
			return false;
		}

		m_vertexStride = static_cast<std::uint32_t>(sizeof(VERTEX_3D));
		m_indexCount = static_cast<std::uint32_t>(mesh.indexCount);
		m_geometryResourceKey = mesh.geometryResourceKey;
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
			RHI::IndexFormat::UInt32,
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
			m_vertexStride = 0;
			m_indexCount = 0;
			m_geometryResourceKey = 0;
		}
		return released && !IsAllocated();
	}

	bool IsReady() const noexcept {
		return static_cast<bool>(m_vertexBuffer) &&
			static_cast<bool>(m_indexBuffer) &&
			m_vertexStride != 0 &&
			m_indexCount != 0 &&
			m_geometryResourceKey != 0;
	}

	bool IsAllocated() const noexcept {
		return static_cast<bool>(m_vertexBuffer) ||
			static_cast<bool>(m_indexBuffer);
	}

	bool Matches(const MeshData& mesh) const noexcept {
		return IsReady() &&
			mesh.geometryResourceKey == m_geometryResourceKey &&
			mesh.indexCount > 0 &&
			static_cast<std::uint32_t>(mesh.indexCount) == m_indexCount;
	}

	RHI::BufferHandle VertexBuffer() const noexcept { return m_vertexBuffer; }
	RHI::BufferHandle IndexBuffer() const noexcept { return m_indexBuffer; }
	std::uint32_t VertexStride() const noexcept { return m_vertexStride; }
	std::uint32_t IndexCount() const noexcept { return m_indexCount; }
	std::uint64_t GeometryResourceKey() const noexcept {
		return m_geometryResourceKey;
	}

private:
	RHI::BufferHandle m_vertexBuffer;
	RHI::BufferHandle m_indexBuffer;
	std::uint32_t m_vertexStride = 0;
	std::uint32_t m_indexCount = 0;
	std::uint64_t m_geometryResourceKey = 0;
};
