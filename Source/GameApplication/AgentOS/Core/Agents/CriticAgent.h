// =======================================================================
//
// CriticAgent.h
//
// 仮説とEvidenceを検証するCritic担当（構想§9）。
// pass/failは必ずプログラム側で決定的に算出する（LLMの所見はllmScoresへ
// advisoryとして保存するのみで、判定には使わない）。
//
// =======================================================================
#pragma once

#include <string>
#include <vector>

#include "AgentContext.h"

namespace agentos {

// ---------------------------------
// critic_internal
// CriticAgent.cppに実装される内部ヘルパのうち、テストから直接検証したい
// ものだけをここで公開する（CriticAgent::Run経由の間接検証では抽出ロジック
// 単体のquoted/ASCII/カタカナ/stoplist分岐を網羅しづらいため）。
// CriticAgent以外から本番コードとして呼び出すことは想定しない。
// ---------------------------------
namespace critic_internal {

// resolvedRequestから「特定の対象を指す語」を抽出する（ゲート#8で使用）。
// - 単一/二重引用符トークン（例: Entity 'Player' → "Player"）
// - ASCII識別子（3文字以上。Entity/Component/Scene/System/Tool/the/and等の
//   一般語はstoplistで除外する。大小無視で重複排除する）
// - カタカナ連続（3文字以上。UTF-8バイト範囲 E382A0-BF/E383 80-BFで判定。
//   シーン/コンポーネント/エンティティ/システム/ツール等の一般語はstoplistで除外する）
std::vector<std::string> ExtractGoalIdentifiers(const std::string& text);

} // namespace critic_internal

struct CriticVerdict {
	double programmaticScore = 0.0;
	Json llmScores = Json::object();
	std::vector<std::string> failures;
	Json additionalTasks = Json::array();

	// 「そもそも立てるべきでなかった」とCriticが判定したTaskのID一覧。
	// 追加(additionalTasks)と対になる撤回の指示であり、
	// 再計画が単調増加しかできない問題を解消するために設けている。
	// 実際に撤回されるかはEvidenceBuilder側の決定的ガードが決める
	// （使えるEvidenceを産んだTaskは撤回されない）。
	Json obsoleteTasks = Json::array();

	bool pass = false;
};

class CriticAgent {
public:
	// rankedHypotheses: LogicGraph::ToJson()相当（{"hypotheses":[...]}）。
	// builtEvidence: EvidenceBuilder::ToJson()相当。
	// LLM呼び出しが失敗しても（advisoryに過ぎないため）Failにはせず、
	// プログラム採点のみで劣化した検証結果を返す。
	static Result Run(AgentContext& ctx, const Json& rankedHypotheses, const Json& builtEvidence, CriticVerdict* out);
};

} // namespace agentos
