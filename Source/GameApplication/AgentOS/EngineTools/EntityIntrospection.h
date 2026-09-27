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
#include <vector>

#include "../Core/Json.h"
#include "../Core/Logic/FuzzyMatch.h"
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

// ResolveEntityCandidatesの1候補。
// matchedByは "exact"/"case_insensitive"/"substring"/"reverse_substring"（名前一致の場合）、
// もしくは "component:<ComponentName>"（Component型一致経由の場合）。
struct EntityCandidate {
	Entity entity;
	std::string name;
	std::string matchedBy;
	double score = 0.0;
};

// ユーザー入力queryに対し、あいまい一致（Fuzzy Match）でEntity候補を列挙する。
// 「Player」はEntity名の厳密一致とは限らず、名前は違ってもPlayerComponentが
// アタッチされたEntityかもしれない、という要求に応えるための解決経路（構想§11拡張）。
//
// pass1（名前一致）: NameComponent.nameをqueryとFuzzy Match（FuzzyMatch::ScoreName）。
//   matchedByはFuzzyMatchのmatchTypeをそのまま使う。
// pass2（Component型一致）: 登録済みComponent名をqueryとFuzzy Match
//   （FuzzyMatch::ScoreComponentName、例:「Light」→「LightComponent」）し、
//   一致したComponent型を実際に持つ全EntityをmatchedBy="component:<ComponentName>"、
//   score = componentScore * 0.9 として候補化する。
//
// 同一Entityが両passで候補化された場合はスコアが高い方を採用してマージする。
// 戻り値はスコア降順（同点はEntity index昇順）でmaxResults件まで。
std::vector<EntityCandidate> ResolveEntityCandidates(
	SceneContext& context,
	const std::string& query,
	std::size_t maxResults = 5
);

// 登録済みComponent名をqueryとのFuzzy Match（ScoreComponentName）でランキングする。
// 「Light」→「LightComponent」のようなComponent型の部分一致解決に使う。
std::vector<fuzzy::Match> ResolveComponentCandidates(
	SceneContext& context,
	const std::string& query,
	std::size_t maxResults = 5
);

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
