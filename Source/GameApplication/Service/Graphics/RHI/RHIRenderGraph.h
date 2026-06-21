// =======================================================================
// RHIRenderGraph.h
// =======================================================================
#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
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
};

class RenderGraphPassBuilder {
public:
	void Read(RenderGraphResource resource){ Add(resource, RenderGraphAccessType::Read); }
	void Write(RenderGraphResource resource){ Add(resource, RenderGraphAccessType::Write); }
	void ReadWrite(RenderGraphResource resource){ Add(resource, RenderGraphAccessType::ReadWrite); }

	const std::vector<RenderGraphAccess>& Accesses() const noexcept {
		return m_accesses;
	}

private:
	void Add(RenderGraphResource resource, RenderGraphAccessType type){
		if(!resource) return;
		for(RenderGraphAccess& access : m_accesses){
			if(access.resource != resource) continue;
			if(access.type != type) access.type = RenderGraphAccessType::ReadWrite;
			return;
		}
		m_accesses.push_back({resource, type});
	}

	std::vector<RenderGraphAccess> m_accesses;
};

class RenderGraph {
public:
	using ExecuteCallback = std::function<void(IRHICommandList&)>;

	RenderGraphResource AddResource(std::string name = {}){
		m_resourceNames.push_back(std::move(name));
		m_compiled = false;
		return {static_cast<uint32_t>(m_resourceNames.size() - 1)};
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
		pass.execute = std::move(execute);
		m_passes.push_back(std::move(pass));
		m_compiled = false;
		return static_cast<uint32_t>(m_passes.size() - 1);
	}

	bool Compile(){
		m_executionOrder.clear();
		m_error.clear();
		for(Pass& pass : m_passes){
			pass.dependencies.clear();
			pass.dependents.clear();
		}

		for(uint32_t earlier = 0; earlier < m_passes.size(); ++earlier){
			for(uint32_t later = earlier + 1; later < m_passes.size(); ++later){
				if(!Conflicts(m_passes[earlier], m_passes[later])) continue;
				m_passes[earlier].dependents.push_back(later);
				m_passes[later].dependencies.push_back(earlier);
			}
		}

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

		m_compiled = m_executionOrder.size() == m_passes.size();
		if(!m_compiled) m_error = "RenderGraph dependency cycle detected.";
		return m_compiled;
	}

	bool Execute(IRHICommandList& commandList){
		if(!m_compiled && !Compile()) return false;
		commandList.Begin();
		for(uint32_t passIndex : m_executionOrder){
			if(m_passes[passIndex].execute){
				m_passes[passIndex].execute(commandList);
			}
		}
		commandList.End();
		return true;
	}

	const std::vector<uint32_t>& ExecutionOrder() const noexcept {
		return m_executionOrder;
	}

	const std::string& Error() const noexcept { return m_error; }

	void Reset(){
		m_resourceNames.clear();
		m_passes.clear();
		m_executionOrder.clear();
		m_error.clear();
		m_compiled = false;
	}

private:
	struct Pass {
		std::string name;
		std::vector<RenderGraphAccess> accesses;
		ExecuteCallback execute;
		std::vector<uint32_t> dependencies;
		std::vector<uint32_t> dependents;
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

	std::vector<std::string> m_resourceNames;
	std::vector<Pass> m_passes;
	std::vector<uint32_t> m_executionOrder;
	std::string m_error;
	bool m_compiled = false;
};

} // namespace RHI
