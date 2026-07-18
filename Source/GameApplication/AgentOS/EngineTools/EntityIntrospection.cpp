// =======================================================================
//
// EntityIntrospection.cpp
//
// =======================================================================
#include "EntityIntrospection.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "Scene/scene.h"
#include "Scene/Registry/componentRegistry.h"
#include "Scene/Registry/entityRegistry.h"
#include "Scene/Component/entityNameComponent.h"

namespace agentos {

namespace {

// pass2（Component型一致）でComponentScoreに掛ける減衰係数。
// 名前直接一致より確度を落として扱う（Componentを持つだけでは名前一致ほど確実ではないため）。
constexpr double kComponentMatchDamping = 0.9;

// ResolveEntityCandidatesが候補として採用する最低スコア。
// FuzzyMatch::RankCandidates/RankComponentCandidatesの既定値(0.3)と揃える。
constexpr double kMinCandidateScore = 0.3;

// YAML Scalarをint64 -> double -> bool -> stringの順で解釈する。
// yaml-cppのas<T>()は変換不能な場合にYAML::BadConversionを投げるので、
// 例外を利用して最初に成功した型を採用する。
Json ScalarToJson(const YAML::Node& node) {
	try {
		return Json(node.as<std::int64_t>());
	} catch(...) {}

	try {
		return Json(node.as<double>());
	} catch(...) {}

	try {
		return Json(node.as<bool>());
	} catch(...) {}

	try {
		return Json(node.as<std::string>());
	} catch(...) {}

	return Json(nullptr);
}

} // namespace

Json YamlToJson(const YAML::Node& node) {
	if(!node || node.IsNull()) return Json(nullptr);

	switch(node.Type()) {
	case YAML::NodeType::Scalar:
		return ScalarToJson(node);

	case YAML::NodeType::Sequence: {
		Json array = Json::array();
		for(const auto& child : node){
			array.push_back(YamlToJson(child));
		}
		return array;
	}

	case YAML::NodeType::Map: {
		Json object = Json::object();
		for(const auto& keyValue : node){
			object[keyValue.first.as<std::string>()] = YamlToJson(keyValue.second);
		}
		return object;
	}

	case YAML::NodeType::Null:
	case YAML::NodeType::Undefined:
	default:
		return Json(nullptr);
	}
}

Json ListEntities(SceneContext& context, std::size_t maxCount) {
	Json result = Json::array();
	if(!context.entity) return result;

	std::vector<Entity> entities(
		context.entity->GetAllAlive().begin(),
		context.entity->GetAllAlive().end()
	);
	std::sort(
		entities.begin(),
		entities.end(),
		[](Entity lhs, Entity rhs){ return lhs.GetIndex() < rhs.GetIndex(); }
	);

	for(Entity entity : entities){
		if(result.size() >= maxCount) break;

		Json entry = Json::object();
		entry["id"] = entity.GetIndex();
		entry["generation"] = entity.GetGeneration();

		if(context.component && context.component->HasComponent<NameComponent>(entity)){
			const NameComponent* name = context.component->GetComponent<NameComponent>(entity);
			if(name) entry["name"] = name->name;
		}

		result.push_back(entry);
	}
	return result;
}

std::optional<Entity> FindEntityByName(SceneContext& context, const std::string& name) {
	if(!context.entity || !context.component) return std::nullopt;

	for(Entity entity : context.entity->GetAllAlive()){
		if(!context.component->HasComponent<NameComponent>(entity)) continue;

		const NameComponent* nameComponent = context.component->GetComponent<NameComponent>(entity);
		if(nameComponent && nameComponent->name == name){
			return entity;
		}
	}
	return std::nullopt;
}

std::vector<EntityCandidate> ResolveEntityCandidates(
	SceneContext& context,
	const std::string& query,
	std::size_t maxResults
) {
	std::unordered_map<Entity, EntityCandidate> best;

	if(!context.entity || !context.component || query.empty()) return {};

	// pass1: NameComponent.nameの直接Fuzzy Match。
	for(Entity entity : context.entity->GetAllAlive()){
		if(!context.component->HasComponent<NameComponent>(entity)) continue;
		const NameComponent* nameComponent = context.component->GetComponent<NameComponent>(entity);
		if(!nameComponent) continue;

		const fuzzy::Match match = fuzzy::ScoreNameMatch(query, nameComponent->name);
		if(match.score < kMinCandidateScore) continue;

		EntityCandidate candidate{entity, nameComponent->name, match.matchType, match.score};
		auto existing = best.find(entity);
		if(existing == best.end() || candidate.score > existing->second.score){
			best[entity] = candidate;
		}
	}

	// pass2: Component型名がqueryにFuzzy Matchした場合、そのComponentを
	// 実際に持つ全EntityをmatchedBy="component:<name>"として候補化する
	// （例:「Light」→LightComponentがアタッチされたEntity全て）。
	for(const auto& [typeID, componentName] : context.component->GetComponentIDToNameMap()){
		const double componentScore = fuzzy::ScoreComponentName(query, componentName);
		if(componentScore < kMinCandidateScore) continue;

		const double score = componentScore * kComponentMatchDamping;
		const std::string matchedBy = "component:" + componentName;

		for(Entity entity : context.entity->GetAllAlive()){
			const ComponentView view = context.component->GetComponentByID(entity, typeID);
			if(!view) continue;

			std::string entityName;
			if(context.component->HasComponent<NameComponent>(entity)){
				if(const NameComponent* nameComponent = context.component->GetComponent<NameComponent>(entity)){
					entityName = nameComponent->name;
				}
			}

			EntityCandidate candidate{entity, entityName, matchedBy, score};
			auto existing = best.find(entity);
			if(existing == best.end() || candidate.score > existing->second.score){
				best[entity] = candidate;
			}
		}
	}

	std::vector<EntityCandidate> candidates;
	candidates.reserve(best.size());
	for(auto& [entity, candidate] : best){
		(void)entity;
		candidates.push_back(std::move(candidate));
	}

	std::stable_sort(
		candidates.begin(),
		candidates.end(),
		[](const EntityCandidate& lhs, const EntityCandidate& rhs){
			if(lhs.score != rhs.score) return lhs.score > rhs.score;
			return lhs.entity.GetIndex() < rhs.entity.GetIndex();
		}
	);

	if(candidates.size() > maxResults) candidates.resize(maxResults);
	return candidates;
}

std::vector<fuzzy::Match> ResolveComponentCandidates(
	SceneContext& context,
	const std::string& query,
	std::size_t maxResults
) {
	if(!context.component || query.empty()) return {};

	std::vector<std::string> componentNames;
	componentNames.reserve(context.component->GetComponentIDToNameMap().size());
	for(const auto& [typeID, componentName] : context.component->GetComponentIDToNameMap()){
		(void)typeID;
		componentNames.push_back(componentName);
	}

	return fuzzy::RankComponentCandidates(query, componentNames, maxResults, kMinCandidateScore);
}

Json DescribeEntity(SceneContext& context, Entity entity) {
	Json result = Json::object();

	if(!context.entity || !context.entity->IsAlive(entity)){
		result["error"] = "entity is not alive";
		return result;
	}

	result["id"] = entity.GetIndex();
	result["generation"] = entity.GetGeneration();

	Json components = Json::array();
	if(context.component){
		for(const ComponentView& view : context.component->GetAllComponentsOfEntitySorted(entity)){
			Json componentJson = Json::object();
			componentJson["component"] = context.component->GetComponentName(view.typeID);
			componentJson["value"] = YamlToJson(context.component->EncodeComponent(view));
			components.push_back(componentJson);
		}
	}
	result["components"] = components;
	return result;
}

Json ReadComponent(SceneContext& context, Entity entity, const std::string& componentName) {
	Json result = Json::object();

	if(!context.entity || !context.entity->IsAlive(entity)){
		result["error"] = "entity is not alive";
		return result;
	}
	if(!context.component){
		result["error"] = "component registry is unavailable";
		return result;
	}

	const ComponentTypeID typeID = context.component->GetComponentIDByName(componentName);
	if(typeID == INVALID_COMPONENT_TYPE_ID){
		result["error"] = "unknown component: " + componentName;

		// 候補（ResolveComponentCandidates）を添えて、Repair/LLM層が
		// 「PlayerComponent」のようなタイプミス・省略形から自己修正できるようにする。
		Json candidates = Json::array();
		for(const fuzzy::Match& match : ResolveComponentCandidates(context, componentName, 5)){
			candidates.push_back(Json::object({
				{"name", match.candidate},
				{"matchType", match.matchType},
				{"score", match.score},
			}));
		}
		result["componentCandidates"] = candidates;
		return result;
	}

	ComponentView view = context.component->GetComponentByID(entity, typeID);
	if(!view){
		result["error"] = "entity does not have component: " + componentName;
		return result;
	}

	result["component"] = componentName;
	result["value"] = YamlToJson(context.component->EncodeComponent(view));
	return result;
}

} // namespace agentos
