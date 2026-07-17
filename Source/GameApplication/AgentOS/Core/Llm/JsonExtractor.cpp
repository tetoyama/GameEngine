// =======================================================================
//
// JsonExtractor.cpp
//
// =======================================================================
#include "JsonExtractor.h"

#include <regex>

namespace agentos {

namespace {

// <think>...</think> ブロックを除去する（大小文字区別なし、複数出現・複数行対応）。
//
// [実機対応] llama.cpp側のchat templateが<think>開始タグを消費するモデル
// （Qwen3系など）では、出力が「思考テキスト ... </think> 本文」という
// 閉じタグだけの形になる。この場合は最初の</think>までを全て思考として捨てる。
std::string StripThinkBlocks(const std::string& text) {
	static const std::regex kThinkPattern(
		"<think>[\\s\\S]*?</think>",
		std::regex::icase);
	std::string result = std::regex_replace(text, kThinkPattern, "");

	// 最後の孤立閉じタグ以降だけを本文として残す。
	static const std::regex kOrphanClose("</think>", std::regex::icase);
	std::size_t cut = 0;
	auto begin = std::sregex_iterator(result.begin(), result.end(), kOrphanClose);
	for (auto it = begin; it != std::sregex_iterator(); ++it) {
		cut = static_cast<std::size_t>(it->position()) + it->length();
	}
	if (cut > 0) {
		result = result.substr(cut);
	}
	return result;
}

// 末尾カンマ（ ,} や ,] ）を除去する簡易修復。
// 変化が無くなるまで繰り返し適用し、ネストしたケースも吸収する。
std::string StripTrailingCommas(const std::string& text) {
	static const std::regex kTrailingComma("[,]\\s*([}\\]])");
	std::string current = text;
	for (int guard = 0; guard < 8; ++guard) {
		std::string next = std::regex_replace(current, kTrailingComma, "$1");
		if (next == current) {
			break;
		}
		current = next;
	}
	return current;
}

// candidateを（必要ならtrailing comma修復してから）パースする。
bool TryParse(const std::string& candidate, Json* out) {
	try {
		*out = Json::parse(candidate);
		return true;
	} catch (const Json::exception&) {
		// フォールスルーして修復を試みる。
	}
	try {
		*out = Json::parse(StripTrailingCommas(candidate));
		return true;
	} catch (const Json::exception&) {
		return false;
	}
}

// startIndexにある'{'からブレースの対応を文字列・エスケープを考慮して追跡し、
// 対応する'}'の直後の位置（=部分文字列の終端, exclusive）を返す。
// 対応が見つからない場合は std::string::npos を返す。
std::size_t FindMatchingBraceEnd(const std::string& text, std::size_t startIndex) {
	int depth = 0;
	bool inString = false;
	bool escaped = false;
	for (std::size_t i = startIndex; i < text.size(); ++i) {
		const char c = text[i];
		if (inString) {
			if (escaped) {
				escaped = false;
			} else if (c == '\\') {
				escaped = true;
			} else if (c == '"') {
				inString = false;
			}
			continue;
		}
		if (c == '"') {
			inString = true;
			continue;
		}
		if (c == '{') {
			++depth;
		} else if (c == '}') {
			--depth;
			if (depth == 0) {
				return i + 1;
			}
		}
	}
	return std::string::npos;
}

// ```json ... ``` フェンスの中身を取り出す（最初のフェンスを採用）。
bool TryExtractFence(const std::string& text, std::string* body) {
	const std::string marker = "```json";
	const std::size_t start = text.find(marker);
	if (start == std::string::npos) {
		return false;
	}
	const std::size_t contentStart = start + marker.size();
	const std::size_t end = text.find("```", contentStart);
	if (end == std::string::npos) {
		return false;
	}
	*body = text.substr(contentStart, end - contentStart);
	return true;
}

} // namespace

Result JsonExtractor::Extract(const std::string& llmText, Json* out) {
	if (out == nullptr) {
		return Result::Fail("out is null");
	}

	const std::string cleaned = StripThinkBlocks(llmText);

	// 1) ```json フェンス
	std::string fenceBody;
	if (TryExtractFence(cleaned, &fenceBody)) {
		if (TryParse(fenceBody, out)) {
			return Result::Ok();
		}
		// フェンスの中身がさらに文章込みの場合に備え、フェンス内でも
		// 埋め込み探索を試みる。
		std::size_t searchFrom = 0;
		while (true) {
			const std::size_t brace = fenceBody.find('{', searchFrom);
			if (brace == std::string::npos) {
				break;
			}
			const std::size_t end = FindMatchingBraceEnd(fenceBody, brace);
			if (end != std::string::npos) {
				const std::string candidate = fenceBody.substr(brace, end - brace);
				if (TryParse(candidate, out)) {
					return Result::Ok();
				}
			}
			searchFrom = brace + 1;
		}
	}

	// 2) フェンス無しの素のJSON（前後の空白を許容）
	{
		std::size_t begin = cleaned.find_first_not_of(" \t\r\n");
		std::size_t last = cleaned.find_last_not_of(" \t\r\n");
		if (begin != std::string::npos && last != std::string::npos && begin <= last) {
			const std::string trimmed = cleaned.substr(begin, last - begin + 1);
			if (!trimmed.empty() && trimmed.front() == '{' && TryParse(trimmed, out)) {
				return Result::Ok();
			}
		}
	}

	// 3) 文章中に埋め込まれたJSON。最初の'{'から探索し、失敗したら次の'{'へ。
	std::size_t searchFrom = 0;
	while (true) {
		const std::size_t brace = cleaned.find('{', searchFrom);
		if (brace == std::string::npos) {
			break;
		}
		const std::size_t end = FindMatchingBraceEnd(cleaned, brace);
		if (end != std::string::npos) {
			const std::string candidate = cleaned.substr(brace, end - brace);
			if (TryParse(candidate, out)) {
				return Result::Ok();
			}
		}
		searchFrom = brace + 1;
	}

	return Result::Fail("no parsable JSON object found in LLM output");
}

} // namespace agentos
