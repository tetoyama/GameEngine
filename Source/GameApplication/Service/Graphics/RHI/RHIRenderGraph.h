#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "RHIInterfaces.h"

namespace RHI {

inline constexpr uint32_t InvalidRenderGraphIndex =
	(std::numeric_limits<uint32_t>::max)();

struct RenderGraphResource {
	uint32_t index = InvalidRenderGraphIndex;
	constexpr explicit operator bool() const noexcept {
		return index != InvalidRenderGraphIndex;
	}
	friend constexpr bool operator==(
		const RenderGraphResource&,
		const RenderGraphResource&
	) noexcept = default;
};

struct RenderGraphResourceLifetime {
	uint32_t firstExecutionIndex = InvalidRenderGraphIndex;
	uint32_t lastExecutionIndex = InvalidRenderGraphIndex;
	uint32_t firstPassIndex = InvalidRenderGraphIndex;
	uint32_t lastPassIndex = InvalidRenderGraphIndex;
	uint32_t passUseCount = 0;

	constexpr bool IsUsed() const noexcept {
		return firstExecutionIndex != InvalidRenderGraphIndex;
	}

	constexpr bool Overlaps(
		const RenderGraphResourceLifetime& other
	) const noexcept {
		if(!IsUsed() || !other.IsUsed()) return false;
		return !(lastExecutionIndex < other.firstExecutionIndex ||
			other.lastExecutionIndex < firstExecutionIndex);
	}
};

struct RenderGraphQueueWait {
	CommandQueueType sourceQueue = CommandQueueType::Graphics;
	uint32_t producerPassIndex = InvalidRenderGraphIndex;
	uint64_t value = 0;
};

struct RenderGraphPassQueueSync {
	CommandQueueType queue = CommandQueueType::Graphics;
	std::vector<RenderGraphQueueWait> waits;
	uint64_t signalValue = 0;

	bool RequiresSynchronization() const noexcept {
		return !waits.empty() || signalValue != 0;
	}
};

enum class RenderGraphAccessType : uint8_t { Read, Write, ReadWrite };

struct RenderGraphSubresourceRange {
	uint32_t first = AllSubresources;
	uint32_t count = 0;

	static constexpr RenderGraphSubresourceRange All() noexcept { return {}; }
	static constexpr RenderGraphSubresourceRange Single(uint32_t index) noexcept { return {index, 1}; }
	static constexpr RenderGraphSubresourceRange Make(uint32_t firstIndex, uint32_t subresourceCount) noexcept {
		return {firstIndex, subresourceCount};
	}
	constexpr bool IsAll() const noexcept { return first == AllSubresources; }
	constexpr bool IsEmpty() const noexcept { return !IsAll() && count == 0; }
	friend constexpr bool operator==(const RenderGraphSubresourceRange&, const RenderGraphSubresourceRange&) noexcept = default;
};

struct RenderGraphAccess {
	RenderGraphResource resource;
	RenderGraphAccessType type = RenderGraphAccessType::Read;
	ResourceState state = ResourceState::Undefined;
	RenderGraphSubresourceRange subresources;
};

class RenderGraphPassBuilder {
public:
	void Read(RenderGraphResource resource, ResourceState state = ResourceState::Undefined,
		RenderGraphSubresourceRange subresources = RenderGraphSubresourceRange::All()){
		Add(resource, RenderGraphAccessType::Read, state, subresources);
	}
	void Write(RenderGraphResource resource, ResourceState state = ResourceState::Undefined,
		RenderGraphSubresourceRange subresources = RenderGraphSubresourceRange::All()){
		Add(resource, RenderGraphAccessType::Write, state, subresources);
	}
	void ReadWrite(RenderGraphResource resource, ResourceState state = ResourceState::Undefined,
		RenderGraphSubresourceRange subresources = RenderGraphSubresourceRange::All()){
		Add(resource, RenderGraphAccessType::ReadWrite, state, subresources);
	}
	const std::vector<RenderGraphAccess>& Accesses() const noexcept { return m_accesses; }
	bool IsValid() const noexcept { return m_error.empty(); }
	const std::string& Error() const noexcept { return m_error; }

private:
	static bool Overlaps(const RenderGraphSubresourceRange& lhs, const RenderGraphSubresourceRange& rhs){
		if(lhs.IsAll() || rhs.IsAll()) return true;
		const uint64_t lhsEnd = static_cast<uint64_t>(lhs.first) + lhs.count;
		const uint64_t rhsEnd = static_cast<uint64_t>(rhs.first) + rhs.count;
		return lhs.first < rhsEnd && rhs.first < lhsEnd;
	}

