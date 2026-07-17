// =======================================================================
//
// ComponentQueryRouting.h
//
// Entity / Componentの読み取り要求を、LLM解釈に依存せず既存Toolへ束縛する。
// =======================================================================
#pragma once

#include <cctype>
#include <initializer_list>
#include <string>

#include "../Json.h"

namespace agentos::componentquery {

struct Route {
	std::string tool;
	Json arguments = Json::object();

	bool IsValid() const noexcept { return !tool.empty(); }
};

inline bool ContainsAny(
	const std::string& text,
	std::initializer_list<const char*> needles) {
	for(const char* needle : needles) {
		if(needle != nullptr && text.find(needle) != std::string::npos) return true;
	}
	return false;
}

inline std::string ExtractQuotedEntity(const std::string& request) {
	struct Marker {
		const char* prefix;
		char suffix;
	};
	constexpr Marker markers[] = {
		{"Entity '", '\''},
		{"entity '", '\''},
		{"Entity \"", '\"'},
		{"entity \"", '\"'},
	};
	for(const Marker& marker : markers) {
		const std::size_t begin = request.find(marker.prefix);
		if(begin == std::string::npos) continue;
		const std::size_t valueBegin =
			begin + std::char_traits<char>::length(marker.prefix);
		const std::size_t end = request.find(marker.suffix, valueBegin);
		if(end != std::string::npos && end > valueBegin) {
			return request.substr(valueBegin, end - valueBegin);
		}
	}
	return {};
}

inline std::string AsciiIdentifierBefore(
	const std::string& text,
	std::size_t position) {
	std::size_t end = position;
	while(end > 0 &&
		std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
		--end;
	}

	std::size_t begin = end;
	while(begin > 0) {
		const unsigned char ch = static_cast<unsigned char>(text[begin - 1]);
		if(std::isalnum(ch) == 0 && ch != '_') break;
		--begin;
	}
	return begin < end ? text.substr(begin, end - begin) : std::string();
}

inline std::string ExtractEntityBeforeParticle(const std::string& request) {
	for(const char* marker : {
		"になん", "に何", "のコンポーネント", "のComponent"}) {
		const std::size_t position = request.find(marker);
		if(position == std::string::npos) continue;
		const std::string entity = AsciiIdentifierBefore(request, position);
		if(!entity.empty()) return entity;
	}
	return {};
}

inline std::string ExtractComponentName(const std::string& request) {
	if(ContainsAny(request, {"ライトコンポーネント", "ライトComponent"})) {
		return "LightComponent";
	}

	std::size_t search = 0;
	while(true) {
		const std::size_t suffix = request.find("Component", search);
		if(suffix == std::string::npos) break;

		std::size_t begin = suffix;
		while(begin > 0) {
			const unsigned char ch =
				static_cast<unsigned char>(request[begin - 1]);
			if(std::isalnum(ch) == 0 && ch != '_') break;
			--begin;
		}
		if(begin < suffix) return request.substr(begin, suffix + 9 - begin);
		search = suffix + 9;
	}
	return {};
}

inline std::string EntityFromComponent(const std::string& component) {
	constexpr const char* suffix = "Component";
	constexpr std::size_t suffixLength = 9;
	if(component.size() > suffixLength &&
		component.compare(
			component.size() - suffixLength,
			suffixLength,
			suffix) == 0) {
		return component.substr(0, component.size() - suffixLength);
	}
	return {};
}

inline Route Resolve(const std::string& request) {
	// Scene全体の状況確認は既存のScene Snapshot経路へ渡す。
	// ここでListEntitiesだけに短絡するとComponent/System観測が欠落する。
	if(ContainsAny(request, {
		"今のシーンの状況", "現在のシーンの状況", "シーンの現状",
		"current scene status", "scene status"})) {
		return {};
	}

	const std::string component = ExtractComponentName(request);
	std::string entity = ExtractQuotedEntity(request);
	if(entity.empty() && !component.empty()) entity = EntityFromComponent(component);
	if(entity.empty()) entity = ExtractEntityBeforeParticle(request);

	if(!component.empty() && !entity.empty()) {
		return {
			"ReadComponent",
			Json::object({
				{"entityName", entity},
				{"component", component},
			}),
		};
	}

	const bool componentQuestion = ContainsAny(request, {
		"コンポーネント", "Component", "component", "にも"});
	if(!entity.empty() && componentQuestion) {
		return {
			"DescribeEntity",
			Json::object({{"entityName", entity}}),
		};
	}
	return {};
}

inline bool PayloadSatisfied(const Json& payload) {
	if(!payload.is_object()) return true;
	if(payload.contains("error")) return false;
	if(payload.contains("found") && payload.at("found").is_boolean() &&
		!payload.at("found").get<bool>()) {
		return false;
	}
	if(payload.contains("exists") && payload.at("exists").is_boolean() &&
		!payload.at("exists").get<bool>()) {
		return false;
	}
	if(payload.contains("satisfied") && payload.at("satisfied").is_boolean() &&
		!payload.at("satisfied").get<bool>()) {
		return false;
	}
	return true;
}

inline std::string BuildReply(const Route& route, const Json& payload) {
	if(!route.IsValid()) return {};

	if(route.tool == "ListEntities") {
		const Json entities = payload.value("entities", Json::array());
		if(!entities.is_array()) {
			return "現在のSceneのEntity一覧を解釈できませんでした。";
		}

		std::string out = "現在のSceneで生存しているEntityは" +
			std::to_string(entities.size()) + "件です。";
		for(std::size_t i = 0; i < entities.size(); ++i) {
			const Json& entry = entities[i];
			const std::string name = entry.is_object()
				? entry.value("name", std::string("(unnamed)"))
				: std::string("(unknown)");
			out += "\n" + std::to_string(i + 1) + ". " + name;
		}
		out += "\n\nこの結果はEntity一覧の観測であり、"
			"各EntityのComponentが空であることは意味しません。";
		return out;
	}

	const std::string entity =
		route.arguments.value("entityName", std::string());
	if(payload.is_object() && payload.contains("error")) {
		return "Entity '" + entity + "' の読み取りに失敗しました: " +
			payload.value("error", std::string("unknown error"));
	}

	if(route.tool == "DescribeEntity") {
		const Json components = payload.value("components", Json::array());
		if(!components.is_array()) {
			return "Entity '" + entity +
				"' のComponent一覧を解釈できませんでした。";
		}

		std::string out = "Entity '" + entity + "' には" +
			std::to_string(components.size()) + "件のComponentがあります。";
		for(std::size_t i = 0; i < components.size(); ++i) {
			const Json& entry = components[i];
			const std::string name = entry.is_object()
				? entry.value("component", std::string("(unknown)"))
				: std::string("(unknown)");
			out += "\n" + std::to_string(i + 1) + ". " + name;
		}
		return out;
	}

	if(route.tool == "ReadComponent") {
		const std::string component =
			route.arguments.value("component", std::string());
		if(!payload.is_object() || !payload.contains("value")) {
			return "Entity '" + entity + "' のComponent '" + component +
				"' の値を取得できませんでした。";
		}
		return "Entity '" + entity + "' の " + component +
			" の現在値です。\n```json\n" +
			payload.at("value").dump(2) + "\n```";
	}
	return {};
}

} // namespace agentos::componentquery
