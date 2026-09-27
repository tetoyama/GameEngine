// =======================================================================
//
// Evidence.cpp
//
// =======================================================================
#include "Evidence.h"

namespace agentos {

Json Evidence::ToJson() const {
	Json j = Json::object();
	j["id"] = id;
	j["taskId"] = taskId;
	j["claim"] = claim;
	j["payload"] = payload;
	j["provenance"] = Json::object({
		{"sourceType", provenance.sourceType},
		{"sourceUri", provenance.sourceUri},
		{"session", provenance.session},
		{"frame", provenance.frame},
	});
	j["confidence"] = confidence;
	return j;
}

// FromJsonはLLM経由・ファイル経由どちらの入力に対しても頑健であること。
// フィールド欠損はデフォルト値へフォールバックし、型不一致等の例外は
// キャッチしてデフォルト構築のEvidenceを返す（呼び出し側で再検証する前提）。
Evidence Evidence::FromJson(const Json& j) {
	Evidence e;
	try {
		if (j.contains("id") && !j.at("id").is_null()) {
			e.id = j.at("id").get<EvidenceId>();
		}
		if (j.contains("taskId") && !j.at("taskId").is_null()) {
			e.taskId = j.at("taskId").get<TaskId>();
		}
		if (j.contains("claim") && j.at("claim").is_string()) {
			e.claim = j.at("claim").get<std::string>();
		}
		if (j.contains("payload") && j.at("payload").is_object()) {
			e.payload = j.at("payload");
		}
		if (j.contains("provenance") && j.at("provenance").is_object()) {
			const Json& p = j.at("provenance");
			if (p.contains("sourceType") && p.at("sourceType").is_string()) {
				e.provenance.sourceType = p.at("sourceType").get<std::string>();
			}
			if (p.contains("sourceUri") && p.at("sourceUri").is_string()) {
				e.provenance.sourceUri = p.at("sourceUri").get<std::string>();
			}
			if (p.contains("session") && p.at("session").is_string()) {
				e.provenance.session = p.at("session").get<std::string>();
			}
			if (p.contains("frame") && !p.at("frame").is_null()) {
				e.provenance.frame = p.at("frame").get<std::int64_t>();
			}
		}
		if (j.contains("confidence") && !j.at("confidence").is_null()) {
			e.confidence = j.at("confidence").get<double>();
		}
	} catch (const Json::exception&) {
		// 型不一致等はデフォルト構築のEvidenceへフォールバックする。
		return Evidence{};
	}
	return e;
}

} // namespace agentos
