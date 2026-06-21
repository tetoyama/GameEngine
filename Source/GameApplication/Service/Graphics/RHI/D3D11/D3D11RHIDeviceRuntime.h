#pragma once

#include <algorithm>
#include <limits>
#include <thread>

namespace RHI {

inline D3D11RHICommandList::D3D11RHICommandList(
	D3D11RHIDevice* owner,
	CommandQueueType queueType
) : m_owner(owner), m_queueType(queueType){}

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
	, m_graphicsQueue(this, CommandQueueType::Graphics)
	, m_computeQueue(this, CommandQueueType::Compute)
	, m_copyQueue(this, CommandQueueType::Copy){
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
	m_samplers.Clear();
	m_textureViews.Clear();
	m_bufferViews.Clear();
	m_textures.Clear();
	m_buffers.Clear();
}

inline bool D3D11RHIDevice::DestroyBuffer(BufferHandle handle){
	D3D11BufferResource* resource = Find(handle);
	return resource && resource->viewCount == 0 && m_buffers.Destroy(handle);
}

inline bool D3D11RHIDevice::DestroyTexture(TextureHandle handle){
	D3D11TextureResource* resource = Find(handle);
	return resource && resource->viewCount == 0 && m_textures.Destroy(handle);
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
	switch(type){
		case CommandQueueType::Graphics: return &m_graphicsQueue;
		case CommandQueueType::Compute: return m_capabilities.supportsCompute ? &m_computeQueue : nullptr;
		case CommandQueueType::Copy: return &m_copyQueue;
	}
	return nullptr;
}

inline const IRHICommandQueue* D3D11RHIDevice::GetQueue(CommandQueueType type) const {
	return const_cast<D3D11RHIDevice*>(this)->GetQueue(type);
}

inline std::unique_ptr<IRHICommandList> D3D11RHIDevice::CreateCommandList(
	const CommandListCreateDesc& desc
){
	if(desc.secondary) return nullptr;
	if(desc.queueType == CommandQueueType::Compute && !m_capabilities.supportsCompute) return nullptr;
	return std::make_unique<D3D11RHICommandList>(this, desc.queueType);
}

inline std::unique_ptr<IRHIFence> D3D11RHIDevice::CreateFence(uint64_t initialValue){
	if(!m_context) return nullptr;
	auto state = std::make_shared<D3D11FenceState>();
	state->completedValue = initialValue;
	state->lastSignaledValue = initialValue;
	state->context = m_context;
	const FenceHandle handle = m_fences.Create(state);
	return std::make_unique<D3D11RHIFence>(handle, std::move(state));
}

inline std::shared_ptr<D3D11FenceState> D3D11RHIDevice::FindFence(FenceHandle handle) const {
	const auto* state = m_fences.TryGet(handle);
	return state ? *state : nullptr;
}

inline uint64_t D3D11FenceState::PollCompletedValue(){
	std::scoped_lock lock(mutex);
	if(!context) return completedValue;
	while(!pendingSignals.empty()){
		D3D11FenceSignal& signal = pendingSignals.front();
		BOOL complete = FALSE;
		const HRESULT result = context->GetData(
			signal.query.Get(), &complete, sizeof(complete), D3D11_ASYNC_GETDATA_DONOTFLUSH
		);
		if(result == S_FALSE || complete == FALSE || FAILED(result)) break;
		completedValue = (std::max)(completedValue, signal.value);
		pendingSignals.pop_front();
	}
	return completedValue;
}

inline bool D3D11FenceState::Wait(uint64_t value, uint64_t timeoutNanoseconds){
	if(PollCompletedValue() >= value) return true;
	if(timeoutNanoseconds == 0) return false;
	const bool infinite = timeoutNanoseconds == (std::numeric_limits<uint64_t>::max)();
	const auto start = std::chrono::steady_clock::now();
	for(;;){
		if(PollCompletedValue() >= value) return true;
		if(!infinite){
			const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
				std::chrono::steady_clock::now() - start
			);
			if(elapsed.count() >= timeoutNanoseconds) return false;
		}
		std::this_thread::yield();
	}
}

inline uint64_t D3D11RHIFence::GetCompletedValue() const {
	return m_state ? m_state->PollCompletedValue() : 0;
}

inline bool D3D11RHIFence::Wait(uint64_t value, uint64_t timeoutNanoseconds){
	return m_state && m_state->Wait(value, timeoutNanoseconds);
}

inline bool D3D11RHIDevice::SignalFence(
	const std::shared_ptr<D3D11FenceState>& state,
	uint64_t value
){
	if(!state || !m_device || !m_context) return false;
	std::scoped_lock lock(state->mutex);
	if(value <= state->completedValue) return true;
	if(value < state->lastSignaledValue) return false;
	if(value == state->lastSignaledValue) return true;

	D3D11_QUERY_DESC desc{};
	desc.Query = D3D11_QUERY_EVENT;
	D3D11FenceSignal signal;
	signal.value = value;
	if(FAILED(m_device->CreateQuery(&desc, signal.query.GetAddressOf()))) return false;
	m_context->End(signal.query.Get());
	state->pendingSignals.push_back(std::move(signal));
	state->lastSignaledValue = value;
	m_context->Flush();
	return true;
}

inline bool D3D11RHICommandQueue::Submit(const QueueSubmitDesc& desc){
	if(!m_owner || !m_owner->m_context) return false;
	for(IRHICommandList* commandList : desc.commandLists){
		if(!commandList || commandList->GetQueueType() != m_type) return false;
		auto* list = dynamic_cast<D3D11RHICommandList*>(commandList);
		if(!list || !list->IsClosed()) return false;
	}
	if(desc.waitFence){
		auto state = m_owner->FindFence(desc.waitFence);
		if(!state || !state->Wait(desc.waitValue, (std::numeric_limits<uint64_t>::max)())) return false;
	}
	if(desc.signalFence){
		auto state = m_owner->FindFence(desc.signalFence);
		if(!m_owner->SignalFence(state, desc.signalValue)) return false;
	}
	else m_owner->m_context->Flush();
	return true;
}

inline bool D3D11RHIDevice::WaitForImmediateContext(){
	if(!m_device || !m_context) return false;
	D3D11_QUERY_DESC desc{};
	desc.Query = D3D11_QUERY_EVENT;
	Microsoft::WRL::ComPtr<ID3D11Query> query;
	if(FAILED(m_device->CreateQuery(&desc, query.GetAddressOf()))) return false;
	m_context->End(query.Get());
	m_context->Flush();
	for(;;){
		BOOL complete = FALSE;
		const HRESULT result = m_context->GetData(
			query.Get(), &complete, sizeof(complete), D3D11_ASYNC_GETDATA_DONOTFLUSH
		);
		if(result == S_OK && complete != FALSE) return true;
		if(FAILED(result)) return false;
		std::this_thread::yield();
	}
}

inline void D3D11RHICommandQueue::WaitIdle(){
	if(m_owner) m_owner->WaitForImmediateContext();
}

inline void D3D11RHIDevice::WaitIdle(){
	WaitForImmediateContext();
}

} // namespace RHI
