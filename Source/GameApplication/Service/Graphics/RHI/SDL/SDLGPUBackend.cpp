#include "SDLGPUBackend.h"
#include "Service/Graphics/RHI/RHIResourcePool.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <limits>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>

namespace RHI {
namespace {

const char* Driver(BackendType type) {
    switch (type) {
    case BackendType::Direct3D12: return "direct3d12";
    case BackendType::Vulkan: return "vulkan";
    case BackendType::Metal: return "metal";
    default: return nullptr;
    }
}
SDL_GPUShaderFormat ShaderFormat(BackendType type) {
    switch (type) {
    case BackendType::Direct3D12: return SDL_GPU_SHADERFORMAT_DXIL;
    case BackendType::Vulkan: return SDL_GPU_SHADERFORMAT_SPIRV;
    case BackendType::Metal: return SDL_GPU_SHADERFORMAT_MSL;
    default: return SDL_GPU_SHADERFORMAT_INVALID;
    }
}
SDL_GPUTextureFormat TextureFormat(Format format) {
    switch (format) {
    case Format::R8_UNorm: return SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    case Format::RG8_UNorm: return SDL_GPU_TEXTUREFORMAT_R8G8_UNORM;
    case Format::RGBA8_UNorm: return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    case Format::BGRA8_UNorm: return SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    case Format::R16_UInt: return SDL_GPU_TEXTUREFORMAT_R16_UINT;
    case Format::R32_UInt: return SDL_GPU_TEXTUREFORMAT_R32_UINT;
    case Format::R16_Float: return SDL_GPU_TEXTUREFORMAT_R16_FLOAT;
    case Format::R32_Float: return SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
    case Format::RG16_Float: return SDL_GPU_TEXTUREFORMAT_R16G16_FLOAT;
    case Format::RG32_Float: return SDL_GPU_TEXTUREFORMAT_R32G32_FLOAT;
    case Format::RGBA16_Float: return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    case Format::RGBA32_Float: return SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    case Format::RGBA32_UInt: return SDL_GPU_TEXTUREFORMAT_R32G32B32A32_UINT;
    case Format::D24_UNorm_S8_UInt: return SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
    case Format::D32_Float: return SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    case Format::D32_Float_S8X24_UInt: return SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT;
    default: return SDL_GPU_TEXTUREFORMAT_INVALID;
    }
}
Format RHIFormat(SDL_GPUTextureFormat format) {
    if (format == SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM) return Format::RGBA8_UNorm;
    if (format == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM) return Format::BGRA8_UNorm;
    return Format::Unknown;
}
uint32_t FormatSize(Format format) {
    switch (format) {
    case Format::R8_UNorm: return 1;
    case Format::RG8_UNorm: case Format::R16_UInt: case Format::R16_Float: return 2;
    case Format::RGBA8_UNorm: case Format::BGRA8_UNorm: case Format::R32_UInt:
    case Format::R32_Float: case Format::RG16_Float: case Format::D32_Float: return 4;
    case Format::RG32_Float: case Format::RGBA16_Float: return 8;
    case Format::RGB32_Float: return 12;
    case Format::RGBA32_Float: case Format::RGBA32_UInt: return 16;
    default: return 0;
    }
}
SDL_GPUVertexElementFormat VertexFormat(Format format) {
    switch (format) {
    case Format::R32_Float: return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT;
    case Format::RG32_Float: return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    case Format::RGB32_Float: return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    case Format::RGBA32_Float: return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    case Format::RGBA8_UNorm: return SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM;
    case Format::R32_UInt: return SDL_GPU_VERTEXELEMENTFORMAT_UINT;
    case Format::RGBA32_UInt: return SDL_GPU_VERTEXELEMENTFORMAT_UINT4;
    default: return SDL_GPU_VERTEXELEMENTFORMAT_INVALID;
    }
}
SDL_GPUCompareOp Compare(ComparisonFunc value) {
    switch (value) {
    case ComparisonFunc::Never: return SDL_GPU_COMPAREOP_NEVER;
    case ComparisonFunc::Less: return SDL_GPU_COMPAREOP_LESS;
    case ComparisonFunc::Equal: return SDL_GPU_COMPAREOP_EQUAL;
    case ComparisonFunc::LessEqual: return SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    case ComparisonFunc::Greater: return SDL_GPU_COMPAREOP_GREATER;
    case ComparisonFunc::NotEqual: return SDL_GPU_COMPAREOP_NOT_EQUAL;
    case ComparisonFunc::GreaterEqual: return SDL_GPU_COMPAREOP_GREATER_OR_EQUAL;
    default: return SDL_GPU_COMPAREOP_ALWAYS;
    }
}
SDL_GPUBlendFactor Blend(BlendFactor value) {
    switch (value) {
    case BlendFactor::Zero: return SDL_GPU_BLENDFACTOR_ZERO;
    case BlendFactor::One: return SDL_GPU_BLENDFACTOR_ONE;
    case BlendFactor::SourceColor: return SDL_GPU_BLENDFACTOR_SRC_COLOR;
    case BlendFactor::InverseSourceColor: return SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR;
    case BlendFactor::SourceAlpha: return SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    case BlendFactor::InverseSourceAlpha: return SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    case BlendFactor::DestinationColor: return SDL_GPU_BLENDFACTOR_DST_COLOR;
    case BlendFactor::InverseDestinationColor: return SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_COLOR;
    case BlendFactor::DestinationAlpha: return SDL_GPU_BLENDFACTOR_DST_ALPHA;
    default: return SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_ALPHA;
    }
}
SDL_GPUBlendOp BlendOp(BlendOperation value) {
    switch (value) {
    case BlendOperation::Add: return SDL_GPU_BLENDOP_ADD;
    case BlendOperation::Subtract: return SDL_GPU_BLENDOP_SUBTRACT;
    case BlendOperation::ReverseSubtract: return SDL_GPU_BLENDOP_REVERSE_SUBTRACT;
    case BlendOperation::Minimum: return SDL_GPU_BLENDOP_MIN;
    default: return SDL_GPU_BLENDOP_MAX;
    }
}
SDL_GPULoadOp LoadOp(LoadOperation value) {
    switch (value) {
    case LoadOperation::Load: return SDL_GPU_LOADOP_LOAD;
    case LoadOperation::Clear: return SDL_GPU_LOADOP_CLEAR;
    default: return SDL_GPU_LOADOP_DONT_CARE;
    }
}
SDL_GPUStoreOp StoreOp(StoreOperation value) {
    return value == StoreOperation::Store ? SDL_GPU_STOREOP_STORE : SDL_GPU_STOREOP_DONT_CARE;
}
SDL_GPUSamplerAddressMode Address(SamplerAddressMode value) {
    switch (value) {
    case SamplerAddressMode::Repeat: return SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    case SamplerAddressMode::MirroredRepeat: return SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT;
    default: return SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    }
}
bool Error(const char* message) { SDL_SetError("GameEngine RHI: %s", message); return false; }

struct Buffer {
    BufferDesc desc;
    std::shared_ptr<SDL_GPUBuffer> native;
    std::vector<std::byte> uniform;
    ResourceState state;
};
struct Texture {
    TextureDesc desc;
    std::shared_ptr<SDL_GPUTexture> native;
    SDL_GPUTexture* swapImage = nullptr; // Borrowed until submission, never released.
    bool swapchain = false;
    ResourceState state;
    SDL_GPUTexture* Get() const { return swapchain ? swapImage : native.get(); }
};
struct BufferView { BufferViewDesc desc; };
struct TextureView { TextureViewDesc desc; };
struct Sampler { SamplerDesc desc; std::shared_ptr<SDL_GPUSampler> native; };
struct Shader {
    ShaderDesc desc;
    std::shared_ptr<SDL_GPUShader> native;
    std::vector<std::byte> computeCode;
    SDL_GPUShaderFormat format;
};
struct Pipeline {
    PipelineStateDesc desc;
    std::shared_ptr<SDL_GPUGraphicsPipeline> graphics;
    std::shared_ptr<SDL_GPUComputePipeline> compute;
    std::vector<VertexBufferLayoutDesc> layouts;
    std::array<ShaderDesc, 3> shaderDescriptions;
};
struct FenceState {
    uint64_t completed = 0;
    uint64_t submitted = 0;
    std::vector<std::pair<uint64_t, std::shared_ptr<SDL_GPUFence>>> pending;
};

class GPUDevice;
class CommandList final : public IRHICommandList {
public:
    explicit CommandList(GPUDevice& device);
    ~CommandList() override;
    CommandQueueType GetQueueType() const noexcept override { return CommandQueueType::Graphics; }
    void Begin() override;
    void End() override;
    bool ResourceBarrier(std::span<const ResourceBarrierDesc>) override;
    bool BeginRenderPass(const RenderPassDesc&) override;
    void EndRenderPass() override;
    bool SetPipelineState(PipelineStateHandle) override;
    void SetViewport(const Viewport&) override;
    bool SetVertexBuffer(uint32_t, BufferHandle, uint32_t, uint32_t) override;
    bool SetIndexBuffer(BufferHandle, IndexFormat, uint32_t) override;
    bool SetConstantBuffer(ShaderStage, uint32_t, BufferHandle) override;
    bool SetBufferView(ShaderStage, uint32_t, BufferViewHandle) override;
    bool SetTextureView(ShaderStage, uint32_t, TextureViewHandle) override;
    bool SetSampler(ShaderStage, uint32_t, SamplerHandle) override;
    bool UpdateBuffer(BufferHandle, std::span<const std::byte>, uint32_t) override;
    void Draw(uint32_t, uint32_t) override;
    void DrawIndexed(uint32_t, uint32_t, int32_t) override;
    bool DrawIndexedInstanced(uint32_t, uint32_t, uint32_t, int32_t, uint32_t) override;
    void Dispatch(uint32_t, uint32_t, uint32_t) override;
    bool AcquireImage();
    bool Ready() const { return !m_lifetime.expired() && m_command && !m_closed && m_thread == std::this_thread::get_id(); }
    bool IsClosed() const { return !m_lifetime.expired() && m_command && m_closed && m_thread == std::this_thread::get_id(); }
    GPUDevice& Device() { return m_device; }
    SDL_GPUCommandBuffer* ReleaseForSubmit();
private:
    bool BindResources(ShaderStage);
    bool CanDraw();
    void Abandon();
    GPUDevice& m_device;
    IRHIDevice::LifetimeToken m_lifetime;
    SDL_GPUCommandBuffer* m_command = nullptr;
    SDL_GPURenderPass* m_pass = nullptr;
    PipelineStateHandle m_pipeline;
    bool m_closed = false;
    bool m_acquired = false;
    bool m_indexBound = false;
    uint32_t m_indexCapacity = 0;
    std::array<bool,16> m_vertexBound{};
    std::array<std::array<bool,4>,3> m_uniformBound{};
    std::vector<Format> m_colorFormats;
    Format m_depthFormat = Format::Unknown;
    std::thread::id m_thread;
    std::array<std::array<TextureViewHandle, 16>, 3> m_textures{};
    std::array<std::array<SamplerHandle, 16>, 3> m_samplers{};
    std::array<std::array<BufferViewHandle, 16>, 3> m_buffers{};
};
class SwapChain final : public IRHISwapChain {
public:
    explicit SwapChain(GPUDevice& device) : m_device(device) {}
    bool Resize(uint32_t width, uint32_t height) override;
    bool Present(bool verticalSync) override;
    const SwapChainDesc& GetDesc() const override { return desc; }
    // SDL rotates the actual images internally. RHI exposes one frame-acquired
    // logical surface, not a fabricated array of native swapchain images.
    uint32_t GetImageCount() const noexcept override { return 1; }
    uint32_t GetCurrentImageIndex() const noexcept override { return 0; }
    TextureHandle GetImage(uint32_t index) const noexcept override { return index == 0 ? image : TextureHandle{}; }
    CommandQueueType GetPresentQueueType() const noexcept override { return CommandQueueType::Graphics; }
    bool AcquireNextImage(IRHICommandList& list) override;
    SwapChainDesc desc;
    TextureHandle image;
private:
    GPUDevice& m_device;
    std::optional<SDL_GPUPresentMode> m_presentMode;
};
class Queue final : public IRHICommandQueue {
public:
    explicit Queue(GPUDevice& device) : m_device(device) {}
    CommandQueueType GetType() const noexcept override { return CommandQueueType::Graphics; }
    bool Submit(const QueueSubmitDesc&) override;
    bool Present(IRHISwapChain& chain, bool vsync) override;
    void WaitIdle() override;
private:
    GPUDevice& m_device;
};
class Fence final : public IRHIFence {
public:
    Fence(GPUDevice& device, FenceHandle handle);
    FenceHandle GetHandle() const noexcept override { return m_handle; }
    uint64_t GetCompletedValue() const override;
    bool Wait(uint64_t, uint64_t) override;
private:
    GPUDevice& m_device;
    FenceHandle m_handle;
    IRHIDevice::LifetimeToken m_lifetime;
};

class GPUDevice final : public IRHIDevice {
public:
    GPUDevice(SDL_GPUDevice* native, SDL_Window* window, BackendType backend)
        : gpu(native), window(window), m_backend(backend), m_queue(*this), m_swap(*this) {
        m_capabilities.backend = backend;
        m_capabilities.maximumColorAttachments = 8;
        m_capabilities.maximumTextureDimension2D = 4096;
        m_capabilities.maximumConstantBufferSize = 16384;
        m_capabilities.maximumVertexBufferSlots = 16;
        m_capabilities.maximumShaderResourceSlots = 16;
        m_capabilities.supportsCompute = true;
        // Geometry/tessellation/multiple explicit queues are deliberately not
        // advertised: SDL's portable feature set does not implement those APIs.
    }
    ~GPUDevice() override {
        SDL_WaitForGPUIdle(gpu);
        fences.Clear(); m_inflight.clear(); pipelines.Clear(); shaders.Clear();
        samplers.Clear(); textureViews.Clear(); bufferViews.Clear(); textures.Clear(); buffers.Clear();
        if (m_claimed) SDL_ReleaseWindowFromGPUDevice(gpu, window);
        SDL_DestroyGPUDevice(gpu);
    }
    bool InitializeSwap(const SwapChainDesc& requested) {
        if (!window) return true;
        if (!SDL_ClaimWindowForGPUDevice(gpu, window)) return false;
        m_claimed = true;
        m_swap.desc = requested;
        m_swap.desc.format = RHIFormat(SDL_GetGPUSwapchainTextureFormat(gpu, window));
        if (m_swap.desc.format == Format::Unknown) return Error("Unsupported swapchain format");
        TextureDesc desc;
        desc.width = requested.width; desc.height = requested.height;
        desc.format = m_swap.desc.format; desc.bindFlags = TextureBindFlags::RenderTarget;
        desc.initialState = ResourceState::Present;
        m_swap.image = textures.Create(Texture{desc, {}, nullptr, true, ResourceState::Present});
        return true;
    }
    BackendType GetBackendType() const noexcept override { return m_backend; }
    const DeviceCapabilities& GetCapabilities() const noexcept override { return m_capabilities; }
    BufferHandle CreateBuffer(const BufferDesc&, std::span<const std::byte>) override;
    TextureHandle CreateTexture(const TextureDesc&, std::span<const std::byte>, uint32_t) override;
    BufferViewHandle CreateBufferView(const BufferViewDesc&) override;
    TextureViewHandle CreateTextureView(const TextureViewDesc&) override;
    SamplerHandle CreateSampler(const SamplerDesc&) override;
    ShaderHandle CreateShader(const ShaderDesc&, std::span<const std::byte>) override;
    PipelineStateHandle CreatePipelineState(const PipelineStateDesc&) override;
    bool DestroyBuffer(BufferHandle h) override { return buffers.Destroy(h); }
    bool DestroyTexture(TextureHandle h) override { auto* t = textures.TryGet(h); return t && !t->swapchain && textures.Destroy(h); }
    bool DestroyBufferView(BufferViewHandle h) override { return bufferViews.Destroy(h); }
    bool DestroyTextureView(TextureViewHandle h) override { return textureViews.Destroy(h); }
    bool DestroySampler(SamplerHandle h) override { return samplers.Destroy(h); }
    bool DestroyShader(ShaderHandle h) override { return shaders.Destroy(h); }
    bool DestroyPipelineState(PipelineStateHandle h) override { return pipelines.Destroy(h); }
    const BufferDesc* GetBufferDesc(BufferHandle h) const override { auto* p = buffers.TryGet(h); return p ? &p->desc : nullptr; }
    const TextureDesc* GetTextureDesc(TextureHandle h) const override { auto* p = textures.TryGet(h); return p ? &p->desc : nullptr; }
    const BufferViewDesc* GetBufferViewDesc(BufferViewHandle h) const override { auto* p = bufferViews.TryGet(h); return p ? &p->desc : nullptr; }
    const TextureViewDesc* GetTextureViewDesc(TextureViewHandle h) const override { auto* p = textureViews.TryGet(h); return p ? &p->desc : nullptr; }
    const SamplerDesc* GetSamplerDesc(SamplerHandle h) const override { auto* p = samplers.TryGet(h); return p ? &p->desc : nullptr; }
    const ShaderDesc* GetShaderDesc(ShaderHandle h) const override { auto* p = shaders.TryGet(h); return p ? &p->desc : nullptr; }
    const PipelineStateDesc* GetPipelineStateDesc(PipelineStateHandle h) const override { auto* p = pipelines.TryGet(h); return p ? &p->desc : nullptr; }
    IRHICommandQueue* GetQueue(CommandQueueType type) override { return type == CommandQueueType::Graphics ? &m_queue : nullptr; }
    const IRHICommandQueue* GetQueue(CommandQueueType type) const override { return type == CommandQueueType::Graphics ? &m_queue : nullptr; }
    std::unique_ptr<IRHICommandList> CreateCommandList(const CommandListCreateDesc& desc) override {
        if (desc.queueType != CommandQueueType::Graphics || desc.secondary) return nullptr;
        return std::make_unique<CommandList>(*this);
    }
    std::unique_ptr<IRHIFence> CreateFence(uint64_t value) override {
        auto h = fences.Create(FenceState{value, value, {}});
        return std::make_unique<Fence>(*this, h);
    }
    bool DestroyFence(FenceHandle h) override { return fences.Destroy(h); }
    IRHISwapChain* GetSwapChain() override { return m_claimed ? &m_swap : nullptr; }
    const IRHISwapChain* GetSwapChain() const override { return m_claimed ? &m_swap : nullptr; }
    void WaitIdle() override { SDL_WaitForGPUIdle(gpu); m_inflight.clear(); }
    bool ReadTexture(TextureHandle, TextureReadback&, uint64_t) override;
    bool WaitFence(FenceHandle, uint64_t, uint64_t);
    uint64_t Completed(FenceHandle);
    bool Upload(Buffer& buffer, std::span<const std::byte> data, uint32_t offset, SDL_GPUCommandBuffer* command);
    bool Upload(Texture& texture, std::span<const std::byte> data, uint32_t rowPitch);
    std::shared_ptr<SDL_GPUFence> Submit(SDL_GPUCommandBuffer* command) {
        SDL_GPUFence* native = SDL_SubmitGPUCommandBufferAndAcquireFence(command);
        if (!native) return {};
        auto result = std::shared_ptr<SDL_GPUFence>(native, [d = gpu](auto* p) { SDL_ReleaseGPUFence(d, p); });
        std::erase_if(m_inflight, [this](const auto& f) { return SDL_QueryGPUFence(gpu, f.get()); });
        m_inflight.push_back(result);
        return result;
    }
    SDL_GPUDevice* gpu;
    SDL_Window* window;
    ResourcePool<BufferHandle, Buffer> buffers;
    ResourcePool<TextureHandle, Texture> textures;
    ResourcePool<BufferViewHandle, BufferView> bufferViews;
    ResourcePool<TextureViewHandle, TextureView> textureViews;
    ResourcePool<SamplerHandle, Sampler> samplers;
    ResourcePool<ShaderHandle, Shader> shaders;
    ResourcePool<PipelineStateHandle, Pipeline> pipelines;
    ResourcePool<FenceHandle, FenceState> fences;
private:
    BackendType m_backend;
    DeviceCapabilities m_capabilities;
    Queue m_queue;
    SwapChain m_swap;
    bool m_claimed = false;
    std::vector<std::shared_ptr<SDL_GPUFence>> m_inflight;
};

BufferHandle GPUDevice::CreateBuffer(const BufferDesc& desc, std::span<const std::byte> initial) {
    if (!desc.byteSize || initial.size() > desc.byteSize || desc.usage == ResourceUsage::Staging) return {};
    Buffer result{desc, {}, {}, desc.initialState};
    if (HasAnyFlag(desc.bindFlags, BufferBindFlags::Constant)) {
        if (desc.byteSize > m_capabilities.maximumConstantBufferSize || desc.byteSize % 16 != 0 ||
            desc.bindFlags != BufferBindFlags::Constant) return {};
        result.uniform.resize(desc.byteSize);
        std::copy(initial.begin(), initial.end(), result.uniform.begin());
        return buffers.Create(std::move(result));
    }
    SDL_GPUBufferCreateInfo info{};
    info.size = desc.byteSize;
    if (HasAnyFlag(desc.bindFlags, BufferBindFlags::Vertex)) info.usage |= SDL_GPU_BUFFERUSAGE_VERTEX;
    if (HasAnyFlag(desc.bindFlags, BufferBindFlags::Index)) info.usage |= SDL_GPU_BUFFERUSAGE_INDEX;
    if (HasAnyFlag(desc.bindFlags, BufferBindFlags::ShaderResource)) info.usage |= SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
    if (HasAnyFlag(desc.bindFlags, BufferBindFlags::UnorderedAccess)) info.usage |= SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
    if (HasAnyFlag(desc.bindFlags, BufferBindFlags::IndirectArguments)) info.usage |= SDL_GPU_BUFFERUSAGE_INDIRECT;
    if (!info.usage) return {};
    auto* native = SDL_CreateGPUBuffer(gpu, &info);
    if (!native) return {};
    result.native = {native, [d = gpu](auto* p) { SDL_ReleaseGPUBuffer(d, p); }};
    if (!desc.debugName.empty()) SDL_SetGPUBufferName(gpu, native, desc.debugName.c_str());
    if (!initial.empty()) {
        auto* command = SDL_AcquireGPUCommandBuffer(gpu);
        if (!command) return {};
        if (!Upload(result, initial, 0, command)) { SDL_CancelGPUCommandBuffer(command); return {}; }
        if (!Submit(command)) return {};
    }
    return buffers.Create(std::move(result));
}
bool GPUDevice::Upload(Buffer& buffer, std::span<const std::byte> data, uint32_t offset, SDL_GPUCommandBuffer* command) {
    if (!buffer.native || data.empty() || data.size() > buffer.desc.byteSize || offset > buffer.desc.byteSize - data.size()) return false;
    SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, static_cast<uint32_t>(data.size()), 0};
    auto* transfer = SDL_CreateGPUTransferBuffer(gpu, &info);
    if (!transfer) return false;
    void* mapped = SDL_MapGPUTransferBuffer(gpu, transfer, false);
    if (!mapped) { SDL_ReleaseGPUTransferBuffer(gpu, transfer); return false; }
    std::memcpy(mapped, data.data(), data.size()); SDL_UnmapGPUTransferBuffer(gpu, transfer);
    auto* copy = SDL_BeginGPUCopyPass(command);
    if (!copy) { SDL_ReleaseGPUTransferBuffer(gpu, transfer); return false; }
    SDL_GPUTransferBufferLocation src{transfer, 0};
    SDL_GPUBufferRegion dst{buffer.native.get(), offset, static_cast<uint32_t>(data.size())};
    // Preserve untouched bytes for partial updates. SDL cycle would discard them.
    SDL_UploadToGPUBuffer(copy, &src, &dst, offset == 0 && data.size() == buffer.desc.byteSize);
    SDL_EndGPUCopyPass(copy); SDL_ReleaseGPUTransferBuffer(gpu, transfer);
    return true;
}
TextureHandle GPUDevice::CreateTexture(const TextureDesc& desc, std::span<const std::byte> initial, uint32_t rowPitch) {
    if (!desc.width || !desc.height || desc.width > m_capabilities.maximumTextureDimension2D || desc.height > m_capabilities.maximumTextureDimension2D || !desc.depth || !desc.arraySize || !desc.mipLevels ||
        desc.depth != 1 || desc.sampleCount != 1 || desc.usage == ResourceUsage::Staging || desc.generateMips) return {};
    SDL_GPUTextureCreateInfo info{};
    info.type = desc.arraySize > 1 ? SDL_GPU_TEXTURETYPE_2D_ARRAY : SDL_GPU_TEXTURETYPE_2D;
    info.format = TextureFormat(desc.format);
    info.width = desc.width; info.height = desc.height; info.layer_count_or_depth = desc.arraySize;
    info.num_levels = desc.mipLevels; info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    if (info.format == SDL_GPU_TEXTUREFORMAT_INVALID) return {};
    if (HasAnyFlag(desc.bindFlags, TextureBindFlags::ShaderResource)) info.usage |= SDL_GPU_TEXTUREUSAGE_SAMPLER;
    if (HasAnyFlag(desc.bindFlags, TextureBindFlags::RenderTarget)) info.usage |= SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    if (HasAnyFlag(desc.bindFlags, TextureBindFlags::DepthStencil)) info.usage |= SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    if (HasAnyFlag(desc.bindFlags, TextureBindFlags::UnorderedAccess)) info.usage |= SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE;
    if (!info.usage || !SDL_GPUTextureSupportsFormat(gpu, info.format, info.type, info.usage)) return {};
    auto* native = SDL_CreateGPUTexture(gpu, &info);
    if (!native) return {};
    Texture result{desc, {native, [d = gpu](auto* p) { SDL_ReleaseGPUTexture(d, p); }}, nullptr, false, desc.initialState};
    if (!desc.debugName.empty()) SDL_SetGPUTextureName(gpu, native, desc.debugName.c_str());
    if (!initial.empty() && !Upload(result, initial, rowPitch)) return {};
    return textures.Create(std::move(result));
}
bool GPUDevice::Upload(Texture& texture, std::span<const std::byte> data, uint32_t rowPitch) {
    const auto& desc = texture.desc;
    uint32_t size = FormatSize(desc.format);
    if (!size || desc.arraySize != 1 || desc.mipLevels != 1 || desc.width > UINT32_MAX / size) return false;
    if (!rowPitch) rowPitch = desc.width * size;
    const uint64_t bytes = uint64_t(rowPitch) * desc.height;
    if (rowPitch < desc.width * size || rowPitch % size || bytes > UINT32_MAX || data.size() < bytes) return false;
    SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, static_cast<uint32_t>(bytes), 0};
    auto* transfer = SDL_CreateGPUTransferBuffer(gpu, &info);
    if (!transfer) return false;
    void* mapped = SDL_MapGPUTransferBuffer(gpu, transfer, false);
    if (!mapped) { SDL_ReleaseGPUTransferBuffer(gpu, transfer); return false; }
    std::memcpy(mapped, data.data(), static_cast<size_t>(bytes)); SDL_UnmapGPUTransferBuffer(gpu, transfer);
    auto* command = SDL_AcquireGPUCommandBuffer(gpu);
    if (!command) { SDL_ReleaseGPUTransferBuffer(gpu, transfer); return false; }
    auto* copy = SDL_BeginGPUCopyPass(command);
    SDL_GPUTextureTransferInfo src{transfer, 0, rowPitch / size, desc.height};
    SDL_GPUTextureRegion dst{}; dst.texture = texture.Get(); dst.w = desc.width; dst.h = desc.height; dst.d = 1;
    SDL_UploadToGPUTexture(copy, &src, &dst, false); SDL_EndGPUCopyPass(copy);
    auto fence = Submit(command); SDL_ReleaseGPUTransferBuffer(gpu, transfer);
    return static_cast<bool>(fence);
}
BufferViewHandle GPUDevice::CreateBufferView(const BufferViewDesc& desc) {
    const auto* buffer = buffers.TryGet(desc.buffer);
    if (!buffer || !buffer->native || !buffer->desc.structured || !buffer->desc.stride || desc.format != Format::Unknown ||
        desc.firstElement != 0 || desc.elementCount != buffer->desc.byteSize / buffer->desc.stride) return {};
    const auto flag = desc.type == BufferViewType::ShaderResource ? BufferBindFlags::ShaderResource : BufferBindFlags::UnorderedAccess;
    return HasAnyFlag(buffer->desc.bindFlags, flag) ? bufferViews.Create(BufferView{desc}) : BufferViewHandle{};
}
TextureViewHandle GPUDevice::CreateTextureView(const TextureViewDesc& desc) {
    const auto* texture = textures.TryGet(desc.texture);
    if (!texture || !desc.mipLevelCount || !desc.arrayLayerCount ||
        uint32_t(desc.baseMipLevel) + desc.mipLevelCount > texture->desc.mipLevels ||
        uint32_t(desc.baseArrayLayer) + desc.arrayLayerCount > texture->desc.arraySize ||
        (desc.format != Format::Unknown && desc.format != texture->desc.format)) return {};
    const auto flag = desc.type == TextureViewType::ShaderResource ? TextureBindFlags::ShaderResource :
        desc.type == TextureViewType::RenderTarget ? TextureBindFlags::RenderTarget :
        desc.type == TextureViewType::DepthStencil ? TextureBindFlags::DepthStencil : TextureBindFlags::UnorderedAccess;
    if (!HasAnyFlag(texture->desc.bindFlags, flag)) return {};
    // Sampling/storage binding uses the complete SDL texture. Never silently
    // reinterpret a restricted RHI view as a full-resource view.
    if ((desc.type == TextureViewType::ShaderResource || desc.type == TextureViewType::UnorderedAccess) &&
        (desc.baseMipLevel || desc.baseArrayLayer || desc.mipLevelCount != texture->desc.mipLevels || desc.arrayLayerCount != texture->desc.arraySize)) return {};
    if ((desc.type == TextureViewType::RenderTarget || desc.type == TextureViewType::DepthStencil) &&
        (desc.mipLevelCount != 1 || desc.arrayLayerCount != 1)) return {};
    return textureViews.Create(TextureView{desc});
}
SamplerHandle GPUDevice::CreateSampler(const SamplerDesc& desc) {
    if (desc.addressU == SamplerAddressMode::ClampToBorder || desc.addressV == SamplerAddressMode::ClampToBorder ||
        desc.addressW == SamplerAddressMode::ClampToBorder || desc.mipLodBias != 0) return {};
    SDL_GPUSamplerCreateInfo info{};
    info.min_filter = desc.minFilter == FilterMode::Linear ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;
    info.mag_filter = desc.magFilter == FilterMode::Linear ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;
    info.mipmap_mode = desc.mipmapMode == MipmapMode::Linear ? SDL_GPU_SAMPLERMIPMAPMODE_LINEAR : SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    info.address_mode_u = Address(desc.addressU); info.address_mode_v = Address(desc.addressV); info.address_mode_w = Address(desc.addressW);
    info.min_lod = desc.minLod; info.max_lod = desc.maxLod;
    info.enable_anisotropy = desc.anisotropyEnable; info.max_anisotropy = desc.maxAnisotropy;
    info.enable_compare = desc.comparisonEnable; info.compare_op = Compare(desc.comparisonFunction);
    auto* native = SDL_CreateGPUSampler(gpu, &info);
    if (!native) return {};
    return samplers.Create(Sampler{desc, {native, [d = gpu](auto* p) { SDL_ReleaseGPUSampler(d, p); }}});
}
ShaderHandle GPUDevice::CreateShader(const ShaderDesc& desc, std::span<const std::byte> bytes) {
    if (bytes.empty() || desc.entryPoint.empty() || desc.sampledTextures > 16 || desc.storageBuffers > 8 ||
        desc.storageTextures > 8 || desc.uniformBuffers > 4 || desc.writableStorageBuffers > 8 || desc.writableStorageTextures > 8 ||
        desc.sampledTextures + desc.storageTextures + desc.writableStorageTextures > 16) return {};
    const auto expected = m_backend == BackendType::Vulkan ? ShaderDesc::CodeFormat::SPIRV :
        m_backend == BackendType::Direct3D12 ? ShaderDesc::CodeFormat::DXIL : ShaderDesc::CodeFormat::MetalSource;
    if (desc.codeFormat != expected && desc.codeFormat != ShaderDesc::CodeFormat::BackendNative) return {};
    if (static_cast<size_t>(desc.stage) >= 3) return {};
    if (m_backend == BackendType::Vulkan) {
        uint32_t magic = 0;
        if (bytes.size() < 20 || bytes.size() % 4) return {};
        std::memcpy(&magic, bytes.data(), sizeof(magic));
        if (magic != 0x07230203u) return {};
    }
    if (m_backend == BackendType::Metal && bytes.back() != std::byte{0}) return {};
    Shader result{desc, {}, {}, ShaderFormat(m_backend)};
    if (desc.stage == ShaderStage::Compute) {
        if (!desc.threadGroupSize[0] || !desc.threadGroupSize[1] || !desc.threadGroupSize[2] ||
            uint64_t(desc.threadGroupSize[0])*desc.threadGroupSize[1]*desc.threadGroupSize[2] > 1024) return {};
        result.computeCode.assign(bytes.begin(), bytes.end());
    } else {
        if (desc.writableStorageBuffers || desc.writableStorageTextures || desc.storageTextures) return {};
        SDL_GPUShaderCreateInfo info{};
        info.code = reinterpret_cast<const Uint8*>(bytes.data()); info.code_size = bytes.size();
        info.entrypoint = desc.entryPoint.c_str(); info.format = result.format;
        info.stage = desc.stage == ShaderStage::Vertex ? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT;
        info.num_samplers = desc.sampledTextures; info.num_storage_textures = desc.storageTextures;
        info.num_storage_buffers = desc.storageBuffers; info.num_uniform_buffers = desc.uniformBuffers;
        auto* native = SDL_CreateGPUShader(gpu, &info);
        if (!native) return {};
        result.native = {native, [d = gpu](auto* p) { SDL_ReleaseGPUShader(d, p); }};
    }
    return shaders.Create(std::move(result));
}
PipelineStateHandle GPUDevice::CreatePipelineState(const PipelineStateDesc& desc) {
    Pipeline result{desc, {}, {}, {}, {}};
    if (desc.computeShader) {
        auto* shader = shaders.TryGet(desc.computeShader);
        if (!shader || shader->desc.stage != ShaderStage::Compute || desc.vertexShader || desc.pixelShader) return {};
        result.shaderDescriptions[2] = shader->desc;
        SDL_GPUComputePipelineCreateInfo info{};
        info.code = reinterpret_cast<const Uint8*>(shader->computeCode.data()); info.code_size = shader->computeCode.size();
        info.entrypoint = shader->desc.entryPoint.c_str(); info.format = shader->format;
        info.num_samplers = shader->desc.sampledTextures; info.num_readonly_storage_textures = shader->desc.storageTextures;
        info.num_readonly_storage_buffers = shader->desc.storageBuffers;
        info.num_readwrite_storage_textures = shader->desc.writableStorageTextures;
        info.num_readwrite_storage_buffers = shader->desc.writableStorageBuffers;
        info.num_uniform_buffers = shader->desc.uniformBuffers;
        info.threadcount_x = shader->desc.threadGroupSize[0]; info.threadcount_y = shader->desc.threadGroupSize[1]; info.threadcount_z = shader->desc.threadGroupSize[2];
        auto* native = SDL_CreateGPUComputePipeline(gpu, &info);
        if (!native) return {};
        result.compute = {native, [d = gpu](auto* p) { SDL_ReleaseGPUComputePipeline(d, p); }};
        return pipelines.Create(std::move(result));
    }
    auto* vertex = shaders.TryGet(desc.vertexShader); auto* fragment = shaders.TryGet(desc.pixelShader);
    if (!vertex || !fragment || vertex->desc.stage != ShaderStage::Vertex || fragment->desc.stage != ShaderStage::Pixel ||
        desc.renderTargets.colorAttachmentCount > 8 || desc.renderTargets.sampleCount != 1 || desc.blend.alphaToCoverageEnable || desc.rasterizer.scissorEnable) return {};
    result.shaderDescriptions[0] = vertex->desc;
    result.shaderDescriptions[1] = fragment->desc;
    std::vector<SDL_GPUVertexAttribute> attributes;
    std::vector<SDL_GPUVertexBufferDescription> layouts;
    std::array<uint32_t, 16> strides{};
    std::array<bool, 16> used{}, perInstance{};
    std::array<bool, 32> locations{};
    for (uint32_t index = 0; index < desc.inputLayout.size(); ++index) {
        const auto& item = desc.inputLayout[index];
        uint32_t location = item.location == AllSubresources ? index : item.location;
        auto format = VertexFormat(item.format); uint32_t size = FormatSize(item.format);
        if (item.inputSlot >= 16 || location >= locations.size() || locations[location] ||
            format == SDL_GPU_VERTEXELEMENTFORMAT_INVALID || item.alignedByteOffset > UINT32_MAX - size ||
            (item.perInstance && item.instanceStepRate != 1)) return {};
        if (used[item.inputSlot] && perInstance[item.inputSlot] != item.perInstance) return {};
        used[item.inputSlot] = true; perInstance[item.inputSlot] = item.perInstance; locations[location] = true;
        strides[item.inputSlot] = std::max(strides[item.inputSlot], item.alignedByteOffset + size);
        attributes.push_back({location, item.inputSlot, format, item.alignedByteOffset});
    }
    for (const auto& layout : desc.vertexBuffers) {
        if (layout.slot >= 16 || !used[layout.slot] || layout.stride < strides[layout.slot] || perInstance[layout.slot] != layout.perInstance) return {};
        strides[layout.slot] = layout.stride;
    }
    for (uint32_t slot = 0; slot < 16; ++slot) if (used[slot]) {
        layouts.push_back({slot, strides[slot], perInstance[slot] ? SDL_GPU_VERTEXINPUTRATE_INSTANCE : SDL_GPU_VERTEXINPUTRATE_VERTEX, 0});
        result.layouts.push_back({slot, strides[slot], perInstance[slot]});
    }
    std::vector<SDL_GPUColorTargetDescription> targets;
    for (uint32_t i = 0; i < desc.renderTargets.colorAttachmentCount; ++i) {
        SDL_GPUColorTargetDescription target{}; target.format = TextureFormat(desc.renderTargets.colorFormats[i]);
        if (target.format == SDL_GPU_TEXTUREFORMAT_INVALID) return {};
        const auto& blend = desc.blend.targets[desc.blend.independentBlendEnable ? i : 0];
        auto& state = target.blend_state;
        state.enable_blend = blend.blendEnable; state.enable_color_write_mask = true; state.color_write_mask = blend.writeMask;
        state.src_color_blendfactor = Blend(blend.sourceColor); state.dst_color_blendfactor = Blend(blend.destinationColor); state.color_blend_op = BlendOp(blend.colorOperation);
        state.src_alpha_blendfactor = Blend(blend.sourceAlpha); state.dst_alpha_blendfactor = Blend(blend.destinationAlpha); state.alpha_blend_op = BlendOp(blend.alphaOperation);
        targets.push_back(target);
    }
    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = vertex->native.get(); info.fragment_shader = fragment->native.get();
    info.vertex_input_state = {layouts.data(), static_cast<uint32_t>(layouts.size()), attributes.data(), static_cast<uint32_t>(attributes.size())};
    switch (desc.topology) {
    case PrimitiveTopology::TriangleList: info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST; break;
    case PrimitiveTopology::TriangleStrip: info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP; break;
    case PrimitiveTopology::LineList: info.primitive_type = SDL_GPU_PRIMITIVETYPE_LINELIST; break;
    case PrimitiveTopology::LineStrip: info.primitive_type = SDL_GPU_PRIMITIVETYPE_LINESTRIP; break;
    case PrimitiveTopology::PointList: info.primitive_type = SDL_GPU_PRIMITIVETYPE_POINTLIST; break;
    default: return {};
    }
    info.rasterizer_state.fill_mode = desc.rasterizer.fillMode == FillMode::Solid ? SDL_GPU_FILLMODE_FILL : SDL_GPU_FILLMODE_LINE;
    info.rasterizer_state.cull_mode = desc.rasterizer.cullMode == CullMode::None ? SDL_GPU_CULLMODE_NONE : desc.rasterizer.cullMode == CullMode::Back ? SDL_GPU_CULLMODE_BACK : SDL_GPU_CULLMODE_FRONT;
    info.rasterizer_state.front_face = desc.rasterizer.frontCounterClockwise ? SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE : SDL_GPU_FRONTFACE_CLOCKWISE;
    info.rasterizer_state.enable_depth_clip = desc.rasterizer.depthClipEnable;
    info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    info.depth_stencil_state.enable_depth_test = desc.depthStencil.depthEnable;
    info.depth_stencil_state.enable_depth_write = desc.depthStencil.depthWriteEnable;
    info.depth_stencil_state.compare_op = Compare(desc.depthStencil.depthFunction);
    info.target_info.color_target_descriptions = targets.data(); info.target_info.num_color_targets = static_cast<uint32_t>(targets.size());
    info.target_info.has_depth_stencil_target = desc.renderTargets.depthStencilFormat != Format::Unknown;
    info.target_info.depth_stencil_format = TextureFormat(desc.renderTargets.depthStencilFormat);
    if (desc.depthStencil.depthEnable && !info.target_info.has_depth_stencil_target) return {};
    auto* native = SDL_CreateGPUGraphicsPipeline(gpu, &info);
    if (!native) return {};
    result.graphics = {native, [d = gpu](auto* p) { SDL_ReleaseGPUGraphicsPipeline(d, p); }};
    return pipelines.Create(std::move(result));
}

