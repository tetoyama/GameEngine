#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

#include "Service/Graphics/RHI/RHIInterfaces.h"
#include "StaticBatchInstanceDataBuffer.h"

struct StaticBatchGpuInstanceBufferTelemetry {
	std::size_t currentInstanceCount = 0;
	std::size_t instanceCapacity = 0;
	std::size_t uploadCount = 0;
	std::size_t reallocationCount = 0;
	std::size_t failedUploadCount = 0;
	std::uint64_t uploadedSourceRevision = 0;
	bool valid = false;
};

class StaticBatchGpuInstanceBuffer {
public:
	static constexpr std::uint32_t InstanceStride =
		static_cast<std::uint32_t>(sizeof(StaticBatchInstanceData));

	bool Synchronize(
		RHI::IRHIDevice& device,
		RHI::IRHICommandList& commandList,
		const StaticBatchInstanceDataBuffer& source
	){
		if(!source.IsValid() || source.IsOverflowed()){
			InvalidateUploadState();
			return false;
		}
		return Upload(
			device,
			commandList,
			source.Instances(),
			source.SourceRevision()
		);
	}

	bool Upload(
		RHI::IRHIDevice& device,
		RHI::IRHICommandList& commandList,
		std::span<const StaticBatchInstanceData> instances,
		std::uint64_t sourceRevision
	){
		if(instances.empty()){
			m_currentInstanceCount = 0;
			m_uploadedSourceRevision = sourceRevision;
			m_valid = true;
			return true;
		}

		if(m_valid && m_buffer &&
			m_uploadedSourceRevision == sourceRevision &&
			m_currentInstanceCount == instances.size()){
			return true;
		}

		if(!EnsureCapacity(device, instances.size())){
			++m_failedUploadCount;
			InvalidateUploadState();
			return false;
		}

		const std::span<const std::byte> bytes = std::as_bytes(instances);
		if(!commandList.UpdateBuffer(m_buffer, bytes, 0)){
			++m_failedUploadCount;
			InvalidateUploadState();
			return false;
		}

		m_currentInstanceCount = instances.size();
		m_uploadedSourceRevision = sourceRevision;
		m_valid = true;
		++m_uploadCount;
		return true;
	}

	bool Bind(RHI::IRHICommandList& commandList, std::uint32_t slot) const {
		return CanSubmit() && commandList.SetVertexBuffer(
			slot,
			m_buffer,
			InstanceStride,
			0
		);
	}

	bool Release(RHI::IRHIDevice& device) noexcept {
		bool released = true;
		if(m_buffer){
			released = device.DestroyBuffer(m_buffer);
		}
		if(released){
			m_buffer = {};
			m_instanceCapacity = 0;
			InvalidateUploadState();
		}
		return released;
	}

	RHI::BufferHandle Buffer() const noexcept { return m_buffer; }
	std::size_t InstanceCount() const noexcept { return m_currentInstanceCount; }
	std::size_t Capacity() const noexcept { return m_instanceCapacity; }
	std::uint64_t UploadedSourceRevision() const noexcept {
		return m_uploadedSourceRevision;
	}
	bool IsValid() const noexcept { return m_valid; }
	bool CanSubmit() const noexcept {
		return m_valid && static_cast<bool>(m_buffer) &&
			m_currentInstanceCount > 0;
	}

	StaticBatchGpuInstanceBufferTelemetry Telemetry() const noexcept {
		return {
			m_currentInstanceCount,
			m_instanceCapacity,
			m_uploadCount,
			m_reallocationCount,
			m_failedUploadCount,
			m_uploadedSourceRevision,
			m_valid
		};
	}

	void ResetMetrics() noexcept {
		m_uploadCount = 0;
		m_reallocationCount = 0;
		m_failedUploadCount = 0;
	}

private:
	static std::size_t ResolveCapacity(
		std::size_t currentCapacity,
		std::size_t requiredCapacity
	) noexcept {
		std::size_t capacity = (std::max)(std::size_t{1}, currentCapacity);
		while(capacity < requiredCapacity){
			if(capacity > (std::numeric_limits<std::size_t>::max)() / 2){
				return requiredCapacity;
			}
			capacity *= 2;
		}
		return capacity;
	}

	bool EnsureCapacity(RHI::IRHIDevice& device, std::size_t requiredCapacity){
		if(m_buffer && requiredCapacity <= m_instanceCapacity){
			return true;
		}

		const std::size_t newCapacity = ResolveCapacity(
			m_instanceCapacity,
			requiredCapacity
		);
		if(newCapacity >
			(static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)()) /
				InstanceStride)){
			return false;
		}

		RHI::BufferDesc desc;
		desc.byteSize = static_cast<std::uint32_t>(newCapacity * InstanceStride);
		desc.stride = InstanceStride;
		desc.usage = RHI::ResourceUsage::Dynamic;
		desc.bindFlags = RHI::BufferBindFlags::Vertex;
		desc.cpuAccess = RHI::CpuAccessFlags::Write;
		desc.initialState = RHI::ResourceState::VertexBuffer;
		desc.debugName = "Static Batch Instance Buffer";

		const RHI::BufferHandle replacement = device.CreateBuffer(desc);
		if(!replacement) return false;

		if(m_buffer && !device.DestroyBuffer(m_buffer)){
			device.DestroyBuffer(replacement);
			return false;
		}

		m_buffer = replacement;
		m_instanceCapacity = newCapacity;
		++m_reallocationCount;
		return true;
	}

	void InvalidateUploadState() noexcept {
		m_currentInstanceCount = 0;
		m_uploadedSourceRevision = 0;
		m_valid = false;
	}

	RHI::BufferHandle m_buffer;
	std::size_t m_currentInstanceCount = 0;
	std::size_t m_instanceCapacity = 0;
	std::size_t m_uploadCount = 0;
	std::size_t m_reallocationCount = 0;
	std::size_t m_failedUploadCount = 0;
	std::uint64_t m_uploadedSourceRevision = 0;
	bool m_valid = false;
};