	void Add(RenderGraphResource resource, RenderGraphAccessType type, ResourceState state,
		RenderGraphSubresourceRange subresources){
		if(!resource) return;
		if(subresources.IsEmpty()){
			m_error = "RenderGraph pass declared an empty subresource range.";
			return;
		}
		for(RenderGraphAccess& access : m_accesses){
			if(access.resource != resource) continue;
			if(access.subresources != subresources){
				if(Overlaps(access.subresources, subresources)){
					m_error = "RenderGraph pass declared overlapping non-identical subresource ranges.";
					return;
				}
				continue;
			}
			if(access.type != type) access.type = RenderGraphAccessType::ReadWrite;
			if(access.state == ResourceState::Undefined) access.state = state;
			else if(state != ResourceState::Undefined && access.state != state){
				m_error = "RenderGraph pass requested conflicting states for one subresource range.";
			}
			return;
		}
		m_accesses.push_back({resource, type, state, subresources});
	}

	std::vector<RenderGraphAccess> m_accesses;
	std::string m_error;
};

class RenderGraph {
public:
	using ExecuteCallback = std::function<void(IRHICommandList&)>;

	RenderGraphResource AddResource(
		std::string name = {},
		ResourceQueueSharingMode queueSharingMode = ResourceQueueSharingMode::Concurrent
	){
		Resource resource;
		resource.name = std::move(name);
		resource.queueSharingMode = queueSharingMode;
		m_resources.push_back(std::move(resource));
		m_compiled = false;
		return {static_cast<uint32_t>(m_resources.size() - 1)};
	}

	RenderGraphResource ImportBuffer(
		BufferHandle buffer,
		ResourceState initialState,
		std::string name = {}
	){
		return ImportBufferInternal(
			buffer,
			initialState,
			ResourceQueueSharingMode::Exclusive,
			std::move(name)
		);
	}

	RenderGraphResource ImportBuffer(
		BufferHandle buffer,
		ResourceState initialState,
		const BufferDesc& desc,
		std::string name = {}
	){
		return ImportBufferInternal(
			buffer,
			initialState,
			desc.queueSharingMode,
			std::move(name)
		);
	}

	RenderGraphResource ImportTexture(TextureHandle texture, ResourceState initialState, std::string name = {}){
		return ImportTextureInternal(
			texture,
			initialState,
			1,
			ResourceQueueSharingMode::Exclusive,
			std::move(name)
		);
	}

	RenderGraphResource ImportTexture(TextureHandle texture, ResourceState initialState,
		const TextureDesc& desc, std::string name = {}){
		const uint32_t count = (std::max)(1u,
			static_cast<uint32_t>(desc.mipLevels) * static_cast<uint32_t>(desc.arraySize));
		return ImportTextureInternal(
			texture,
			initialState,
			count,
			desc.queueSharingMode,
			std::move(name)
		);
	}

	uint32_t AddPass(
		std::string name,
		const std::function<void(RenderGraphPassBuilder&)>& setup,
		ExecuteCallback execute
	){
		return AddPass(
			std::move(name),
			CommandQueueType::Graphics,
			setup,
			std::move(execute)
		);
	}

	uint32_t AddPass(
		std::string name,
		CommandQueueType queue,
		const std::function<void(RenderGraphPassBuilder&)>& setup,
		ExecuteCallback execute
	){
		RenderGraphPassBuilder builder;
		if(setup) setup(builder);
		Pass pass;
		pass.name = std::move(name);
		pass.queue = queue;
		pass.accesses = builder.Accesses();
		pass.setupError = builder.Error();
		pass.execute = std::move(execute);
		m_passes.push_back(std::move(pass));
		m_compiled = false;
		return static_cast<uint32_t>(m_passes.size() - 1);
	}

