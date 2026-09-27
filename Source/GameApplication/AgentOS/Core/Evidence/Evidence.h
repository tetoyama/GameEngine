// =======================================================================
//
// Evidence.h
//
// Evidence = 観測・検索・実行によって直接得られた事実（構想§3.2）。
// 必ずProvenance（出典）を持ち、Logicが破綻しても破棄されない。
//
// =======================================================================
#pragma once

#include <string>

#include "../AgentOsTypes.h"
#include "../Json.h"

namespace agentos {

// ---------------------------------
// 出典情報
// ---------------------------------
struct Provenance {
	std::string sourceType;   // "RuntimeTrace" / "CodeSearch" / "TestResult" / "Log" など
	std::string sourceUri;    // ファイルパス、Tool名、セッション名など
	std::string session;      // 実行セッション識別子（例: "run_51"）
	std::int64_t frame = -1;  // フレーム番号（該当しない場合は-1）
};

// ---------------------------------
// Evidence本体
// claimは自然言語1文、payloadは構造化データ。
// confidenceは出典の確からしさ（直接観測=1.0、推定を含む場合は下げる）。
// LLMの自己申告ではなく、Evidence生成側が決定的に設定する。
// ---------------------------------
struct Evidence {
	EvidenceId id = kInvalidId;
	TaskId taskId = kInvalidId;
	std::string claim;
	Json payload = Json::object();
	Provenance provenance;
	double confidence = 1.0;

	Json ToJson() const;
	static Evidence FromJson(const Json& j);
};

// ---------------------------------
// 失敗Evidenceの判定
// ---------------------------------
// 「Toolが失敗した」「対象が見つからなかった」等、世界について何も
// 確定していない記録を指す。EvidenceBuilderのcoverage計算と、
// 会話履歴の引き継ぎ（TaskStore::SearchConversationHistory）の
// 両方が同じ基準で判定する必要があるため、ここに置く。
//
// 以前はEvidenceBuilder.cppの匿名namespaceにあり、履歴検索側は
// 判定を持たなかった。その結果、前セッションで失敗した記録が
// 次のセッションへ「観測」として戻り、同じ探索を繰り返させていた。
inline bool IsFailureEvidence(const Evidence& evidence) {
	const std::string& sourceType = evidence.provenance.sourceType;
	if (sourceType == "ToolError" || sourceType == "ToolResultError" ||
	    sourceType == "ToolUnsatisfied" || sourceType == "CommandValidationError" ||
	    sourceType == "DependencyUnsatisfied") {
		return true;
	}
	if (!evidence.payload.is_object()) return false;
	if (evidence.payload.value("failure", false) || evidence.payload.value("unsatisfied", false)) {
		return true;
	}
	return evidence.payload.contains("error") && evidence.payload.at("error").is_string() &&
		!evidence.payload.at("error").get<std::string>().empty();
}

} // namespace agentos
