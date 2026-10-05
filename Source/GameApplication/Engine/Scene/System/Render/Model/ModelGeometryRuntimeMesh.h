#pragma once
#include <cstdint>
#include "Service/Graphics/RHI/RHIDescriptors.h"

// The existing runtime geometry binding, separated from ModelData/importers so
// render snapshots and portable renderers can reuse it without native headers.
struct ModelGeometryRuntimeMesh {
    RHI::BufferHandle vertexBuffer;
    RHI::BufferHandle indexBuffer;
    std::uint32_t vertexStride = 0;
    std::uint32_t vertexCount = 0;
    std::uint32_t indexCount = 0;
    RHI::IndexFormat indexFormat = RHI::IndexFormat::UInt32;
    bool IsReady() const noexcept {
        return static_cast<bool>(vertexBuffer) && static_cast<bool>(indexBuffer) &&
            vertexStride != 0 && vertexCount != 0 && indexCount != 0;
    }
};
