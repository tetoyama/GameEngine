#pragma once
#include "Shader/common.hlsl"
#include <array>
#include <cstdint>
#include <vector>
#include <filesystem>
#include <memory>
#include <span>
#include "Service/Graphics/RHI/RHIInterfaces.h"
#include "Engine/Scene/System/Render/Model/ModelGeometryRuntimeMesh.h"

namespace Rendering {
// GPU/API independent, owning frame snapshot. Matrices are column-major,
// column-vector, left-handed; clip depth is [0,1]. No ECS pointers survive here.
using Matrix = std::array<float, 16>;
struct Instance {
    Matrix world;
    std::array<float, 4> color{1,1,1,1};
    std::array<float, 4> uvTransform{1,1,0,0};
    std::array<float, 4> shading{}; // x: unlit
    std::array<float, 4> material{0,1,1,1}; // metallic, roughness, AO, flags (shadow=1, environment=2)
    std::array<float, 4> emissive{}; // RGB and intensity
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
    std::array<float,4> cameraPosition{0,0,-10,0}; // w: environment map enabled
};
struct RenderScene { FrameUniforms frame; std::vector<DrawItem> draws; RHI::TextureViewHandle environmentTexture; };


// The renderer knows only RHI contracts. SDL and platform headers live below it.
// Device must outlive the renderer. Render/Resize run on the render thread.
class FrameRenderer final {
public:
    FrameRenderer(RHI::IRHIDevice&,std::filesystem::path shaderDirectory);
    ~FrameRenderer();
    FrameRenderer(const FrameRenderer&)=delete;
    FrameRenderer& operator=(const FrameRenderer&)=delete;
    // Meshes and material textures are borrowed from the existing resource/runtime owners.
    void Resize(uint32_t width,uint32_t height);
    // Offscreen path is identical to the displayed frame. A minimized surface
    // skips presentation while the owning output texture remains capturable.
    void Render(const RenderScene&,bool present=true,bool verticalSync=true);
    bool Capture(RHI::TextureReadback&,uint64_t timeoutNanoseconds=5'000'000'000ull);
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}
