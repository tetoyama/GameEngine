// =======================================================================
//
// JsonExtractor.h
//
// LLMの生テキストから単一のJSONオブジェクトを頑健に抽出する（構想§9）。
// ローカル9BモデルはJSON契約を守れないことがあるため、以下を許容する:
//   - ```json ... ``` フェンス（最初のフェンスを採用）
//   - フェンス無しの素のJSON
//   - 文章中に埋め込まれたJSON（最初の'{'から探索し、失敗したら次の'{'へ）
//   - 末尾カンマ（trailing comma）の除去による簡易修復
//   - <think>...</think> ブロック（Qwen系モデルが出力する）は無視する
//
// =======================================================================
#pragma once

#include <string>

#include "../AgentOsTypes.h"
#include "../Json.h"

namespace agentos {

class JsonExtractor {
public:
	// llmTextからJSONオブジェクトを1つ抽出しoutへ格納する。
	// 抽出できなかった場合はResult::Fail(reason)を返す。
	static Result Extract(const std::string& llmText, Json* out);
};

} // namespace agentos
