#pragma once

#include <cstdint>

#include "RHIDescriptors.h"

namespace RHI {

struct DeviceCapabilities {
	BackendType backend = BackendType::Null;
	uint32_t maximumColorAttachments = 1;
	uint32_t maximumTextureDimension2D = 1;
	uint32_t maximumConstantBufferSize = 0;
	uint32_t maximumVertexBufferSlots = 0;
	uint32_t maximumShaderResourceSlots = 0;
	bool supportsCompute = false;
	bool supportsGeometryShader = false;
	bool supportsTessellation = false;
	bool supportsIndirectDraw = false;
	bool supportsConservativeRasterization = false;
	bool supportsVariableRateShading = false;
	bool supportsRayTracing = false;
	bool supportsAsyncCompute = false;
	bool supportsMultipleCommandQueues = false;
	bool supportsTimelineSynchronization = false;
};

} // namespace RHI
