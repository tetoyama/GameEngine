// =======================================================================
//
// IntakeAgent.cpp
//
// =======================================================================
#include "IntakeAgent.h"

namespace agentos {

namespace {

// rawの配列フィールドから文字列要素のみを取り出してoutOwnerへ設定する。
// 欠損・型不一致は空配列にフォールバックする（LLM出力は untrusted）。
void NormalizeStringArray(const Json& raw, const char* key, Json* normalized) {
	Json arr = Json::array();
	if (raw.contains(key) && raw.at(key).is_array()) {
		for (const auto& v : raw.at(key)) {
			if (v.is_string()) {
				arr.push_back(v.get<std::string>());
			}
		}
	}
	(*normalized)[key] = std::move(arr);
}

} // namespace

Result IntakeAgent::Run(AgentContext& ctx, const std::string& userRequest, Json* intakeOut) {
	if (intakeOut == nullptr) {
		return Result::Fail("IntakeAgent: intakeOut is null");
	}

	const PromptPair prompt = prompts::Intake(userRequest);

	Json raw;
	Result callResult = CallLlmJson(ctx, prompt, &raw);
	if (!callResult) {
		return callResult;
	}

	if (!raw.is_object() || !raw.contains("goal") || !raw.at("goal").is_string() ||
	    raw.at("goal").get<std::string>().empty()) {
		return Result::Fail("IntakeAgent: response missing non-empty string 'goal'");
	}

	Json normalized = Json::object();
	normalized["goal"] = raw.at("goal").get<std::string>();
	NormalizeStringArray(raw, "symptoms", &normalized);
	NormalizeStringArray(raw, "constraints", &normalized);
	NormalizeStringArray(raw, "requiredCapabilities", &normalized);

	// requestTypeの欠損・不正値は"investigation"へフォールバックする
	// （後方互換: requestTypeを含まない旧来のIntake応答・テストモックも
	// 従来どおり調査パイプラインへ流れる）。
	std::string requestType = "investigation";
	if (raw.contains("requestType") && raw.at("requestType").is_string()) {
		const std::string rawType = raw.at("requestType").get<std::string>();
		if (rawType == "conversation" || rawType == "investigation") {
			requestType = rawType;
		}
	}
	normalized["requestType"] = requestType;

	*intakeOut = std::move(normalized);
	return Result::Ok();
}

} // namespace agentos
