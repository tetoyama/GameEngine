#pragma once

#include <cstdint>

#include "RHIDescriptors.h"

// Windows SDK headers expose DeviceCapabilities as an A/W macro. RHI uses the
// name as a backend-independent C++ type, so prevent substitution while the
// canonical type is declared.
#ifdef DeviceCapabilities
#undef DeviceCapabilities
#endif

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

// If a Windows header defines DeviceCapabilities after this header has already
// been consumed, later uses expand to DeviceCapabilitiesA/W. Keep those names
// valid aliases so RHI headers remain include-order independent.
using DeviceCapabilitiesA = DeviceCapabilities;
using DeviceCapabilitiesW = DeviceCapabilities;

} // namespace RHI
