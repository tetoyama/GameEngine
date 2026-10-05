#pragma once
#include "RenderScene.h"
#include "Service/Graphics/RHI/RHIInterfaces.h"
#include <filesystem>
#include <memory>
#include <span>
namespace Rendering {
// The renderer knows only RHI contracts. SDL and platform headers live below it.
// Device must outlive the renderer. Render/Resize/upload run on the render thread.
class FrameRenderer final {
public:
    FrameRenderer(RHI::IRHIDevice&,std::filesystem::path shaderDirectory);
    ~FrameRenderer();
    FrameRenderer(const FrameRenderer&)=delete;
    FrameRenderer& operator=(const FrameRenderer&)=delete;
    // Convenience upload for generated meshes. Existing ModelGeometryRuntime
    // buffers can be passed directly in DrawItem without re-upload or a cache.
    ModelGeometryRuntimeMesh UploadMesh(std::span<const Vertex>,std::span<const uint32_t>);
    bool RemoveMesh(const ModelGeometryRuntimeMesh&);
    void Resize(uint32_t width,uint32_t height);
    // Offscreen path is identical to the displayed frame. A minimized surface
    // skips presentation while the owning output texture remains capturable.
    void Render(const RenderScene&,bool present=true,bool verticalSync=true);
    bool Capture(RHI::TextureReadback&,uint64_t timeoutNanoseconds=5'000'000'000ull);
    RHI::TextureHandle Output() const;
    uint32_t Width() const;
    uint32_t Height() const;
    const RenderStatistics& Statistics() const;
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}