	bool Compile(){
		m_executionOrder.clear();
		m_resourceLifetimes.clear();
		m_queueSync.clear();
		m_finalStates.clear();
		m_error.clear();
		for(Pass& pass : m_passes){
			pass.dependencies.clear();
			pass.dependents.clear();
			pass.barriers.clear();
			if(!pass.setupError.empty()) return Fail(pass.name + ": " + pass.setupError);
			for(const RenderGraphAccess& access : pass.accesses){
				if(!access.resource || access.resource.index >= m_resources.size()){
					return Fail(pass.name + ": invalid RenderGraph resource.");
				}
				if(!ValidateRange(m_resources[access.resource.index], access.subresources)){
					return Fail(pass.name + ": invalid subresource range for RenderGraph resource.");
				}
			}
		}
		if(!ValidateQueueSharing()) return false;
		BuildDependencies();
		if(!BuildExecutionOrder()) return Fail("RenderGraph dependency cycle detected.");
		BuildResourceLifetimes();
		BuildQueueSynchronization();
		if(!BuildBarriers()) return false;
		m_compiled = true;
		return true;
	}

	bool Execute(IRHICommandList& commandList){
		if(!m_compiled && !Compile()) return false;
		for(uint32_t passIndex : m_executionOrder){
			if(m_passes[passIndex].queue != commandList.GetQueueType()){
				return Fail(
					"RenderGraph contains multiple queue domains; use Execute(IRHIDevice&)."
				);
			}
		}

		commandList.Begin();
		for(uint32_t passIndex : m_executionOrder){
			Pass& pass = m_passes[passIndex];
			if(!RecordPass(commandList, pass)){
				commandList.End();
				return false;
			}
		}
		commandList.End();
		return true;
	}

	bool Execute(IRHIDevice& device){
		if(!m_compiled && !Compile()) return false;

		if(m_executionDevice && m_executionDevice != &device){
			return Fail(
				"RenderGraph execution device changed; release execution state before reusing the graph."
			);
		}
		m_executionDevice = &device;

		const std::array<uint64_t, 3> executionBase = m_queueFenceValues;
		for(const RenderGraphPassQueueSync& sync : m_queueSync){
			if(sync.signalValue == 0) continue;
			const size_t queueIndex = QueueIndex(sync.queue);
			if(!m_queueFences[queueIndex]){
				m_queueFences[queueIndex] = device.CreateFence(0);
				if(!m_queueFences[queueIndex]){
					return Fail("RenderGraph failed to create a queue synchronization fence.");
				}
			}
		}

		for(uint32_t passIndex : m_executionOrder){
			Pass& pass = m_passes[passIndex];
			const RenderGraphPassQueueSync& sync = m_queueSync[passIndex];
			IRHICommandQueue* queue = device.GetQueue(pass.queue);
			if(!queue){
				return Fail(pass.name + ": requested queue is not available.");
			}

			CommandListCreateDesc commandListDesc;
			commandListDesc.queueType = pass.queue;
			std::unique_ptr<IRHICommandList> commandList =
				device.CreateCommandList(commandListDesc);
			if(!commandList){
				return Fail(pass.name + ": failed to create a command list.");
			}

			commandList->Begin();
			if(!RecordPass(*commandList, pass)){
				commandList->End();
				return false;
			}
			commandList->End();

			std::vector<QueueFenceWait> waits;
			waits.reserve(sync.waits.size());
			for(const RenderGraphQueueWait& wait : sync.waits){
				IRHIFence* fence = m_queueFences[QueueIndex(wait.sourceQueue)].get();
				if(!fence){
					return Fail(pass.name + ": missing producer queue fence.");
				}
				waits.push_back({
					fence->GetHandle(),
					executionBase[QueueIndex(wait.sourceQueue)] + wait.value
				});
			}

			std::array<QueueFenceSignal, 1> signals{};
			std::span<const QueueFenceSignal> signalSpan;
			if(sync.signalValue != 0){
				IRHIFence* fence = m_queueFences[QueueIndex(sync.queue)].get();
				if(!fence){
					return Fail(pass.name + ": missing signal queue fence.");
				}
				signals[0] = {
					fence->GetHandle(),
					executionBase[QueueIndex(sync.queue)] + sync.signalValue
				};
				signalSpan = signals;
			}

			std::array<IRHICommandList*, 1> commandLists{commandList.get()};
			QueueSubmitDesc submit;
			submit.commandLists = commandLists;
			submit.waits = waits;
			submit.signals = signalSpan;
			if(!queue->Submit(submit)){
				return Fail(pass.name + ": queue submission failed.");
			}
			if(sync.signalValue != 0){
				m_queueFenceValues[QueueIndex(sync.queue)] = signals[0].value;
			}
		}
		return true;
	}

