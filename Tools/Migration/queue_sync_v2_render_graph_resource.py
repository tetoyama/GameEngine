from queue_sync_v2_common import load, replace_once, save

path = "Source/GameApplication/Service/Graphics/RHI/RHIRenderGraph.h"
text = load(path)
text = replace_once(
    text,
    """\tRenderGraphResource AddResource(std::string name = {}){
\t\tResource resource;
\t\tresource.name = std::move(name);
\t\tm_resources.push_back(std::move(resource));
\t\tm_compiled = false;
\t\treturn {static_cast<uint32_t>(m_resources.size() - 1)};
\t}
""",
    """\tRenderGraphResource AddResource(
\t\tstd::string name = {},
\t\tResourceQueueSharingMode queueSharingMode = ResourceQueueSharingMode::Concurrent
\t){
\t\tResource resource;
\t\tresource.name = std::move(name);
\t\tresource.queueSharingMode = queueSharingMode;
\t\tm_resources.push_back(std::move(resource));
\t\tm_compiled = false;
\t\treturn {static_cast<uint32_t>(m_resources.size() - 1)};
\t}
""",
    "logical resource queue sharing",
)
text = replace_once(
    text,
    """\tRenderGraphResource ImportBuffer(BufferHandle buffer, ResourceState initialState, std::string name = {}){
\t\tif(!buffer) return {};
\t\tResource resource;
\t\tresource.name = std::move(name);
\t\tresource.buffer = buffer;
\t\tresource.initialState = initialState;
\t\tresource.subresourceCount = 1;
\t\tm_resources.push_back(std::move(resource));
\t\tm_compiled = false;
\t\treturn {static_cast<uint32_t>(m_resources.size() - 1)};
\t}
""",
    """\tRenderGraphResource ImportBuffer(
\t\tBufferHandle buffer,
\t\tResourceState initialState,
\t\tstd::string name = {}
\t){
\t\treturn ImportBufferInternal(
\t\t\tbuffer,
\t\t\tinitialState,
\t\t\tResourceQueueSharingMode::Exclusive,
\t\t\tstd::move(name)
\t\t);
\t}

\tRenderGraphResource ImportBuffer(
\t\tBufferHandle buffer,
\t\tResourceState initialState,
\t\tconst BufferDesc& desc,
\t\tstd::string name = {}
\t){
\t\treturn ImportBufferInternal(
\t\t\tbuffer,
\t\t\tinitialState,
\t\t\tdesc.queueSharingMode,
\t\t\tstd::move(name)
\t\t);
\t}
""",
    "buffer import queue sharing",
)
text = replace_once(
    text,
    """\tRenderGraphResource ImportTexture(TextureHandle texture, ResourceState initialState, std::string name = {}){
\t\treturn ImportTextureInternal(texture, initialState, 1, std::move(name));
\t}
""",
    """\tRenderGraphResource ImportTexture(TextureHandle texture, ResourceState initialState, std::string name = {}){
\t\treturn ImportTextureInternal(
\t\t\ttexture,
\t\t\tinitialState,
\t\t\t1,
\t\t\tResourceQueueSharingMode::Exclusive,
\t\t\tstd::move(name)
\t\t);
\t}
""",
    "simple texture import queue sharing",
)
text = replace_once(
    text,
    """\t\treturn ImportTextureInternal(texture, initialState, count, std::move(name));
""",
    """\t\treturn ImportTextureInternal(
\t\t\ttexture,
\t\t\tinitialState,
\t\t\tcount,
\t\t\tdesc.queueSharingMode,
\t\t\tstd::move(name)
\t\t);
""",
    "descriptor texture import queue sharing",
)
text = replace_once(
    text,
    """\t\tBuildDependencies();
\t\tif(!BuildExecutionOrder()) return Fail("RenderGraph dependency cycle detected.");
""",
    """\t\tif(!ValidateQueueSharing()) return false;
\t\tBuildDependencies();
\t\tif(!BuildExecutionOrder()) return Fail("RenderGraph dependency cycle detected.");
""",
    "queue sharing validation during compile",
)
text = replace_once(
    text,
    """\tbool IsTransient(RenderGraphResource resource) const noexcept {
\t\treturn resource && resource.index < m_resources.size() &&
\t\t\t!m_resources[resource.index].IsBound();
\t}

""",
    """\tbool IsTransient(RenderGraphResource resource) const noexcept {
\t\treturn resource && resource.index < m_resources.size() &&
\t\t\t!m_resources[resource.index].IsBound();
\t}

\tResourceQueueSharingMode QueueSharingModeForResource(
\t\tRenderGraphResource resource
\t) const noexcept {
\t\treturn resource && resource.index < m_resources.size()
\t\t\t? m_resources[resource.index].queueSharingMode
\t\t\t: ResourceQueueSharingMode::Exclusive;
\t}

""",
    "resource queue sharing query",
)
text = replace_once(
    text,
    """\t\tResourceState initialState = ResourceState::Undefined;
\t\tuint32_t subresourceCount = 1;
\t\tbool IsBound() const noexcept { return static_cast<bool>(buffer) != static_cast<bool>(texture); }
""",
    """\t\tResourceState initialState = ResourceState::Undefined;
\t\tuint32_t subresourceCount = 1;
\t\tResourceQueueSharingMode queueSharingMode = ResourceQueueSharingMode::Exclusive;
\t\tbool IsBound() const noexcept { return static_cast<bool>(buffer) != static_cast<bool>(texture); }
""",
    "RenderGraph resource queue sharing field",
)
text = replace_once(
    text,
    """\tRenderGraphResource ImportTextureInternal(TextureHandle texture, ResourceState initialState,
\t\tuint32_t subresourceCount, std::string name){
\t\tif(!texture || subresourceCount == 0) return {};
\t\tResource resource;
\t\tresource.name = std::move(name);
\t\tresource.texture = texture;
\t\tresource.initialState = initialState;
\t\tresource.subresourceCount = subresourceCount;
\t\tm_resources.push_back(std::move(resource));
\t\tm_compiled = false;
\t\treturn {static_cast<uint32_t>(m_resources.size() - 1)};
\t}
""",
    """\tRenderGraphResource ImportBufferInternal(
\t\tBufferHandle buffer,
\t\tResourceState initialState,
\t\tResourceQueueSharingMode queueSharingMode,
\t\tstd::string name
\t){
\t\tif(!buffer) return {};
\t\tResource resource;
\t\tresource.name = std::move(name);
\t\tresource.buffer = buffer;
\t\tresource.initialState = initialState;
\t\tresource.subresourceCount = 1;
\t\tresource.queueSharingMode = queueSharingMode;
\t\tm_resources.push_back(std::move(resource));
\t\tm_compiled = false;
\t\treturn {static_cast<uint32_t>(m_resources.size() - 1)};
\t}

\tRenderGraphResource ImportTextureInternal(
\t\tTextureHandle texture,
\t\tResourceState initialState,
\t\tuint32_t subresourceCount,
\t\tResourceQueueSharingMode queueSharingMode,
\t\tstd::string name
\t){
\t\tif(!texture || subresourceCount == 0) return {};
\t\tResource resource;
\t\tresource.name = std::move(name);
\t\tresource.texture = texture;
\t\tresource.initialState = initialState;
\t\tresource.subresourceCount = subresourceCount;
\t\tresource.queueSharingMode = queueSharingMode;
\t\tm_resources.push_back(std::move(resource));
\t\tm_compiled = false;
\t\treturn {static_cast<uint32_t>(m_resources.size() - 1)};
\t}
""",
    "RenderGraph resource import helpers",
)
text = replace_once(
    text,
    """\tvoid BuildDependencies(){
""",
    """\tbool ValidateQueueSharing(){
\t\tstd::vector<uint8_t> queueMasks(m_resources.size(), 0);
\t\tfor(const Pass& pass : m_passes){
\t\t\tconst uint8_t queueBit = static_cast<uint8_t>(1u << QueueIndex(pass.queue));
\t\t\tfor(const RenderGraphAccess& access : pass.accesses){
\t\t\t\tqueueMasks[access.resource.index] |= queueBit;
\t\t\t}
\t\t}

\t\tfor(size_t resourceIndex = 0; resourceIndex < m_resources.size(); ++resourceIndex){
\t\t\tconst uint8_t queueMask = queueMasks[resourceIndex];
\t\t\tconst bool usesMultipleQueues =
\t\t\t\tqueueMask != 0 &&
\t\t\t\t(queueMask & static_cast<uint8_t>(queueMask - 1)) != 0;
\t\t\tif(!usesMultipleQueues) continue;

\t\t\tconst Resource& resource = m_resources[resourceIndex];
\t\t\tif(resource.queueSharingMode == ResourceQueueSharingMode::Concurrent) continue;

\t\t\tconst std::string resourceName = resource.name.empty()
\t\t\t\t? std::string("RenderGraph resource #") + std::to_string(resourceIndex)
\t\t\t\t: resource.name;
\t\t\treturn Fail(
\t\t\t\tresourceName +
\t\t\t\t": exclusive resource is used by multiple queue domains; declare Concurrent sharing."
\t\t\t);
\t\t}
\t\treturn true;
\t}

\tvoid BuildDependencies(){
""",
    "RenderGraph queue sharing validation implementation",
)
save(path, text)

print("RenderGraph resource queue sharing hardened")
