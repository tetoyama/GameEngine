#pragma once

#include "Service/Graphics/RHI/RHIBackend.h"

namespace RHI {

// SDL owns native synchronization and coordinate conversion. The renderer still
// uses the same RHI/RenderGraph contract for D3D12, Vulkan, and Metal.
class SDLGPUBackend final : public IRHIBackend {
public:
    explicit SDLGPUBackend(BackendType type);
    BackendType GetType() const noexcept override;
    std::string_view GetName() const noexcept override;
    bool IsSupported() const override;
    std::vector<AdapterInfo> EnumerateAdapters() const override;
    std::unique_ptr<IRHIDevice> CreateDevice(const DeviceCreateDesc& desc) override;
private:
    BackendType m_type;
};

bool RegisterSDLGPUBackends(BackendRegistry& registry);

} // namespace RHI