CommandList::CommandList(GPUDevice& device) : m_device(device), m_lifetime(device.GetLifetimeToken()) {}
CommandList::~CommandList() { Abandon(); }
void CommandList::Abandon() {
    if (!m_command || m_lifetime.expired()) return;
    if (std::this_thread::get_id() != m_thread) std::terminate();
    if (m_pass) SDL_EndGPURenderPass(m_pass);
    // SDL forbids cancellation after acquiring a presentation image.
    if (m_acquired) SDL_SubmitGPUCommandBuffer(m_command); else SDL_CancelGPUCommandBuffer(m_command);
    m_command = nullptr; m_pass = nullptr;
}
void CommandList::Begin() {
    if (m_lifetime.expired()) throw std::runtime_error("RHI device expired");
    Abandon();
    m_command = SDL_AcquireGPUCommandBuffer(m_device.gpu);
    if (!m_command) throw std::runtime_error(SDL_GetError());
    m_thread = std::this_thread::get_id(); m_closed = false; m_acquired = false; m_indexBound = false;
    m_pipeline = {}; m_textures = {}; m_samplers = {}; m_buffers = {}; m_vertexBound = {}; m_uniformBound = {};
}
void CommandList::End() { if (!Ready()) throw std::runtime_error("Invalid RHI command-list end"); EndRenderPass(); m_closed = true; }
SDL_GPUCommandBuffer* CommandList::ReleaseForSubmit() {
    if (!IsClosed() || std::this_thread::get_id() != m_thread) return nullptr;
    m_acquired = false; return std::exchange(m_command, nullptr);
}
bool CommandList::AcquireImage() {
    if (!Ready() || m_pass || m_acquired || std::this_thread::get_id() != m_thread || !m_device.window) return false;
    auto* swap = m_device.GetSwapChain(); auto* texture = m_device.textures.TryGet(swap->GetCurrentImage());
    Uint32 width = 0, height = 0; SDL_GPUTexture* image = nullptr;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(m_command, m_device.window, &image, &width, &height)) return false;
    texture->swapImage = image;
    if (!image) return false;
    texture->desc.width = width; texture->desc.height = height; m_acquired = true;
    return true;
}
bool CommandList::ResourceBarrier(std::span<const ResourceBarrierDesc> barriers) {
    if (!Ready() || m_pass) return false;
    // SDL inserts native barriers. Validate the public resource contract before
    // translating logical transitions into its automatically synchronized API.
    for (const auto& barrier : barriers) {
        if (!barrier.HasExactlyOneResource() || barrier.after == ResourceState::Undefined) return false;
        if (barrier.buffer) {
            auto* buffer = m_device.buffers.TryGet(barrier.buffer);
            if (!buffer || barrier.subresource != AllSubresources) return false;
        } else {
            auto* texture = m_device.textures.TryGet(barrier.texture);
            if (!texture || (barrier.subresource != AllSubresources && barrier.subresource >= uint32_t(texture->desc.mipLevels) * texture->desc.arraySize)) return false;
        }
    }
    for (const auto& barrier : barriers) {
        if (barrier.buffer) m_device.buffers.TryGet(barrier.buffer)->state = barrier.after;
        else m_device.textures.TryGet(barrier.texture)->state = barrier.after;
    }
    return true;
}
bool CommandList::BeginRenderPass(const RenderPassDesc& desc) {
    if (!Ready() || m_pass || desc.colorAttachments.size() > 8 || (desc.colorAttachments.empty() && !desc.hasDepthAttachment)) return false;
    std::vector<SDL_GPUColorTargetInfo> colors;
    std::vector<Format> formats;
    for (const auto& attachment : desc.colorAttachments) {
        auto* view = m_device.textureViews.TryGet(attachment.view);
        if (!view || view->desc.type != TextureViewType::RenderTarget) return false;
        auto* texture = m_device.textures.TryGet(view->desc.texture);
        if (!texture || !texture->Get() || (texture->swapchain && !m_acquired)) return false;
        SDL_GPUColorTargetInfo info{};
        info.texture = texture->Get(); info.mip_level = view->desc.baseMipLevel; info.layer_or_depth_plane = view->desc.baseArrayLayer;
        info.load_op = LoadOp(attachment.loadOperation); info.store_op = StoreOp(attachment.storeOperation);
        info.clear_color = {attachment.clearColor[0], attachment.clearColor[1], attachment.clearColor[2], attachment.clearColor[3]};
        colors.push_back(info);
        formats.push_back(texture->desc.format);
    }
    SDL_GPUDepthStencilTargetInfo depth{};
    Format depthFormat = Format::Unknown;
    if (desc.hasDepthAttachment) {
        auto* view = m_device.textureViews.TryGet(desc.depthAttachment.view);
        if (!view || view->desc.type != TextureViewType::DepthStencil || view->desc.baseArrayLayer > 255 || view->desc.baseMipLevel > 255) return false;
        auto* texture = m_device.textures.TryGet(view->desc.texture);
        if (!texture || !texture->Get()) return false;
        depthFormat = texture->desc.format;
        depth.texture = texture->Get(); depth.mip_level = static_cast<uint8_t>(view->desc.baseMipLevel); depth.layer = static_cast<uint8_t>(view->desc.baseArrayLayer);
        depth.clear_depth = desc.depthAttachment.clearDepth; depth.clear_stencil = desc.depthAttachment.clearStencil;
        depth.load_op = LoadOp(desc.depthAttachment.depthLoadOperation); depth.store_op = StoreOp(desc.depthAttachment.depthStoreOperation);
        depth.stencil_load_op = LoadOp(desc.depthAttachment.stencilLoadOperation); depth.stencil_store_op = StoreOp(desc.depthAttachment.stencilStoreOperation);
    }
    m_pass = SDL_BeginGPURenderPass(m_command, colors.data(), static_cast<uint32_t>(colors.size()), desc.hasDepthAttachment ? &depth : nullptr);
    m_pipeline = {}; m_indexBound = false;
    m_vertexBound = {}; m_colorFormats = std::move(formats); m_depthFormat = depthFormat;
    return m_pass != nullptr;
}
void CommandList::EndRenderPass() { if (m_pass && !m_lifetime.expired()) SDL_EndGPURenderPass(std::exchange(m_pass, nullptr)); m_pipeline = {}; }
bool CommandList::SetPipelineState(PipelineStateHandle h) {
    if (!Ready()) return false;
    auto* pipeline = m_device.pipelines.TryGet(h);
    if (!pipeline || (m_pass && !pipeline->graphics) || (!m_pass && !pipeline->compute)) return false;
    if (m_pass) {
        const auto& targets = pipeline->desc.renderTargets;
        if (targets.colorAttachmentCount != m_colorFormats.size() || targets.depthStencilFormat != m_depthFormat) return false;
        for (size_t i = 0; i < m_colorFormats.size(); ++i) if (targets.colorFormats[i] != m_colorFormats[i]) return false;
    }
    m_pipeline = h;
    if (m_pass) SDL_BindGPUGraphicsPipeline(m_pass, pipeline->graphics.get());
    return true;
}
void CommandList::SetViewport(const Viewport& viewport) {
    if (!Ready() || !m_pass || viewport.width <= 0 || viewport.height <= 0) throw std::runtime_error("Invalid RHI viewport");
    SDL_GPUViewport native{viewport.x, viewport.y, viewport.width, viewport.height, viewport.minDepth, viewport.maxDepth};
    SDL_SetGPUViewport(m_pass, &native);
}
bool CommandList::SetVertexBuffer(uint32_t slot, BufferHandle h, uint32_t stride, uint32_t offset) {
    if (!Ready() || !m_pass || slot >= 16) return false;
    auto* buffer = m_device.buffers.TryGet(h); auto* pipeline = m_device.pipelines.TryGet(m_pipeline);
    if (!buffer || !buffer->native || !pipeline || offset >= buffer->desc.byteSize || !HasAnyFlag(buffer->desc.bindFlags, BufferBindFlags::Vertex)) return false;
    auto layout = std::find_if(pipeline->layouts.begin(), pipeline->layouts.end(), [slot](const auto& l) { return l.slot == slot; });
    if (layout == pipeline->layouts.end() || layout->stride != stride) return false;
    SDL_GPUBufferBinding binding{buffer->native.get(), offset}; SDL_BindGPUVertexBuffers(m_pass, slot, &binding, 1); m_vertexBound[slot] = true; return true;
}
bool CommandList::SetIndexBuffer(BufferHandle h, IndexFormat format, uint32_t offset) {
    if (!Ready() || !m_pass) return false;
    auto* buffer = m_device.buffers.TryGet(h);
    if (!buffer || !buffer->native || offset >= buffer->desc.byteSize || !HasAnyFlag(buffer->desc.bindFlags, BufferBindFlags::Index) || offset % (format == IndexFormat::UInt16 ? 2 : 4)) return false;
    SDL_GPUBufferBinding binding{buffer->native.get(), offset};
    SDL_BindGPUIndexBuffer(m_pass, &binding, format == IndexFormat::UInt16 ? SDL_GPU_INDEXELEMENTSIZE_16BIT : SDL_GPU_INDEXELEMENTSIZE_32BIT);
    m_indexBound = true; m_indexCapacity = (buffer->desc.byteSize-offset)/(format == IndexFormat::UInt16 ? 2 : 4); return true;
}
bool CommandList::SetConstantBuffer(ShaderStage stage, uint32_t slot, BufferHandle h) {
    if (!Ready() || slot >= 4 || static_cast<size_t>(stage) >= 3) return false;
    auto* buffer = m_device.buffers.TryGet(h);
    if (!buffer || buffer->uniform.empty()) return false;
    const uint32_t size = static_cast<uint32_t>(buffer->uniform.size());
    switch (stage) {
    case ShaderStage::Vertex: SDL_PushGPUVertexUniformData(m_command, slot, buffer->uniform.data(), size); break;
    case ShaderStage::Pixel: SDL_PushGPUFragmentUniformData(m_command, slot, buffer->uniform.data(), size); break;
    case ShaderStage::Compute: SDL_PushGPUComputeUniformData(m_command, slot, buffer->uniform.data(), size); break;
    }
    m_uniformBound[static_cast<size_t>(stage)][slot] = true;
    return true;
}
bool CommandList::SetBufferView(ShaderStage stage, uint32_t slot, BufferViewHandle h) {
    if (!Ready() || slot >= 16 || static_cast<size_t>(stage) >= 3 || !m_device.bufferViews.TryGet(h)) return false;
    m_buffers[static_cast<size_t>(stage)][slot] = h; return true;
}
bool CommandList::SetTextureView(ShaderStage stage, uint32_t slot, TextureViewHandle h) {
    if (!Ready() || slot >= 16 || static_cast<size_t>(stage) >= 3 || !m_device.textureViews.TryGet(h)) return false;
    m_textures[static_cast<size_t>(stage)][slot] = h; return true;
}
bool CommandList::SetSampler(ShaderStage stage, uint32_t slot, SamplerHandle h) {
    if (!Ready() || slot >= 16 || static_cast<size_t>(stage) >= 3 || !m_device.samplers.TryGet(h)) return false;
    m_samplers[static_cast<size_t>(stage)][slot] = h; return true;
}
bool CommandList::UpdateBuffer(BufferHandle h, std::span<const std::byte> data, uint32_t offset) {
    if (!Ready()) return false;
    auto* buffer = m_device.buffers.TryGet(h);
    if (!buffer || data.size() > buffer->desc.byteSize || offset > buffer->desc.byteSize - data.size()) return false;
    if (!buffer->uniform.empty()) {
        std::copy(data.begin(), data.end(), buffer->uniform.begin() + offset); return true;
    }
    if (m_pass || buffer->desc.usage == ResourceUsage::Immutable) return false;
    return data.empty() || m_device.Upload(*buffer, data, offset, m_command);
}
bool CommandList::BindResources(ShaderStage stage) {
    auto* pipeline = m_device.pipelines.TryGet(m_pipeline);
    if (!pipeline) return false;
    const size_t index = static_cast<size_t>(stage);
    const auto& shader = pipeline->shaderDescriptions[index];
    for (uint32_t slot = 0; slot < shader.uniformBuffers; ++slot) if (!m_uniformBound[index][slot]) return false;
    std::vector<SDL_GPUTextureSamplerBinding> bindings;
    for (uint32_t slot = 0; slot < shader.sampledTextures; ++slot) {
        auto* view = m_device.textureViews.TryGet(m_textures[index][slot]); auto* sampler = m_device.samplers.TryGet(m_samplers[index][slot]);
        if (!view || !sampler || view->desc.type != TextureViewType::ShaderResource) return false;
        auto* texture = m_device.textures.TryGet(view->desc.texture);
        if (!texture || !texture->Get() || texture->swapchain) return false;
        bindings.push_back({texture->Get(), sampler->native.get()});
    }
    if (!bindings.empty()) {
        if (stage == ShaderStage::Vertex) SDL_BindGPUVertexSamplers(m_pass, 0, bindings.data(), static_cast<uint32_t>(bindings.size()));
        else if (stage == ShaderStage::Pixel) SDL_BindGPUFragmentSamplers(m_pass, 0, bindings.data(), static_cast<uint32_t>(bindings.size()));
        // Compute resources are bound after BeginGPUComputePass in Dispatch.
    }
    if (stage != ShaderStage::Compute) {
        std::vector<SDL_GPUBuffer*> storage;
        for (uint32_t slot = 0; slot < shader.storageBuffers; ++slot) {
            auto* view = m_device.bufferViews.TryGet(m_buffers[index][slot]);
            if (!view || view->desc.type != BufferViewType::ShaderResource) return false;
            auto* buffer = m_device.buffers.TryGet(view->desc.buffer);
            if (!buffer || !buffer->native) return false;
            storage.push_back(buffer->native.get());
        }
        if (!storage.empty()) {
            if (stage == ShaderStage::Vertex) SDL_BindGPUVertexStorageBuffers(m_pass, 0, storage.data(), static_cast<uint32_t>(storage.size()));
            else SDL_BindGPUFragmentStorageBuffers(m_pass, 0, storage.data(), static_cast<uint32_t>(storage.size()));
        }
    }
    return true;
}
bool CommandList::CanDraw() {
    if (!Ready() || !m_pass) return false;
    auto* pipeline = m_device.pipelines.TryGet(m_pipeline);
    if (!pipeline) return false;
    for (auto layout : pipeline->layouts) if (!m_vertexBound[layout.slot]) return false;
    return BindResources(ShaderStage::Vertex) && BindResources(ShaderStage::Pixel);
}
void CommandList::Draw(uint32_t count, uint32_t first) {
    if (!CanDraw()) throw std::runtime_error("Missing or invalid RHI draw bindings");
    SDL_DrawGPUPrimitives(m_pass, count, 1, first, 0);
}
void CommandList::DrawIndexed(uint32_t count, uint32_t first, int32_t offset) {
    if (!DrawIndexedInstanced(count, 1, first, offset, 0)) throw std::runtime_error("Missing or invalid RHI indexed-draw bindings");
}
bool CommandList::DrawIndexedInstanced(uint32_t count, uint32_t instances, uint32_t first, int32_t offset, uint32_t firstInstance) {
    if (!m_indexBound || first > m_indexCapacity || count > m_indexCapacity-first || !CanDraw()) return false;
    SDL_DrawGPUIndexedPrimitives(m_pass, count, instances, first, offset, firstInstance); return true;
}
void CommandList::Dispatch(uint32_t x, uint32_t y, uint32_t z) {
    if (!Ready() || m_pass || !x || !y || !z) throw std::runtime_error("Invalid RHI compute dispatch");
    auto* pipeline = m_device.pipelines.TryGet(m_pipeline);
    if (!pipeline || !pipeline->compute) throw std::runtime_error("Missing compute pipeline");
    const auto& shader = pipeline->shaderDescriptions[2];
    for (uint32_t slot = 0; slot < shader.uniformBuffers; ++slot) if (!m_uniformBound[2][slot]) throw std::runtime_error("Missing compute uniforms");
    // Slot order is readonly buffers followed by writable buffers. Textures use
    // sampled slots followed by readonly storage and writable storage textures.
    std::vector<SDL_GPUBuffer*> readBuffers;
    std::vector<SDL_GPUStorageBufferReadWriteBinding> writeBuffers;
    std::vector<SDL_GPUTexture*> readTextures;
    std::vector<SDL_GPUStorageTextureReadWriteBinding> writeTextures;
    std::vector<SDL_GPUTextureSamplerBinding> samplers;
    const size_t stage = static_cast<size_t>(ShaderStage::Compute);
    for (uint32_t slot = 0; slot < shader.storageBuffers + shader.writableStorageBuffers; ++slot) {
        auto* view = m_device.bufferViews.TryGet(m_buffers[stage][slot]);
        auto* buffer = view ? m_device.buffers.TryGet(view->desc.buffer) : nullptr;
        bool write = slot >= shader.storageBuffers;
        if (!buffer || !buffer->native || view->desc.type != (write ? BufferViewType::UnorderedAccess : BufferViewType::ShaderResource)) throw std::runtime_error("Invalid compute storage buffer");
        if (write) writeBuffers.push_back({buffer->native.get(), false, 0, 0, 0}); else readBuffers.push_back(buffer->native.get());
    }
    for (uint32_t slot = 0; slot < shader.sampledTextures + shader.storageTextures + shader.writableStorageTextures; ++slot) {
        auto* view = m_device.textureViews.TryGet(m_textures[stage][slot]); auto* texture = view ? m_device.textures.TryGet(view->desc.texture) : nullptr;
        if (!texture || !texture->Get()) throw std::runtime_error("Invalid compute texture");
        if (slot < shader.sampledTextures) {
            auto* sampler = m_device.samplers.TryGet(m_samplers[stage][slot]);
            if (!sampler || view->desc.type != TextureViewType::ShaderResource) throw std::runtime_error("Invalid compute sampler");
            samplers.push_back({texture->Get(), sampler->native.get()});
        } else if (slot < shader.sampledTextures + shader.storageTextures) readTextures.push_back(texture->Get());
        else {
            if (view->desc.type != TextureViewType::UnorderedAccess) throw std::runtime_error("Invalid compute output texture");
            SDL_GPUStorageTextureReadWriteBinding binding{}; binding.texture = texture->Get(); writeTextures.push_back(binding);
        }
    }
    auto* compute = SDL_BeginGPUComputePass(m_command, writeTextures.data(), static_cast<uint32_t>(writeTextures.size()), writeBuffers.data(), static_cast<uint32_t>(writeBuffers.size()));
    SDL_BindGPUComputePipeline(compute, pipeline->compute.get());
    if (!samplers.empty()) SDL_BindGPUComputeSamplers(compute, 0, samplers.data(), static_cast<uint32_t>(samplers.size()));
    if (!readTextures.empty()) SDL_BindGPUComputeStorageTextures(compute, 0, readTextures.data(), static_cast<uint32_t>(readTextures.size()));
    if (!readBuffers.empty()) SDL_BindGPUComputeStorageBuffers(compute, 0, readBuffers.data(), static_cast<uint32_t>(readBuffers.size()));
    SDL_DispatchGPUCompute(compute, x, y, z); SDL_EndGPUComputePass(compute);
}

