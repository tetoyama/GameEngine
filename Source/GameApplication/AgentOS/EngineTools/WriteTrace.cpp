// =======================================================================
//
// WriteTrace.cpp
//
// =======================================================================
#include "WriteTrace.h"

#include <algorithm>
#include <typeindex>
#include <unordered_set>

#include <yaml-cpp/yaml.h>

#include "EntityIntrospection.h" // YamlToJson
#include "Scene/scene.h"
#include "Scene/Registry/componentRegistry.h"
#include "Scene/Registry/systemRegistry.h"

namespace agentos {

namespace {

// before/afterのJSONツリーを比較し、変化したリーフを {path, before, after} として集める。
// Object/Arrayはキー/添字を経路(path)に連結して辿る。型が変わった場合もリーフ差分として扱う。
void DiffJson(const Json& before, const Json& after, const std::string& path, Json& changesOut) {
	if(before == after) return;

	const bool beforeContainer = before.is_object() || before.is_array();
	const bool afterContainer = after.is_object() || after.is_array();

	if(beforeContainer && afterContainer && before.type() == after.type()){
		if(before.is_object()){
			std::unordered_set<std::string> keys;
			for(auto it = before.begin(); it != before.end(); ++it) keys.insert(it.key());
			for(auto it = after.begin(); it != after.end(); ++it) keys.insert(it.key());

			for(const std::string& key : keys){
				const Json childBefore = before.contains(key) ? before.at(key) : Json(nullptr);
				const Json childAfter = after.contains(key) ? after.at(key) : Json(nullptr);
				const std::string childPath = path.empty() ? key : (path + "." + key);
				DiffJson(childBefore, childAfter, childPath, changesOut);
			}
			return;
		}

		// Sequence
		const std::size_t count = (std::max)(before.size(), after.size());
		for(std::size_t index = 0; index < count; ++index){
			const Json childBefore = index < before.size() ? before.at(index) : Json(nullptr);
			const Json childAfter = index < after.size() ? after.at(index) : Json(nullptr);
			const std::string childPath = path + "[" + std::to_string(index) + "]";
			DiffJson(childBefore, childAfter, childPath, changesOut);
		}
		return;
	}

	Json entry = Json::object();
	entry["path"] = path.empty() ? std::string("$") : path;
	entry["before"] = before;
	entry["after"] = after;
	changesOut.push_back(entry);
}

} // namespace

void WriteTracer::SetTarget(Entity entity, std::string componentName, SceneContext* sceneContext) {
	m_entity = entity;
	m_componentName = std::move(componentName);
	m_sceneContext = sceneContext;
	m_hasPrevious = false;
	m_previous = Json(nullptr);
	m_active = true;
}

void WriteTracer::Clear() {
	m_active = false;
	m_entity = Entity{};
	m_componentName.clear();
	m_sceneContext = nullptr;
	m_hasPrevious = false;
	m_previous = Json(nullptr);
	m_events.clear();
}

void WriteTracer::Stop() {
	m_active = false;
}

Json WriteTracer::CollectSuspectedWriters(ComponentTypeID targetTypeID) const {
	Json writers = Json::array();
	if(!m_sceneContext || !m_sceneContext->system || !m_sceneContext->component) return writers;

	for(const SystemTask& task : m_sceneContext->system->GetTasks()){
		for(const std::type_index& typeIndex : task.access.componentWrites){
			if(m_sceneContext->component->GetComponentIDByTypeIndex(typeIndex) == targetTypeID){
				writers.push_back(task.name);
				break;
			}
		}
	}
	return writers;
}

void WriteTracer::Sample(std::int64_t frame) {
	if(!m_active || !m_sceneContext || !m_sceneContext->component) return;

	const ComponentTypeID typeID = m_sceneContext->component->GetComponentIDByName(m_componentName);
	if(typeID == INVALID_COMPONENT_TYPE_ID) return;

	ComponentView view = m_sceneContext->component->GetComponentByID(m_entity, typeID);
	if(!view){
		// Entity/Componentが消えている場合は、次にComponentが戻ってきたときに
		// そこから改めて差分検出を始める（誤った「消滅イベント」を記録しない）。
		m_hasPrevious = false;
		return;
	}

	const YAML::Node node = m_sceneContext->component->EncodeComponent(view);
	const Json current = YamlToJson(node);

	if(m_hasPrevious){
		Json changes = Json::array();
		DiffJson(m_previous, current, std::string(), changes);

		if(!changes.empty()){
			Json record = Json::object();
			record["frame"] = frame;
			record["componentName"] = m_componentName;
			record["changedFields"] = changes;
			record["suspectedWriters"] = CollectSuspectedWriters(typeID);
			record["attribution"] = "estimated";

			m_events.push_back(record);
			while(m_events.size() > kMaxEvents){
				m_events.pop_front();
			}
		}
	}

	m_previous = current;
	m_hasPrevious = true;
}

Json WriteTracer::GetTrace() const {
	Json array = Json::array();
	for(const Json& event : m_events){
		array.push_back(event);
	}
	return array;
}

} // namespace agentos