	const std::vector<uint32_t>& ExecutionOrder() const noexcept { return m_executionOrder; }
	const std::vector<uint32_t>& DependenciesForPass(uint32_t passIndex) const noexcept {
		static const std::vector<uint32_t> empty;
		return passIndex < m_passes.size() ? m_passes[passIndex].dependencies : empty;
	}
	const std::vector<ResourceBarrierDesc>& BarriersForPass(uint32_t passIndex) const noexcept {
		static const std::vector<ResourceBarrierDesc> empty;
		return passIndex < m_passes.size() ? m_passes[passIndex].barriers : empty;
	}

	CommandQueueType QueueForPass(uint32_t passIndex) const noexcept {
		return passIndex < m_passes.size()
			? m_passes[passIndex].queue
			: CommandQueueType::Graphics;
	}

	const RenderGraphPassQueueSync& QueueSyncForPass(
		uint32_t passIndex
	) const noexcept {
		static const RenderGraphPassQueueSync empty;
		return passIndex < m_queueSync.size() ? m_queueSync[passIndex] : empty;
	}

	bool RequiresQueueSynchronization() const noexcept {
		return std::any_of(
			m_queueSync.begin(),
			m_queueSync.end(),
			[](const RenderGraphPassQueueSync& sync){
				return sync.RequiresSynchronization();
			}
		);
	}

	uint64_t LastSubmittedQueueFenceValue(CommandQueueType queue) const noexcept {
		return m_queueFenceValues[QueueIndex(queue)];
	}

	bool ReleaseExecutionState(IRHIDevice& device){
		if(!m_executionDevice) return true;
		if(m_executionDevice != &device) return false;

		device.WaitIdle();
		bool released = true;
		for(std::unique_ptr<IRHIFence>& fence : m_queueFences){
			if(!fence) continue;
			const FenceHandle handle = fence->GetHandle();
			if(!device.DestroyFence(handle)) released = false;
			fence.reset();
		}
		m_queueFenceValues.fill(0);
		m_executionDevice = nullptr;
		return released;
	}

	const RenderGraphResourceLifetime& Lifetime(
		RenderGraphResource resource
	) const noexcept {
		static const RenderGraphResourceLifetime empty;
		return resource && resource.index < m_resourceLifetimes.size()
			? m_resourceLifetimes[resource.index]
			: empty;
	}

	bool IsImported(RenderGraphResource resource) const noexcept {
		return resource && resource.index < m_resources.size() &&
			m_resources[resource.index].IsBound();
	}

	bool IsTransient(RenderGraphResource resource) const noexcept {
		return resource && resource.index < m_resources.size() &&
			!m_resources[resource.index].IsBound();
	}

	ResourceQueueSharingMode QueueSharingModeForResource(
		RenderGraphResource resource
	) const noexcept {
		return resource && resource.index < m_resources.size()
			? m_resources[resource.index].queueSharingMode
			: ResourceQueueSharingMode::Exclusive;
	}

	bool CanAlias(
		RenderGraphResource lhs,
		RenderGraphResource rhs
	) const noexcept {
		if(lhs == rhs || !IsTransient(lhs) || !IsTransient(rhs)) return false;
		const RenderGraphResourceLifetime& left = Lifetime(lhs);
		const RenderGraphResourceLifetime& right = Lifetime(rhs);
		return left.IsUsed() && right.IsUsed() && !left.Overlaps(right);
	}