bool SwapChain::AcquireNextImage(IRHICommandList& list) {
    auto* command = dynamic_cast<CommandList*>(&list);
    return command && &command->Device() == &m_device && command->AcquireImage();
}
bool SwapChain::Resize(uint32_t width, uint32_t height) {
    if (!width || !height || width > INT32_MAX || height > INT32_MAX) return false;
    m_device.WaitIdle();
    if (!SDL_SetWindowSize(m_device.window, static_cast<int>(width), static_cast<int>(height))) return false;
    desc.width = width; desc.height = height; return true;
}
bool SwapChain::Present(bool vsync) {
    // Acquired textures are presented by SDL on Submit. Set the policy for the
    // following frame; never perform a second native present here.
    const auto mode = vsync ? SDL_GPU_PRESENTMODE_VSYNC : SDL_GPU_PRESENTMODE_IMMEDIATE;
    const auto selected = SDL_WindowSupportsGPUPresentMode(m_device.gpu, m_device.window, mode) ? mode : SDL_GPU_PRESENTMODE_VSYNC;
    if (m_presentMode == selected) return true;
    if (!SDL_SetGPUSwapchainParameters(m_device.gpu, m_device.window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, selected)) return false;
    m_presentMode = selected; return true;
}
bool Queue::Submit(const QueueSubmitDesc& desc) {
    if (desc.commandLists.empty()) return false;
    std::vector<QueueFenceWait> waits(desc.waits.begin(), desc.waits.end());
    std::vector<QueueFenceSignal> signals(desc.signals.begin(), desc.signals.end());
    if (desc.waitFence) waits.push_back({desc.waitFence, desc.waitValue});
    if (desc.signalFence) signals.push_back({desc.signalFence, desc.signalValue});
    std::vector<CommandList*> commands;
    for (auto* list : desc.commandLists) {
        auto* command = dynamic_cast<CommandList*>(list);
        if (!command || &command->Device() != &m_device || !command->IsClosed() || std::find(commands.begin(), commands.end(), command) != commands.end()) return false;
        commands.push_back(command);
    }
    for (size_t i = 0; i < signals.size(); ++i) {
        const auto& signal = signals[i]; auto* state = m_device.fences.TryGet(signal.fence);
        if (!state || signal.value <= state->submitted) return false;
        for (size_t j = 0; j < i; ++j) if (signals[j].fence == signal.fence) return false;
    }
    // This backend exposes a single ordered queue; foreign queue/timeline waits
    // are not advertised. Fence waits are explicit host waits, never fake GPU waits.
    for (const auto& wait : waits) if (!m_device.WaitFence(wait.fence, wait.value, UINT64_MAX)) return false;
    std::shared_ptr<SDL_GPUFence> completion;
    for (auto* command : commands) {
        auto* native = command->ReleaseForSubmit();
        if (!native || !(completion = m_device.Submit(native))) return false;
    }
    for (const auto& signal : signals) {
        auto* state = m_device.fences.TryGet(signal.fence);
        state->submitted = signal.value; state->pending.emplace_back(signal.value, completion);
    }
    return true;
}
bool Queue::Present(IRHISwapChain& chain, bool vsync) { return &chain == m_device.GetSwapChain() && chain.Present(vsync); }
void Queue::WaitIdle() { m_device.WaitIdle(); }
uint64_t GPUDevice::Completed(FenceHandle h) {
    auto* state = fences.TryGet(h); if (!state) return 0;
    std::erase_if(state->pending, [this, state](const auto& entry) {
        if (!SDL_QueryGPUFence(gpu, entry.second.get())) return false;
        state->completed = std::max(state->completed, entry.first); return true;
    });
    return state->completed;
}
bool GPUDevice::WaitFence(FenceHandle h, uint64_t value, uint64_t timeout) {
    auto* state = fences.TryGet(h);
    if (!state || value > state->submitted) return false;
    const auto start = std::chrono::steady_clock::now();
    while (Completed(h) < value) {
        if (timeout != UINT64_MAX && static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count()) >= timeout) return false;
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    return true;
}
Fence::Fence(GPUDevice& device, FenceHandle h) : m_device(device), m_handle(h), m_lifetime(device.GetLifetimeToken()) {}
uint64_t Fence::GetCompletedValue() const { return m_lifetime.expired() ? 0 : m_device.Completed(m_handle); }
bool Fence::Wait(uint64_t value, uint64_t timeout) { return !m_lifetime.expired() && m_device.WaitFence(m_handle, value, timeout); }
bool GPUDevice::ReadTexture(TextureHandle h, TextureReadback& result, uint64_t timeout) {
    auto* texture = textures.TryGet(h);
    if (!texture || texture->swapchain || texture->desc.arraySize != 1 || texture->desc.sampleCount != 1) return false;
    const uint32_t size = FormatSize(texture->desc.format);
    const uint64_t bytes = uint64_t(texture->desc.width) * texture->desc.height * size;
    if (!size || bytes > UINT32_MAX) return false;
    SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD, static_cast<uint32_t>(bytes), 0};
    auto* transfer = SDL_CreateGPUTransferBuffer(gpu, &info);
    if (!transfer) return false;
    auto* command = SDL_AcquireGPUCommandBuffer(gpu);
    if (!command) { SDL_ReleaseGPUTransferBuffer(gpu, transfer); return false; }
    auto* copy = SDL_BeginGPUCopyPass(command);
    SDL_GPUTextureRegion src{}; src.texture = texture->Get(); src.w = texture->desc.width; src.h = texture->desc.height; src.d = 1;
    SDL_GPUTextureTransferInfo dst{transfer, 0, texture->desc.width, texture->desc.height};
    SDL_DownloadFromGPUTexture(copy, &src, &dst); SDL_EndGPUCopyPass(copy);
    auto completion = Submit(command);
    const auto start = std::chrono::steady_clock::now();
    while (completion && !SDL_QueryGPUFence(gpu, completion.get())) {
        if (timeout != UINT64_MAX && static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count()) >= timeout) {
            // SDL retains the transfer buffer until the queued download completes.
            SDL_ReleaseGPUTransferBuffer(gpu, transfer); return false;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    void* mapped = completion ? SDL_MapGPUTransferBuffer(gpu, transfer, false) : nullptr;
    if (!mapped) { SDL_ReleaseGPUTransferBuffer(gpu, transfer); return false; }
    TextureReadback output;
    output.width = texture->desc.width; output.height = texture->desc.height;
    output.format = texture->desc.format; output.rowPitch = texture->desc.width * size;
    output.pixels.resize(static_cast<size_t>(bytes)); std::memcpy(output.pixels.data(), mapped, output.pixels.size());
    SDL_UnmapGPUTransferBuffer(gpu, transfer); SDL_ReleaseGPUTransferBuffer(gpu, transfer);
    result = std::move(output); return true;
}

} // namespace

