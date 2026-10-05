#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include "Engine/Scene/System/Render/Model/ModelGeometryRuntimeMesh.h"

namespace Rendering {
// GPU/API independent, owning frame snapshot. Matrices are column-major,
// column-vector, left-handed; clip depth is [0,1]. No ECS pointers survive here.
using Matrix = std::array<float, 16>;
struct Vertex { std::array<float, 3> position, normal; };
struct Instance { Matrix world; std::array<float, 4> color{1,1,1,1}; };
struct DrawItem { ModelGeometryRuntimeMesh mesh; Instance instance; bool castsShadow = true; };
struct FrameUniforms { Matrix viewProjection, lightViewProjection; std::array<float,4> lightDirection; };
struct RenderScene { FrameUniforms frame; std::vector<DrawItem> draws; uint64_t generation = 0; };
struct RenderStatistics { uint32_t instances = 0, batches = 0, shadowDraws = 0, geometryDraws = 0, passes = 0; };
}
