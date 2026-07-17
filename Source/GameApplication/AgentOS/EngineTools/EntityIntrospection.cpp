// =======================================================================
//
// EntityIntrospection.cpp
//
// =======================================================================
#include "EntityIntrospection.h"

#include <algorithm>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "Scene/scene.h"
#include "Scene/Registry/componentRegistry.h"
#include "Scene/Registry/entityRegistry.h"
#include "Scene/Component/entityNameComponent.h"

namespace agentos {

namespace {

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
