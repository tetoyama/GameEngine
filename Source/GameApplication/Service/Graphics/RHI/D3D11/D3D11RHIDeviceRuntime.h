#pragma once

#include <algorithm>

namespace RHI {

inline D3D11RHICommandList::D3D11RHICommandList(
	D3D11RHIDevice* owner,
	CommandQueueType queueType
)
	: m_owner(owner)
	, m_queueType(queueType){
}

inline void D3D11RHICommandList::Attach(
	D3D11RHIDevice* owner,
	CommandQueueType queueType
){
	m_owner = owner;
	m_queueType = queueType;
	m_recording = false;
	m_closed = false;
	m_renderPassActive = false;
}

inline D3D11RHIDevice::D3D11RHIDevice(
	ID3D11Device* device,
	ID3D11DeviceContext* context,
	IDXGISwapChain* swapChain,
	const SwapChainDesc& swapChainDesc
)
	: m_device(device)
	, m_context(context)
	, m_swapChain(swapChain, swapChainDesc)
	, m_graphicsQueue(this){
	m_capabilities.backend = BackendType::Direct3D11;
	m_capabilities.maximumColorAttachments = D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;
	m_capabilities.maximumTextureDimension2D = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
	m_capabilities.maximumConstantBufferSize = D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16u;
	m_capabilities.maximumVertexBufferSlots = D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;
	m_capabilities.maximumShaderResourceSlots = D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;
	m_capabilities.supportsCompute = true;
	m_capabilities.supportsGeometryShader = true;
	m_capabilities.supportsTessellation = true;
	m_capabilities.supportsIndirectDraw = true;
	m_capabilities.supportsAsyncCompute = false;
	m_capabilities.supportsMultipleCommandQueues = false;
	m_capabilities.supportsTimelineSynchronization = false;
}

inline D3D11RHIDevice::~D3D11RHIDevice(){
	WaitIdle();
	m_fences.Clear();
	m_pipelines.Clear();
	m_shaders.Clear();
	m_textures.Clear();
	m_buffers.Clear();
}

inline bool D3D11RHIDevice::DestroyBuffer(BufferHandle handle){
	return m_buffers.Destroy(handle);
}

inline bool D3D11RHIDevice::DestroyTexture(TextureHandle handle){
	return m_textures.Destroy(handle);
}

inline bool D3D11RHIDevice::DestroyShader(ShaderHandle handle){
	return m_shaders.Destroy(handle);
}

inline bool D3D11RHIDevice::DestroyPipelineState(PipelineStateHandle handle){
	return m_pipelines.Destroy(handle);
}

inline const BufferDesc* D3D11RHIDevice::GetBufferDesc(BufferHandle handle) const {
	const auto* resource = Find(handle);
	return resource ? &resource->desc : nullptr;
}

inline const TextureDesc* D3D11RHIDevice::GetTextureDesc(TextureHandle handle) const {
	const auto* resource = Find(handle);
	return resource ? &resource->desc : nullptr;
}

inline const ShaderDesc* D3D11RHIDevice::GetShaderDesc(ShaderHandle handle) const {
	const auto* resource = Find(handle);
	return resource ? &resource->desc : nullptr;
}

inline const PipelineStateDesc* D3D11RHIDevice::GetPipelineStateDesc(
	PipelineStateHandle handle
) const {
	const auto* resource = Find(handle);
	return resource ? &resource->desc : nullptr;
}

inline IRHICommandQueue* D3D11RHIDevice::GetQueue(CommandQueueType type){
	return type == CommandQueueType::Graphics
		? static_cast<IRHICommandQueue*>(&m_graphicsQueue)
		: nullptr;
}

inline const IRHICommandQueue* D3D11RHIDevice::GetQueue(
	CommandQueueType type
) const {
	return type == CommandQueueType::Graphics
		? static_cast<const IRHICommandQueue*>(&m_graphicsQueue)
		: nullptr;
}

inline std::unique_ptr<IRHICommandList> D3D11RHIDevice::CreateCommandList(
	const CommandListCreateDesc& desc
){
	if(desc.queueType != CommandQueueType::Graphics || desc.secondary){
		return nullptr;
	}
	return std::make_unique<D3D11RHICommandList>(
		this,
		CommandQueueType::Graphics
	);
}

inline std::unique_ptr<IRHIFence> D3D11RHIDevice::CreateFence(
	uint64_t initialValue
){
	auto state = std::make_shared<D3D11FenceState>();
	state->completedValue = initialValue;
	const FenceHandle handle = m_fences.Create(state);
	return std::make_unique<D3D11RHIFence>(handle, std::move(state));
}

inline std::shared_ptr<D3D11FenceState> D3D11RHIDevice::FindFence(
	FenceHandle handle
) const {
	const auto* state = m_fences.TryGet(handle);
	return state ? *state : nullptr;
}

inline uint64_t D3D11RHIFence::GetCompletedValue() const {
	std::scoped_lock lock(m_state->mutex);
	return m_state->completedValue;
}

inline bool D3D11RHIFence::Wait(
	uint64_t value,
	uint64_t timeoutNanoseconds
){
	std::unique_lock lock(m_state->mutex);
	if(timeoutNanoseconds == 0){
		return m_state->completedValue >= value;
	}
	return m_state->condition.wait_for(
		lock,
		std::chrono::nanoseconds(timeoutNanoseconds),
		[&]{ return m_state->completedValue >= value; }
	);
}

inline bool D3D11RHICommandQueue::Submit(const QueueSubmitDesc& desc){
	if(!m_owner || !m_owner->m_context) return false;

	if(desc.waitFence){
		auto state = m_owner->FindFence(desc.waitFence);
		if(!state) return false;
		std::unique_lock lock(state->mutex);
		state->condition.wait(lock, [&]{
			return state->completedValue >= desc.waitValue;
		});
	}

	for(IRHICommandList* commandList : desc.commandLists){
		if(!commandList ||
			commandList->GetQueueType() != CommandQueueType::Graphics){
			return false;
		}
		auto* d3d11List = dynamic_cast<D3D11RHICommandList*>(commandList);
		if(!d3d11List || !d3d11List->IsClosed()) return false;
	}

	// D3D11ではCommandList記録時にImmediate Contextへ発行済み。
	// SubmitはAPI非依存Queue契約の検証とFence通知を担当する。
	m_owner->m_context->Flush();

	if(desc.signalFence){
		auto state = m_owner->FindFence(desc.signalFence);
		if(!state) return false;
		{
			std::scoped_lock lock(state->mutex);
			state->completedValue = (std::max)(
				state->completedValue,
				desc.signalValue
			);
		}
		state->condition.notify_all();
	}

	return true;
}

inline void D3D11RHICommandQueue::WaitIdle(){
	if(m_owner && m_owner->m_context){
		m_owner->m_context->Flush();
	}
}

inline void D3D11RHIDevice::WaitIdle(){
	m_graphicsQueue.WaitIdle();
}

} // namespace RHI
