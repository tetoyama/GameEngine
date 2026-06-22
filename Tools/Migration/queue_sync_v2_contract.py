from queue_sync_v2_common import load, replace_once, save

path = "Source/GameApplication/Service/Graphics/RHI/RHIDescriptors.h"
text = load(path)
text = replace_once(
    text,
    """enum class BackendType : uint8_t {
\tNull,
\tDirect3D11,
\tDirect3D12,
\tVulkan
};
""",
    """enum class BackendType : uint8_t {
\tNull,
\tDirect3D11,
\tDirect3D12,
\tVulkan
};

enum class CommandQueueType : uint8_t {
\tGraphics,
\tCompute,
\tCopy
};

enum class ResourceQueueSharingMode : uint8_t {
\tExclusive,
\tConcurrent
};
""",
    "queue and resource sharing enums",
)
text = replace_once(
    text,
    """\tResourceState initialState = ResourceState::Common;
\tstd::string debugName;
};

struct TextureDesc {
""",
    """\tResourceState initialState = ResourceState::Common;
\tstd::string debugName;
\tResourceQueueSharingMode queueSharingMode = ResourceQueueSharingMode::Exclusive;
};

struct TextureDesc {
""",
    "buffer queue sharing descriptor",
)
text = replace_once(
    text,
    """\tbool generateMips = false;
\tResourceState initialState = ResourceState::Common;
\tstd::string debugName;
};

struct BufferViewDesc {
""",
    """\tbool generateMips = false;
\tResourceState initialState = ResourceState::Common;
\tstd::string debugName;
\tResourceQueueSharingMode queueSharingMode = ResourceQueueSharingMode::Exclusive;
};

struct BufferViewDesc {
""",
    "texture queue sharing descriptor",
)
save(path, text)

path = "Source/GameApplication/Service/Graphics/RHI/RHIExecution.h"
text = load(path)
text = replace_once(
    text,
    """enum class CommandQueueType : uint8_t {
\tGraphics,
\tCompute,
\tCopy
};

""",
    "",
    "move CommandQueueType to descriptors",
)
text = replace_once(
    text,
    """struct QueueSubmitDesc {
\tstd::span<IRHICommandList* const> commandLists;
""",
    """// After Submit returns, callers may release command-list wrappers.
// Backends retain native submission state until GPU completion.
// Fence states are Device-owned until DestroyFence or Device destruction.
struct QueueSubmitDesc {
\tstd::span<IRHICommandList* const> commandLists;
""",
    "queue submission lifetime contract",
)
save(path, text)

path = "Source/GameApplication/Service/Graphics/RHI/RHIInterfaces.h"
text = load(path)
text = replace_once(
    text,
    """\tvirtual std::unique_ptr<IRHICommandList> CreateCommandList(const CommandListCreateDesc& desc) = 0;
\tvirtual std::unique_ptr<IRHIFence> CreateFence(uint64_t initialValue = 0) = 0;
\tvirtual IRHISwapChain* GetSwapChain() = 0;
""",
    """\tvirtual std::unique_ptr<IRHICommandList> CreateCommandList(const CommandListCreateDesc& desc) = 0;
\tvirtual std::unique_ptr<IRHIFence> CreateFence(uint64_t initialValue = 0) = 0;
\tvirtual bool DestroyFence(FenceHandle handle) = 0;
\tvirtual IRHISwapChain* GetSwapChain() = 0;
""",
    "IRHIDevice fence destruction contract",
)
save(path, text)

path = "Source/GameApplication/Service/Graphics/RHI/Null/NullRHIBackend.h"
text = load(path)
text = replace_once(
    text,
    """\tstd::unique_ptr<IRHIFence> CreateFence(uint64_t value) override { auto state = std::make_shared<NullFenceState>(); state->completedValue = value; auto handle = m_fences.Create(state); return std::make_unique<NullRHIFence>(handle, std::move(state)); }
\tIRHISwapChain* GetSwapChain() override { return &m_swapChain; }
""",
    """\tstd::unique_ptr<IRHIFence> CreateFence(uint64_t value) override { auto state = std::make_shared<NullFenceState>(); state->completedValue = value; auto handle = m_fences.Create(state); return std::make_unique<NullRHIFence>(handle, std::move(state)); }
\tbool DestroyFence(FenceHandle handle) override { return m_fences.Destroy(handle); }
\tIRHISwapChain* GetSwapChain() override { return &m_swapChain; }
""",
    "Null fence destruction",
)
save(path, text)

path = "Source/GameApplication/Service/Graphics/RHI/D3D11/D3D11RHIDevice.h"
text = load(path)
text = replace_once(
    text,
    """\tstd::unique_ptr<IRHICommandList> CreateCommandList(const CommandListCreateDesc&) override;
\tstd::unique_ptr<IRHIFence> CreateFence(uint64_t) override;
\tIRHISwapChain* GetSwapChain() override { return &m_swapChain; }
""",
    """\tstd::unique_ptr<IRHICommandList> CreateCommandList(const CommandListCreateDesc&) override;
\tstd::unique_ptr<IRHIFence> CreateFence(uint64_t) override;
\tbool DestroyFence(FenceHandle) override;
\tIRHISwapChain* GetSwapChain() override { return &m_swapChain; }
""",
    "D3D11 fence destruction declaration",
)
save(path, text)

path = "Source/GameApplication/Service/Graphics/RHI/D3D11/D3D11RHIDeviceRuntime.h"
text = load(path)
needle = """\treturn std::make_unique<D3D11RHIFence>(handle, std::move(state));
}

inline std::shared_ptr<D3D11FenceState> D3D11RHIDevice::FindFence"""
replacement = """\treturn std::make_unique<D3D11RHIFence>(handle, std::move(state));
}

inline bool D3D11RHIDevice::DestroyFence(FenceHandle handle){
\treturn m_fences.Destroy(handle);
}

inline std::shared_ptr<D3D11FenceState> D3D11RHIDevice::FindFence"""
text = replace_once(text, needle, replacement, "D3D11 fence destruction implementation")
save(path, text)

print("Queue sync public contract hardened")
