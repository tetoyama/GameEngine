#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "RHIInterfaces.h"

namespace RHI {

struct RenderGraphResource {
	uint32_t index = (std::numeric_limits<uint32_t>::max)();
	constexpr explicit operator bool() const noexcept { return index != (std::numeric_limits<uint32_t>::max)(); }
	friend constexpr bool operator==(const RenderGraphResource&, const RenderGraphResource&) noexcept = default;
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

	RenderGraphResource AddResource(std::string name = {}){
		Resource resource;
		resource.name = std::move(name);
		m_resources.push_back(std::move(resource));
		m_compiled = false;
		return {static_cast<uint32_t>(m_resources.size() - 1)};
	}

	RenderGraphResource ImportBuffer(BufferHandle buffer, ResourceState initialState, std::string name = {}){
		if(!buffer) return {};
		Resource resource;
		resource.name = std::move(name);
		resource.buffer = buffer;
		resource.initialState = initialState;
		resource.subresourceCount = 1;
		m_resources.push_back(std::move(resource));
		m_compiled = false;
		return {static_cast<uint32_t>(m_resources.size() - 1)};
	}

	RenderGraphResource ImportTexture(TextureHandle texture, ResourceState initialState, std::string name = {}){
		return ImportTextureInternal(texture, initialState, 1, std::move(name));
	}

	RenderGraphResource ImportTexture(TextureHandle texture, ResourceState initialState,
		const TextureDesc& desc, std::string name = {}){
		const uint32_t count = (std::max)(1u,
			static_cast<uint32_t>(desc.mipLevels) * static_cast<uint32_t>(desc.arraySize));
		return ImportTextureInternal(texture, initialState, count, std::move(name));
	}

	uint32_t AddPass(std::string name, const std::function<void(RenderGraphPassBuilder&)>& setup,
		ExecuteCallback execute){
		RenderGraphPassBuilder builder;
		if(setup) setup(builder);
		Pass pass;
		pass.name = std::move(name);
		pass.accesses = builder.Accesses();
		pass.setupError = builder.Error();
		pass.execute = std::move(execute);
		m_passes.push_back(std::move(pass));
		m_compiled = false;
		return static_cast<uint32_t>(m_passes.size() - 1);
	}

	bool Compile(){
		m_executionOrder.clear();
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
		BuildDependencies();
		if(!BuildExecutionOrder()) return Fail("RenderGraph dependency cycle detected.");
		if(!BuildBarriers()) return false;
		m_compiled = true;
		return true;
	}

	bool Execute(IRHICommandList& commandList){
		if(!m_compiled && !Compile()) return false;
		commandList.Begin();
		for(uint32_t passIndex : m_executionOrder){
			Pass& pass = m_passes[passIndex];
			if(!pass.barriers.empty() && !commandList.ResourceBarrier(pass.barriers)){
				commandList.End();
				m_error = pass.name + ": backend rejected generated resource barriers.";
				return false;
			}
			if(pass.execute) pass.execute(commandList);
		}
		commandList.End();
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
		bool IsBound() const noexcept { return static_cast<bool>(buffer) != static_cast<bool>(texture); }
	};
	struct Pass {
		std::string name;
		std::vector<RenderGraphAccess> accesses;
		std::string setupError;
		ExecuteCallback execute;
		std::vector<uint32_t> dependencies;
		std::vector<uint32_t> dependents;
		std::vector<ResourceBarrierDesc> barriers;
	};

	RenderGraphResource ImportTextureInternal(TextureHandle texture, ResourceState initialState,
		uint32_t subresourceCount, std::string name){
		if(!texture || subresourceCount == 0) return {};
		Resource resource;
		resource.name = std::move(name);
		resource.texture = texture;
		resource.initialState = initialState;
		resource.subresourceCount = subresourceCount;
		m_resources.push_back(std::move(resource));
		m_compiled = false;
		return {static_cast<uint32_t>(m_resources.size() - 1)};
	}
	bool Fail(std::string message){ m_error = std::move(message); m_compiled = false; return false; }
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
	std::vector<std::vector<ResourceState>> m_finalStates;
	std::string m_error;
	bool m_compiled = false;
};

} // namespace RHI
