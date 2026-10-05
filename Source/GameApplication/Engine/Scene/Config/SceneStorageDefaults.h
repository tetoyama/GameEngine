#pragma once
#include <cstdint>
// Capacity policy is independent from its YAML persistence adapter.
struct SceneStorageDefaults {
    static constexpr uint32_t DirectPagedPageSize = 256;
    static constexpr uint32_t DefaultExpectedEntityCount = 1024;
    static constexpr uint32_t DefaultExpectedTransformCount = 768;
    static constexpr uint32_t DefaultExpectedRenderableCount = 512;
    static constexpr uint32_t DefaultExpectedCullingCount = 512;
    static constexpr uint32_t DefaultExpectedStaticEntityCount = 256;
    static constexpr uint32_t DefaultRenderPacketReserve = 512;
    static constexpr uint32_t DefaultVisibleEntityReserve = 512;
    static constexpr uint32_t DefaultStaticBatchReserve = 64;
};