	ResourceState FinalState(RenderGraphResource resource) const noexcept {
		if(!resource || resource.index >= m_finalStates.size() || m_finalStates[resource.index].empty()){
			return ResourceState::Undefined;
		}
		const ResourceState state = m_finalStates[resource.index].front();
		for(ResourceState current : m_finalStates[resource.index]) if(current != state) return ResourceState::Undefined;
		return state;
	}
	ResourceState FinalState(RenderGraphResource resource, uint32_t subresource) const noexcept {
		if(!resource || resource.index >= m_finalStates.size() ||
			subresource >= m_finalStates[resource.index].size()) return ResourceState::Undefined;
		return m_finalStates[resource.index][subresource];
	}
	const std::string& Error() const noexcept { return m_error; }
	void Reset(){
		m_resources.clear();
		m_passes.clear();
		m_executionOrder.clear();
		m_resourceLifetimes.clear();
		m_queueSync.clear();
		m_finalStates.clear();
		m_error.clear();
		m_compiled = false;
	}

private:
	struct Resource {
		std::string name;
		BufferHandle buffer;
		TextureHandle texture;
		ResourceState initialState = ResourceState::Undefined;
		uint32_t subresourceCount = 1;
		ResourceQueueSharingMode queueSharingMode = ResourceQueueSharingMode::Exclusive;
		bool IsBound() const noexcept { return static_cast<bool>(buffer) != static_cast<bool>(texture); }
	};
	struct Pass {
		std::string name;
		CommandQueueType queue = CommandQueueType::Graphics;
		std::vector<RenderGraphAccess> accesses;
		std::string setupError;
		ExecuteCallback execute;
		std::vector<uint32_t> dependencies;
		std::vector<uint32_t> dependents;
		std::vector<ResourceBarrierDesc> barriers;
	};

	RenderGraphResource ImportBufferInternal(
		BufferHandle buffer,
		ResourceState initialState,
		ResourceQueueSharingMode queueSharingMode,
		std::string name
	){
		if(!buffer) return {};
		Resource resource;
		resource.name = std::move(name);
		resource.buffer = buffer;
		resource.initialState = initialState;
		resource.subresourceCount = 1;
		resource.queueSharingMode = queueSharingMode;
		m_resources.push_back(std::move(resource));
		m_compiled = false;
		return {static_cast<uint32_t>(m_resources.size() - 1)};
	}

	RenderGraphResource ImportTextureInternal(
		TextureHandle texture,
		ResourceState initialState,
		uint32_t subresourceCount,
		ResourceQueueSharingMode queueSharingMode,
		std::string name
	){
		if(!texture || subresourceCount == 0) return {};
		Resource resource;
		resource.name = std::move(name);
		resource.texture = texture;
		resource.initialState = initialState;
		resource.subresourceCount = subresourceCount;
		resource.queueSharingMode = queueSharingMode;
		m_resources.push_back(std::move(resource));
		m_compiled = false;
		return {static_cast<uint32_t>(m_resources.size() - 1)};
	}
	bool Fail(std::string message){ m_error = std::move(message); m_compiled = false; return false; }

	static size_t QueueIndex(CommandQueueType queue) noexcept {
		switch(queue){
			case CommandQueueType::Graphics: return 0;
			case CommandQueueType::Compute: return 1;
			case CommandQueueType::Copy: return 2;
		}
		return 0;
	}

	bool RecordPass(IRHICommandList& commandList, Pass& pass){
		if(!pass.barriers.empty() && !commandList.ResourceBarrier(pass.barriers)){
			m_error = pass.name + ": backend rejected generated resource barriers.";
			return false;
		}
		if(pass.execute) pass.execute(commandList);
		return true;
	}

	static bool Writes(RenderGraphAccessType type){ return type != RenderGraphAccessType::Read; }
	static bool RangesOverlap(const RenderGraphSubresourceRange& lhs,
		const RenderGraphSubresourceRange& rhs){
		if(lhs.IsAll() || rhs.IsAll()) return true;
		const uint64_t lhsEnd = static_cast<uint64_t>(lhs.first) + lhs.count;
		const uint64_t rhsEnd = static_cast<uint64_t>(rhs.first) + rhs.count;
		return lhs.first < rhsEnd && rhs.first < lhsEnd;
	}
	static bool ValidateRange(const Resource& resource, const RenderGraphSubresourceRange& range){
		if(range.IsAll()) return true;
		if(!resource.texture || range.count == 0 || range.first >= resource.subresourceCount) return false;
		return static_cast<uint64_t>(range.first) + range.count <= resource.subresourceCount;
	}
	bool Conflicts(const Pass& lhs, const Pass& rhs) const {
		for(const RenderGraphAccess& left : lhs.accesses){
			for(const RenderGraphAccess& right : rhs.accesses){
				if(left.resource == right.resource && RangesOverlap(left.subresources, right.subresources) &&
					(Writes(left.type) || Writes(right.type))) return true;
			}
		}
		return false;
	}
	bool ValidateQueueSharing(){
		std::vector<uint8_t> queueMasks(m_resources.size(), 0);
		for(const Pass& pass : m_passes){
			const uint8_t queueBit = static_cast<uint8_t>(1u << QueueIndex(pass.queue));
			for(const RenderGraphAccess& access : pass.accesses){
				queueMasks[access.resource.index] |= queueBit;
			}
		}

		for(size_t resourceIndex = 0; resourceIndex < m_resources.size(); ++resourceIndex){
			const uint8_t queueMask = queueMasks[resourceIndex];
			const bool usesMultipleQueues =
				queueMask != 0 &&
				(queueMask & static_cast<uint8_t>(queueMask - 1)) != 0;
			if(!usesMultipleQueues) continue;

			const Resource& resource = m_resources[resourceIndex];
			if(resource.queueSharingMode == ResourceQueueSharingMode::Concurrent) continue;

			const std::string resourceName = resource.name.empty()
				? std::string("RenderGraph resource #") + std::to_string(resourceIndex)
				: resource.name;
			return Fail(
				resourceName +
				": exclusive resource is used by multiple queue domains; declare Concurrent sharing."
			);
		}
		return true;
	}