SDLGPUBackend::SDLGPUBackend(BackendType type) : m_type(type) {
    if (!Driver(type)) throw std::invalid_argument("SDL GPU backend requires D3D12, Vulkan, or Metal");
}
BackendType SDLGPUBackend::GetType() const noexcept { return m_type; }
std::string_view SDLGPUBackend::GetName() const noexcept { return Driver(m_type); }
bool SDLGPUBackend::IsSupported() const { return SDL_GPUSupportsShaderFormats(ShaderFormat(m_type), Driver(m_type)); }
std::vector<AdapterInfo> SDLGPUBackend::EnumerateAdapters() const {
    // SDL selects the adapter. Index 0 represents that default; do not fabricate
    // native adapter enumeration or claim specific GPU memory sizes.
    return IsSupported() ? std::vector<AdapterInfo>{{0, std::string(Driver(m_type)) + " default adapter", 0, 0, false}} : std::vector<AdapterInfo>{};
}
std::unique_ptr<IRHIDevice> SDLGPUBackend::CreateDevice(const DeviceCreateDesc& desc) {
    if (desc.adapterIndex != 0 || desc.allowSoftwareAdapter ||
        (desc.nativeWindow.window && desc.nativeWindow.kind != NativeWindowHandle::Kind::SDL)) return nullptr;
    auto* native = SDL_CreateGPUDevice(ShaderFormat(m_type), desc.enableDebugLayer, Driver(m_type));
    if (!native) return nullptr;
    if (std::strcmp(SDL_GetGPUDeviceDriver(native), Driver(m_type)) != 0) { SDL_DestroyGPUDevice(native); return nullptr; }
    auto device = std::make_unique<GPUDevice>(native, static_cast<SDL_Window*>(desc.nativeWindow.window), m_type);
    if (!device->InitializeSwap(desc.swapChain)) return nullptr;
    return device;
}
bool RegisterSDLGPUBackends(BackendRegistry& registry) {
    bool result = true;
    result &= registry.Register(BackendType::Direct3D12, +[]() -> std::unique_ptr<IRHIBackend> { return std::make_unique<SDLGPUBackend>(BackendType::Direct3D12); });
    result &= registry.Register(BackendType::Vulkan, +[]() -> std::unique_ptr<IRHIBackend> { return std::make_unique<SDLGPUBackend>(BackendType::Vulkan); });
    result &= registry.Register(BackendType::Metal, +[]() -> std::unique_ptr<IRHIBackend> { return std::make_unique<SDLGPUBackend>(BackendType::Metal); });
    return result;
}

} // namespace RHI
