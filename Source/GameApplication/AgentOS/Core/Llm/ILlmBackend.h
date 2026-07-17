// =======================================================================
//
// ILlmBackend.h
//
// LLM呼び出しの抽象。CoreはこのインターフェースのみでLLMへアクセスする。
// 実機: Service/LlamaLlmBackend（LLAMAAgentをポーリング）
// テスト: Core/Llm/MockLlmBackend（台本方式）
//
// =======================================================================
#pragma once

#include <cstdint>
#include <string>

namespace agentos {

struct LlmGenerationStats {
	std::int64_t promptChars = 0;      // トークン数が取れない場合は文字数で代替
	std::int64_t completionChars = 0;
	std::int64_t elapsedMillis = 0;
};

class ILlmBackend {
public:
	virtual ~ILlmBackend() = default;

	// 同期生成。呼び出しスレッドをブロックする。
	// Orchestrator側が専用スレッドから呼ぶ前提。
	virtual std::string Generate(
		const std::string& systemPrompt,
		const std::string& userPrompt,
		LlmGenerationStats* statsOut = nullptr) = 0;

	// 実行中の生成を中断する（対応していないバックエンドは無視してよい）。
	virtual void Cancel() {}
};

} // namespace agentos