	void BuildDependencies(){
		for(uint32_t earlier = 0; earlier < m_passes.size(); ++earlier){
			for(uint32_t later = earlier + 1; later < m_passes.size(); ++later){
				if(!Conflicts(m_passes[earlier], m_passes[later])) continue;
				m_passes[earlier].dependents.push_back(later);
				m_passes[later].dependencies.push_back(earlier);
			}
		}
	}
	bool BuildExecutionOrder(){
		std::vector<uint32_t> unresolved;
		std::vector<uint32_t> ready;
		for(uint32_t index = 0; index < m_passes.size(); ++index){
			unresolved.push_back(static_cast<uint32_t>(m_passes[index].dependencies.size()));
			if(unresolved.back() == 0) ready.push_back(index);
		}
		while(!ready.empty()){
			auto next = (std::min_element)(ready.begin(), ready.end());
			const uint32_t index = *next;
			ready.erase(next);
			m_executionOrder.push_back(index);
			for(uint32_t dependent : m_passes[index].dependents){
				if(unresolved[dependent] == 0) continue;
				--unresolved[dependent];
				if(unresolved[dependent] == 0) ready.push_back(dependent);
			}
		}
		return m_executionOrder.size() == m_passes.size();
	}
	void BuildResourceLifetimes(){
		m_resourceLifetimes.assign(m_resources.size(), {});
		std::vector<uint8_t> seenInPass(m_resources.size(), 0);

		for(uint32_t executionIndex = 0;
			executionIndex < m_executionOrder.size();
			++executionIndex){
			std::fill(seenInPass.begin(), seenInPass.end(), 0);
			const uint32_t passIndex = m_executionOrder[executionIndex];

			for(const RenderGraphAccess& access : m_passes[passIndex].accesses){
				const uint32_t resourceIndex = access.resource.index;
				if(seenInPass[resourceIndex]) continue;
				seenInPass[resourceIndex] = 1;

				RenderGraphResourceLifetime& lifetime =
					m_resourceLifetimes[resourceIndex];
				if(!lifetime.IsUsed()){
					lifetime.firstExecutionIndex = executionIndex;
					lifetime.firstPassIndex = passIndex;
				}
				lifetime.lastExecutionIndex = executionIndex;
				lifetime.lastPassIndex = passIndex;
				++lifetime.passUseCount;
			}
		}
	}

