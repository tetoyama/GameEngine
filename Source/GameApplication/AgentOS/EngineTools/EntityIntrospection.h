// =======================================================================
//
// EntityIntrospection.h
//
// Entity / Component の読み取り専用Introspection Tool群（構想§11）。
// すべてMain Thread（MainThreadDispatcher経由）から呼び出される前提。
//
// =======================================================================
#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "../Core/Json.h"
#include "Scene/Entity/Entity.h"

struct SceneContext;

namespace YAML {
	class Node;
}

namespace agentos {

// YAML::Node をagentos::Json (nlohmann::json) へ変換する。
// Scalarはint64 -> double -> bool -> stringの順で解釈を試みる
// （yaml-cppのas<T>()は変換不能な場合に例外を投げるため、その失敗を利用して型を絞り込む）。
Json YamlToJson(const YAML::Node& node);

// アクティブSceneの生存Entity一覧を返す（indexの昇順、最大maxCount件）。
// [{id, generation, name(NameComponentがあれば)}]
Json ListEntities(SceneContext& context, std::size_t maxCount);

// NameComponent.name が一致する最初のEntityを返す。
std::optional<Entity> FindEntityByName(SceneContext& context, const std::string& name);

// Entityが持つ全Componentを {component, value} の配列として返す。
// value は各ComponentのYAMLエンコード結果をJSON化したもの。
Json DescribeEntity(SceneContext& context, Entity entity);

// 単一Componentの値を読み取る。componentName未登録 / Entity未所持の場合は
// payload内に "error" フィールドを持つJSONを返す（Command自体は成功=Read Toolとして扱う）。
Json ReadComponent(SceneContext& context, Entity entity, const std::string& componentName);

} // namespace agentos
