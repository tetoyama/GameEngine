#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include <d3d11.h>

#include "Service/Graphics/RHI/RHIDescriptors.h"

// Static Batch Geometry生成元。移行中のNative D3D11 Buffer Importと、
// Backend非依存CPU SnapshotからのRHI Buffer生成を同じ検証契約で扱う。
struct StaticBatchD3D11GeometrySource {
	std::span<const std::byte> vertexData;
	std::span<const std::byte> indexData;
	ID3D11Buffer* vertexBuffer = nullptr;
	ID3D11Buffer* indexBuffer = nullptr;
	std::uint32_t vertexStride = 0;
	std::uint32_t vertexCount = 0;
	std::uint32_t indexCount = 0;
	RHI::IndexFormat indexFormat = RHI::IndexFormat::UInt32;
	std::uint64_t geometryResourceKey = 0;

	std::uint32_t IndexElementSize() const noexcept {
		return indexFormat == RHI::IndexFormat::UInt16
			? static_cast<std::uint32_t>(sizeof(std::uint16_t))
			: static_cast<std::uint32_t>(sizeof(std::uint32_t));
	}

	std::uint64_t RequiredVertexBytes() const noexcept {
		return static_cast<std::uint64_t>(vertexCount) * vertexStride;
	}

	std::uint64_t RequiredIndexBytes() const noexcept {
		return static_cast<std::uint64_t>(indexCount) * IndexElementSize();
	}

	bool HasCpuData() const noexcept {
		return !vertexData.empty() && !indexData.empty() &&
			RequiredVertexBytes() <= vertexData.size() &&
			RequiredIndexBytes() <= indexData.size();
	}

	bool HasNativeBuffers() const noexcept {
		return vertexBuffer != nullptr && indexBuffer != nullptr;
	}

	bool IsValid() const noexcept {
		return vertexStride != 0 &&
			vertexCount != 0 &&
			indexCount != 0 &&
			geometryResourceKey != 0 &&
			(HasCpuData() || HasNativeBuffers());
	}
};