	void BuildQueueSynchronization(){
		m_queueSync.assign(m_passes.size(), {});
		std::array<uint64_t, 3> nextSignalValue{1, 1, 1};

		for(uint32_t passIndex : m_executionOrder){
			Pass& pass = m_passes[passIndex];
			RenderGraphPassQueueSync& sync = m_queueSync[passIndex];
			sync.queue = pass.queue;

			const bool hasCrossQueueDependent = std::any_of(
				pass.dependents.begin(),
				pass.dependents.end(),
				[this, &pass](uint32_t dependent){
					return m_passes[dependent].queue != pass.queue;
				}
			);
			if(hasCrossQueueDependent){
				sync.signalValue = nextSignalValue[QueueIndex(pass.queue)]++;
			}
		}

		for(uint32_t passIndex : m_executionOrder){
			const Pass& pass = m_passes[passIndex];
			RenderGraphPassQueueSync& sync = m_queueSync[passIndex];
			std::array<uint64_t, 3> waitValues{};
			std::array<uint32_t, 3> producers{
				InvalidRenderGraphIndex,
				InvalidRenderGraphIndex,
				InvalidRenderGraphIndex
			};

			for(uint32_t dependency : pass.dependencies){
				const Pass& producer = m_passes[dependency];
				if(producer.queue == pass.queue) continue;
				const size_t sourceIndex = QueueIndex(producer.queue);
				const uint64_t value = m_queueSync[dependency].signalValue;
				if(value > waitValues[sourceIndex]){
					waitValues[sourceIndex] = value;
					producers[sourceIndex] = dependency;
				}
			}

			for(size_t sourceIndex = 0; sourceIndex < waitValues.size(); ++sourceIndex){
				if(waitValues[sourceIndex] == 0) continue;
				sync.waits.push_back({
					m_passes[producers[sourceIndex]].queue,
					producers[sourceIndex],
					waitValues[sourceIndex]
				});
			}
		}
	}

	void AppendBarrier(Pass& pass, const Resource& resource, ResourceBarrierType type,
		ResourceState before, ResourceState after, uint32_t subresource){
		ResourceBarrierDesc barrier;
		barrier.type = type;
		barrier.buffer = resource.buffer;
		barrier.texture = resource.texture;
		barrier.before = before;
		barrier.after = after;
		barrier.subresource = subresource;
		pass.barriers.push_back(barrier);
	}
	void ProcessState(Pass& pass, const Resource& resource, const RenderGraphAccess& access,
		ResourceState& current, uint32_t subresource){
		if(current != access.state){
			AppendBarrier(pass, resource, ResourceBarrierType::Transition, current, access.state, subresource);
			current = access.state;
		}
		else if(Writes(access.type) && HasAnyFlag(access.state, ResourceState::UnorderedAccess)){
			AppendBarrier(pass, resource, ResourceBarrierType::UnorderedAccess, current, access.state, subresource);
		}
	}
	bool BuildBarriers(){
		m_finalStates.reserve(m_resources.size());
		for(const Resource& resource : m_resources){
			m_finalStates.emplace_back(resource.subresourceCount, resource.initialState);
		}
		for(uint32_t passIndex : m_executionOrder){
			Pass& pass = m_passes[passIndex];
			for(const RenderGraphAccess& access : pass.accesses){
				if(access.state == ResourceState::Undefined) continue;
				const Resource& resource = m_resources[access.resource.index];
				if(!resource.IsBound()) return Fail(pass.name + ": state was declared for an unbound logical resource.");
				auto& states = m_finalStates[access.resource.index];
				if(resource.buffer){
					ProcessState(pass, resource, access, states[0], AllSubresources);
					continue;
				}
				if(!access.subresources.IsAll()){
					const uint32_t end = access.subresources.first + access.subresources.count;
					for(uint32_t subresource = access.subresources.first; subresource < end; ++subresource){
						ProcessState(pass, resource, access, states[subresource], subresource);
					}
					continue;
				}
				const bool uniform = std::all_of(states.begin(), states.end(),
					[&](ResourceState state){ return state == states.front(); });
				if(uniform){
					ResourceState combined = states.front();
					const size_t beforeCount = pass.barriers.size();
					ProcessState(pass, resource, access, combined, AllSubresources);
					if(pass.barriers.size() != beforeCount) std::fill(states.begin(), states.end(), access.state);
				}
				else{
					for(uint32_t subresource = 0; subresource < states.size(); ++subresource){
						ProcessState(pass, resource, access, states[subresource], subresource);
					}
				}
			}
		}
		return true;
	}

	std::vector<Resource> m_resources;
	std::vector<Pass> m_passes;
	std::vector<uint32_t> m_executionOrder;
	std::vector<RenderGraphResourceLifetime> m_resourceLifetimes;
	std::vector<RenderGraphPassQueueSync> m_queueSync;
	std::vector<std::vector<ResourceState>> m_finalStates;
	IRHIDevice* m_executionDevice = nullptr;
	std::array<std::unique_ptr<IRHIFence>, 3> m_queueFences;
	std::array<uint64_t, 3> m_queueFenceValues{};
	std::string m_error;
	bool m_compiled = false;
};

} // namespace RHI
