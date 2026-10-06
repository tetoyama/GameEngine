#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include "Engine/Scene/System/Render/Model/ModelGeometryRuntimeMesh.h"

namespace Rendering {
// GPU/API independent, owning frame snapshot. Matrices are column-major,
// column-vector, left-handed; clip depth is [0,1]. No ECS pointers survive here.
using Matrix = std::array<float, 16>;
struct Vertex { std::array<float, 3> position, normal; std::array<float,2> uv{}; };
struct Instance {
    Matrix world;
    std::array<float, 4> color{1,1,1,1};
    std::array<float, 4> uvTransform{1,1,0,0};
    std::array<float, 4> shading{}; // x: unlit
};
struct DrawItem { ModelGeometryRuntimeMesh mesh; Instance instance; bool castsShadow = true; RHI::TextureViewHandle albedoTexture; };
struct FrameUniforms {
    Matrix viewProjection, lightViewProjection;
    std::array<float,4> lightDirection;
    std::array<float,4> lightColor{1.8f,1.8f,1.8f,1}; // w: cast shadow
    std::array<float,4> ambientColor{.17f,.17f,.17f,0};
    // Diagnostic runtime uses ACES + gamma; the existing Editor without
    // camera post effects publishes linear color to its UNORM viewport.
    std::array<float,4> outputTransform{1,0,0,0}; // x: ACES + gamma enabled
};
struct RenderScene { FrameUniforms frame; std::vector<DrawItem> draws; uint64_t generation = 0; };
struct RenderStatistics { uint32_t instances = 0, batches = 0, shadowDraws = 0, geometryDraws = 0, passes = 0; };
}
