// =======================================================================
//
// SystemIntrospection.cpp
//
// =======================================================================
#include "SystemIntrospection.h"

#include <string>
#include <typeindex>
#include <unordered_set>
#include <vector>

#include "Scene/scene.h"
#include "Scene/Registry/componentRegistry.h"
#include "Scene/Registry/systemRegistry.h"

namespace agentos {

namespace {

const char* DomainName(SystemTaskDomain domain) {
	switch(domain) {
	case SystemTaskDomain::Fixed:  return "Fixed";
	case SystemTaskDomain::Frame:  return "Frame";
	case SystemTaskDomain::Editor: return "Editor";
	case SystemTaskDomain::Render: return "Render";
	}
	return "Unknown";
}

const char* PhaseName(SystemPhase phase) {
	switch(phase) {
	case SystemPhase::Earliest: return "Earliest";
	case SystemPhase::Early:    return "Early";
	case SystemPhase::Default:  return "Default";
	case SystemPhase::Late:     return "Late";
	case SystemPhase::Latest:   return "Latest";
	}
	return "Custom";
}

const char* AffinityName(ThreadAffinity affinity) {
	switch(affinity) {
	case ThreadAffinity::AnyWorker:   return "AnyWorker";
	case ThreadAffinity::MainThread:  return "MainThread";
	case ThreadAffinity::RenderThread:return "RenderThread";
	}
	return "Unknown";
}

const char* StructuralAccessName(StructuralAccess access) {
	switch(access) {
	case StructuralAccess::None:                return "None";
	case StructuralAccess::EmitCommands:        return "EmitCommands";
	case StructuralAccess::ExclusiveWorldWrite: return "ExclusiveWorldWrite";
	}
	return "Unknown";
}

const char* WorldAccessName(WorldAccessMode access) {
	switch(access) {
	case WorldAccessMode::None:      return "None";
	case WorldAccessMode::Exclusive: return "Exclusive";
	}
	return "Unknown";
}

// type_index集合をComponentRegistry経由で表示名の配列へ変換する。
// 未登録の型（Resource等、YAML Componentとして登録されていない型）は
// "raw:<type_index.name()>" のままフォールバックする。
Json ComponentSetToNames(
	const std::unordered_set<std::type_index>& types,
	ComponentRegistry& components
) {
	Json array = Json::array();
	for(const std::type_index& typeIndex : types){
		const ComponentTypeID typeID = components.GetComponentIDByTypeIndex(typeIndex);
		std::string name = (typeID != INVALID_COMPONENT_TYPE_ID)
			? components.GetComponentName(typeID)
			: std::string();
		if(name.empty()){
			name = std::string("raw:") + typeIndex.name();
		}
		array.push_back(name);
	}
	return array;
}

} // namespace

Json ExportSystemDescriptors(SystemRegistry& registry, ComponentRegistry& components) {
	Json result = Json::object();

	const std::vector<SystemTask>& tasks = registry.GetTasks();

	Json tasksJson = Json::array();
	for(const SystemTask& task : tasks){
		Json taskJson = Json::object();
		taskJson["name"] = task.name;
		taskJson["domain"] = DomainName(task.domain);
		taskJson["phase"] = PhaseName(task.order.phase);
		taskJson["priority"] = task.order.priority;
		taskJson["threadAffinity"] = AffinityName(task.threadAffinity);
		taskJson["structuralAccess"] = StructuralAccessName(task.access.structuralAccess);
		taskJson["worldAccess"] = WorldAccessName(task.access.worldAccess);
		taskJson["componentReads"] = ComponentSetToNames(task.access.componentReads, components);
		taskJson["componentWrites"] = ComponentSetToNames(task.access.componentWrites, components);
		tasksJson.push_back(taskJson);
	}
	result["tasks"] = tasksJson;
	result["taskCount"] = tasks.size();

	// Domainごとのコンパイル済みSchedule依存辺（Access競合由来）をTask名のペアで出力する。
	Json edgesJson = Json::array();
	const SystemTaskDomain domains[] = {
		SystemTaskDomain::Fixed,
		SystemTaskDomain::Frame,
		SystemTaskDomain::Editor,
		SystemTaskDomain::Render
	};
	for(SystemTaskDomain domain : domains){
		const CompiledSystemSchedule& schedule = registry.GetSchedule(domain);
		for(const SystemScheduleNode& node : schedule.nodes){
			for(std::size_t dependencyNodeIndex : node.dependencies){
				Json edge = Json::object();
				edge["domain"] = DomainName(domain);
				edge["from"] = tasks[schedule.nodes[dependencyNodeIndex].taskIndex].name;
				edge["to"] = tasks[node.taskIndex].name;
				edgesJson.push_back(edge);
			}
		}
	}
	result["edges"] = edgesJson;

	return result;
}

Json ExportSystemDescriptors(SceneContext& context) {
	if(!context.system || !context.component){
		Json error = Json::object();
		error["error"] = "system or component registry unavailable";
		return error;
	}
	return ExportSystemDescriptors(*context.system, *context.component);
}

} // namespace agentos
