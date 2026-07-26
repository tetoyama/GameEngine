// =======================================================================
//
// EvidencePromptCompressor.h
//
// Reason/Critic/Synthesisへ渡すEvidenceを、根拠IDと全体状態を維持したまま
// bounded contextへ圧縮する。最新Evidenceを先頭・詳細優先にすることで、
// Repairで後から得た観測が古い巨大payloadに押し出されることを防ぐ。
//
// =======================================================================
#pragma once

#include <algorithm>
#include <cstddef>
#include <string>

#include "../Json.h"
#include "../TextUtf8.h"

namespace agentos::evidence_prompt {

// dump時の共通設定。
//
// error_handler_t::replace を指定して、不正なUTF-8があっても例外を投げず
// 置換文字へ倒す。Compressは「プロンプトに収まるサイズか」を測るために
// dumpを何度も呼ぶため、ここが投げるとセッションごとプロセスが落ちる。
// 切り詰め自体は文字境界で行っている（TruncateText）ので通常は不要だが、
// Evidenceの中身は外部由来（ツール出力・LLM出力）で何が来るか保証できない。
inline std::string SafeDump(const Json& value) {
	return value.dump(-1, ' ', false, Json::error_handler_t::replace);
}

inline std::size_t SafeDumpSize(const Json& value) {
	return SafeDump(value).size();
}

namespace detail {

// UTF-8の文字境界で切る。
// バイト位置で切っていた頃、CodeSearchが返すコード抜粋（日本語コメント入り）の
// 途中で切断され、不正なUTF-8になってjson::dump()が例外を投げていた。
// ワーカースレッドがそれを捕まえずterminateしていた。
inline std::string TruncateText(const std::string& text, std::size_t limit) {
	if(text.size() <= limit) return text;
	if(limit <= 20) return TruncateUtf8(text, limit);
	return TruncateUtf8(text, limit - 18, "...(compressed)...");
}

inline Json CompactValue(const Json& value, int depth, std::size_t stringLimit) {
	if(value.is_string()) {
		return TruncateText(value.get<std::string>(), stringLimit);
	}
	if(value.is_primitive()) return value;
	if(depth >= 3) {
		if(value.is_array()) {
			return Json::object({{"_summary", "array"}, {"size", value.size()}});
		}
		return Json::object({{"_summary", "object"}, {"size", value.size()}});
	}
	if(value.is_array()) {
		Json out = Json::array();
		constexpr std::size_t kMaxItems = 5;
		const std::size_t count = (std::min)(value.size(), kMaxItems);
		for(std::size_t i = 0; i < count; ++i) {
			out.push_back(CompactValue(value.at(i), depth + 1, stringLimit));
		}
		if(value.size() > count) {
			out.push_back(Json::object({{"_omitted", value.size() - count}}));
		}
		return out;
	}

	Json out = Json::object();
	constexpr std::size_t kMaxFields = 18;
	std::size_t fields = 0;
	for(const auto& item : value.items()) {
		if(fields >= kMaxFields) break;
		out[item.key()] = CompactValue(item.value(), depth + 1, stringLimit);
		++fields;
	}
	if(value.size() > fields) out["_omitted_fields"] = value.size() - fields;
	return out;
}

inline Json CompactProvenance(const Json& evidence) {
	Json out = Json::object();
	if(!evidence.is_object() || !evidence.contains("provenance") ||
	   !evidence.at("provenance").is_object()) return out;
	const Json& source = evidence.at("provenance");
	for(const char* key : {"sourceType", "sourceUri", "session", "frame"}) {
		if(source.contains(key)) out[key] = source.at(key);
	}
	return out;
}

inline Json PayloadSignals(const Json& evidence) {
	Json signals = Json::object();
	if(!evidence.is_object() || !evidence.contains("payload") ||
	   !evidence.at("payload").is_object()) return signals;
	const Json& payload = evidence.at("payload");
	constexpr std::size_t kMaxSignals = 14;
	std::size_t count = 0;
	for(const auto& item : payload.items()) {
		if(count >= kMaxSignals) break;
		const Json& value = item.value();
		if(value.is_boolean() || value.is_number() || value.is_null()) {
			signals[item.key()] = value;
			++count;
		} else if(value.is_string()) {
			signals[item.key()] = TruncateText(value.get<std::string>(), 220);
			++count;
		} else if(value.is_array()) {
			signals[item.key() + "_count"] = value.size();
		}
	}
	return signals;
}

inline Json IndexEntry(const Json& evidence) {
	Json out = Json::object();
	if(!evidence.is_object()) return out;
	for(const char* key : {"id", "taskId", "confidence"}) {
		if(evidence.contains(key)) out[key] = evidence.at(key);
	}
	out["claim"] = TruncateText(evidence.value("claim", std::string()), 280);
	out["provenance"] = CompactProvenance(evidence);
	const Json signals = PayloadSignals(evidence);
	if(!signals.empty()) out["payloadSignals"] = signals;
	return out;
}

// 詳細版。payloadを丸ごと載せるので、payloadの要約であるpayloadSignalsは持たない。
// 以前は IndexEntry をそのまま土台にしていたため、同じ文字列が本文と要約の
// 両方に入っていた（実機では claim が1件のEvidenceにつき5回出現した）。
inline Json DetailedEntry(const Json& evidence) {
	Json out = IndexEntry(evidence);
	out.erase("payloadSignals");
	if(evidence.is_object() && evidence.contains("payload")) {
		Json payload = CompactValue(evidence.at("payload"), 0, 900);
		// Toolはpayloadの中にもclaimを入れる（CommandResult::Okの慣習）。
		// EvidenceがそれをEvidence.claimへ持ち上げているので、同じ文が2度出る。
		// 見出しはEntry側に既にあるため、payload側の複製は落とす。
		if(payload.is_object() && payload.contains("claim") &&
		   out.contains("claim") && payload.at("claim") == out.at("claim")) {
			payload.erase("claim");
		}
		out["payload"] = std::move(payload);
	}
	return out;
}

inline void CopyIfPresent(const Json& source, Json* destination, const char* key) {
	if(destination != nullptr && source.is_object() && source.contains(key)) {
		(*destination)[key] = source.at(key);
	}
}

} // namespace detail

// 出力は必ずmaxChars以下を目標にし、次の順で情報を保持する。
// 1. Coverage/失敗数/Revision等の決定的な全体状態
// 2. 最新順Evidence index（ID・claim・provenance・scalar signal）
// 3. 残り予算へ最新Evidenceの圧縮payload詳細
// 古い詳細は省略してもEvidence IDとclaimは可能な限り残す。
inline Json Compress(const Json& builtEvidence, std::size_t maxChars = 8500) {
	maxChars = (std::max)(maxChars, std::size_t(2048));
	Json out = Json::object();
	for(const char* key : {
		"coverage", "tasksWithoutEvidence", "usableEvidenceCount", "failedEvidenceCount",
		"supersededEvidenceCount", "activeRevision", "retiredTasks", "contradictions"}) {
		detail::CopyIfPresent(builtEvidence, &out, key);
	}

	const Json empty = Json::array();
	const Json& evidences = builtEvidence.is_object() && builtEvidence.contains("evidences") &&
		builtEvidence.at("evidences").is_array()
		? builtEvidence.at("evidences")
		: empty;

	out["compression"] = Json::object({
		{"policy", "latest_first_index_plus_recent_details"},
		{"originalEvidenceCount", evidences.size()},
		{"maxChars", maxChars},
		{"evidenceOrder", "newest_first"},
	});
	out["evidences"] = Json::array();
	out["recentEvidenceDetails"] = Json::array();

	// 詳細と索引は重ねない。
	//
	// 以前は両方とも「最新から」詰めていたため、詳細に入るEvidenceは
	// 必ず索引にも入っていた（詳細集合は索引集合の先頭部分になる）。
	// つまり重複は偶発ではなく構造的で、Evidenceが少ないほど比率が上がる。
	// 実機（Evidence 1件）では出力2421文字のうち claim が5回・595文字を占め、
	// 肝心のコード本文503文字より多かった。
	//
	// どちらを先に詰めるかは選ばざるを得ない。「索引＋最新の詳細」という
	// 方針の主語は詳細側なので、詳細を先に確保し、索引には
	// 詳細に入らなかった（＝より古い）Evidenceだけを載せる。
	const std::size_t detailBudget = maxChars * 3 / 5;
	std::size_t detailedCount = 0;
	for(std::size_t offset = 0; offset < evidences.size(); ++offset) {
		const std::size_t index = evidences.size() - 1 - offset;
		Json candidate = out;
		candidate["recentEvidenceDetails"].push_back(detail::DetailedEntry(evidences.at(index)));
		if(SafeDumpSize(candidate) > detailBudget && !out["recentEvidenceDetails"].empty()) break;
		out = std::move(candidate);
		++detailedCount;
		if(SafeDumpSize(out) > detailBudget) break;
	}

	// 残り予算へ、詳細に入らなかった古いEvidenceの索引を詰める。
	for(std::size_t offset = detailedCount; offset < evidences.size(); ++offset) {
		const std::size_t index = evidences.size() - 1 - offset;
		Json candidate = out;
		candidate["evidences"].push_back(detail::IndexEntry(evidences.at(index)));
		if(SafeDumpSize(candidate) > maxChars) break;
		out = std::move(candidate);
	}

	const std::size_t indexed = out.at("evidences").size();
	const std::size_t detailed = out.at("recentEvidenceDetails").size();
	// indexedとdetailedは互いに素なので、どちらにも載らなかった件数が本当の欠落。
	const std::size_t represented = indexed + detailed;
	out["compression"]["indexedEvidenceCount"] = indexed;
	out["compression"]["detailedEvidenceCount"] = detailed;
	out["compression"]["omittedEvidenceCount"] =
		evidences.size() > represented ? evidences.size() - represented : 0;
	out["compression"]["omittedDetailCount"] =
		evidences.size() > detailed ? evidences.size() - detailed : 0;

	// metadata追記で数十文字超えた場合は詳細を後ろから削る。
	while(SafeDumpSize(out) > maxChars && !out["recentEvidenceDetails"].empty()) {
		out["recentEvidenceDetails"].erase(out["recentEvidenceDetails"].end() - 1);
		out["compression"]["detailedEvidenceCount"] = out["recentEvidenceDetails"].size();
		out["compression"]["omittedDetailCount"] =
			evidences.size() - out["recentEvidenceDetails"].size();
	}
	return out;
}

inline std::string CompressToString(const Json& builtEvidence, std::size_t maxChars = 8500) {
	// compact JSONで返し、pretty-printによる再膨張でPrompt上限を越えないようにする。
	return SafeDump(Compress(builtEvidence, maxChars));
}

} // namespace agentos::evidence_prompt
