// =======================================================================
// RHIRenderGraph.h
// =======================================================================
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

	constexpr explicit operator bool() const noexcept {
		return index != (std::numeric_limits<uint32_t>::max)();
	}

	friend constexpr bool operator==(
		const RenderGraphResource& lhs,
		const RenderGraphResource& rhs
	) noexcept = default;
};

enum class RenderGraphAccessType : uint8_t {
	Read,
	Write,
	ReadWrite
};

struct RenderGraphAccess {
	RenderGraphResource resource;
	RenderGraphAccessType type = RenderGraphAccessType::Read;
	ResourceState state = ResourceState::Undefined;
};

class RenderGraphPassBuilder {
public:
	void Read(
		RenderGraphResource resource,
		ResourceState state = ResourceState::Undefined
	){
		Add(resource, RenderGraphAccessType::Read, state);
	}

	void Write(
		RenderGraphResource resource,
		ResourceState state = ResourceState::Undefined
	){
		Add(resource, RenderGraphAccessType::Write, state);
	}

	void ReadWrite(
		RenderGraphResource resource,
		ResourceState state = ResourceState::Undefined
	){
		Add(resource, RenderGraphAccessType::ReadWrite, state);
	}

	const std::vector<RenderGraphAccess>& Accesses() const noexcept {
		return m_accesses;
	}

	bool IsValid() const noexcept { return m_error.empty(); }
	const std::string& Error() const noexcept { return m_error; }

private:
	void Add(
		RenderGraphResource resource,
		RenderGraphAccessType type,
		ResourceState state
	){
		if(!resource) return;
		for(RenderGraphAccess& access : m_accesses){
			if(access.resource != resource) continue;

			if(access.type != type){
				access.type = RenderGraphAccessType::ReadWrite;
			}

			if(access.state == ResourceState::Undefined){
				access.state = state;
			}
			else if(state != ResourceState::Undefined && access.state != state){
				m_error = "RenderGraph pass requested conflicting states for one resource.";
			}
			return;
		}
		m_accesses.push_back({resource, type, state});
	}

	std::vector<RenderGraphAccess> m_accesses;
	std::string m_error;
};

class RenderGraph {
public:
	using ExecuteCallback = std::function<void(IRHICommandList&)>;

	// Dependency-only logical resource. Stateを省略した従来Passと互換。
	RenderGraphResource AddResource(std::string name = {}){
		Resource resource;
		resource.name = std::move(name);
		m_resources.push_back(std::move(resource));
		m_compiled = false;
		return {static_cast<uint32_t>(m_resources.size() - 1)};
	}

	// 外部BufferをGraphへ取り込み、初期状態からPass間Barrierを生成する。
	RenderGraphResource ImportBuffer(
		BufferHandle buffer,
		ResourceState initialState,
		std::string name = {}
	){
		if(!buffer) return {};
		Resource resource;
		resource.name = std::move(name);
		resource.buffer = buffer;
		resource.initialState = initialState;
		m_resources.push_back(std::move(resource));
		m_compiled = false;
		return {static_cast<uint32_t>(m_resources.size() - 1)};
	}

	// 外部TextureをGraphへ取り込み、初期状態からPass間Barrierを生成する。
	RenderGraphResource ImportTexture(
		TextureHandle texture,
		ResourceState initialState,
		std::string name = {}
	){
		if(!texture) return {};
		Resource resource;
		resource.name = std::move(name);
		resource.texture = texture;
		resource.initialState = initialState;
		m_resources.push_back(std::move(resource));
		m_compiled = false;
		return {static_cast<uint32_t>(m_resources.size() - 1)};
	}

	uint32_t AddPass(
		std::string name,
		const std::function<void(RenderGraphPassBuilder&)>& setup,
		ExecuteCallback execute
	){
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
			if(!pass.setupError.empty()){
				m_error = pass.name + ": " + pass.setupError;
				m_compiled = false;
				return false;
			}
			for(const RenderGraphAccess& access : pass.accesses){
				if(!access.resource || access.resource.index >= m_resources.size()){
					m_error = pass.name + ": invalid RenderGraph resource.";
					m_compiled = false;
					return false;
				}
			}
		}

		BuildDependencies();
		if(!BuildExecutionOrder()){
			m_error = "RenderGraph dependency cycle detected.";
			m_compiled = false;
			return false;
		}

		if(!BuildBarriers()){
			m_compiled = false;
			return false;
		}

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

	const std::vector<uint32_t>& ExecutionOrder() const noexcept {
		return m_executionOrder;
	}

	const std::vector<ResourceBarrierDesc>& BarriersForPass(
		uint32_t passIndex
	) const noexcept {
		static const std::vector<ResourceBarrierDesc> empty;
		return passIndex < m_passes.size()
			? m_passes[passIndex].barriers
			: empty;
	}

	ResourceState FinalState(RenderGraphResource resource) const noexcept {
		if(!resource || resource.index >= m_finalStates.size()){
			return ResourceState::Undefined;
		}
		return m_finalStates[resource.index];
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

		bool IsBound() const noexcept {
			return static_cast<bool>(buffer) != static_cast<bool>(texture);
		}
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

	static bool Writes(RenderGraphAccessType type){
		return type != RenderGraphAccessType::Read;
	}

	static bool Conflicts(const Pass& lhs, const Pass& rhs){
		for(const RenderGraphAccess& left : lhs.accesses){
			for(const RenderGraphAccess& right : rhs.accesses){
				if(left.resource == right.resource &&
					(Writes(left.type) || Writes(right.type))){
					return true;
				}
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
			unresolved.push_back(
				static_cast<uint32_t>(m_passes[index].dependencies.size())
			);
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

	bool BuildBarriers(){
		m_finalStates.reserve(m_resources.size());
		for(const Resource& resource : m_resources){
			m_finalStates.push_back(resource.initialState);
		}

		for(uint32_t passIndex : m_executionOrder){
			Pass& pass = m_passes[passIndex];
			for(const RenderGraphAccess& access : pass.accesses){
				if(access.state == ResourceState::Undefined) continue;

				const Resource& resource = m_resources[access.resource.index];
				if(!resource.IsBound()){
					m_error = pass.name +
						": state was declared for an unbound logical resource.";
					return false;
				}

				ResourceState& current = m_finalStates[access.resource.index];
				ResourceBarrierDesc barrier;
				barrier.buffer = resource.buffer;
				barrier.texture = resource.texture;
				barrier.before = current;
				barrier.after = access.state;

				if(current != access.state){
					barrier.type = ResourceBarrierType::Transition;
					pass.barriers.push_back(barrier);
					current = access.state;
				}
				else if(Writes(access.type) && HasAnyFlag(
					access.state,
					ResourceState::UnorderedAccess
				)){
					barrier.type = ResourceBarrierType::UnorderedAccess;
					pass.barriers.push_back(barrier);
				}
			}
		}
		return true;
	}

	std::vector<Resource> m_resources;
	std::vector<Pass> m_passes;
	std::vector<uint32_t> m_executionOrder;
	std::vector<ResourceState> m_finalStates;
	std::string m_error;
	bool m_compiled = false;
};

} // namespace RHI
