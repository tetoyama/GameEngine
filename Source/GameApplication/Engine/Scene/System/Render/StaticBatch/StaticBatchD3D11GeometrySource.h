#pragma once

#include <cstdint>

#include <d3d11.h>

#include "Service/Graphics/RHI/RHIDescriptors.h"

struct StaticBatchD3D11GeometrySource {
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

	bool IsValid() const noexcept {
		return vertexBuffer != nullptr &&
			indexBuffer != nullptr &&
			vertexStride != 0 &&
			vertexCount != 0 &&
			indexCount != 0 &&
			geometryResourceKey != 0;
	}
};
