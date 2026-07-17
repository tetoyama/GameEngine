// =======================================================================
//
// LlamaLlmBackend.h
//
// agentos::ILlmBackend の実機実装。LLAMAAgentを1体ラップしてポーリング駆動する。
// 実装パターンはEngine/Editor/UI/BRAIN/BRAIN.cppのWorkerLoopを踏襲する。
//
// AgentOSの各Generate()呼び出しは自己完結したプロンプト（systemPrompt+userPrompt）を
// 想定しているため、呼び出しごとにResetContext()してから実行する（意図的な設計）。
//
// =======================================================================
#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

#include "../Core/Llm/ILlmBackend.h"

class LLAMAAgent;

namespace agentos {

class LlamaLlmBackend : public ILlmBackend {
public:
	// timeoutMillis: 1回のGenerate呼び出し全体（Running遷移待ち含む）のタイムアウト。
	// 超過した場合はStop()して、その時点のGetOutput()（部分出力）を返す。
	explicit LlamaLlmBackend(
		std::shared_ptr<LLAMAAgent> agent,
		std::int64_t timeoutMillis = 300000
	);

	std::string Generate(
		const std::string& systemPrompt,
		const std::string& userPrompt,
		LlmGenerationStats* statsOut = nullptr
	) override;

	// Cancelは以降のGenerateも即時拒否するラッチとして扱う。
	// 新しいSessionを開始するときだけResetCancellation()で解除する。
	void Cancel() override;
	void ResetCancellation() noexcept;
	bool IsCancellationRequested() const noexcept;

private:
	std::shared_ptr<LLAMAAgent> m_agent;
	std::int64_t m_timeoutMillis;
	std::atomic<bool> m_cancelRequested{false};
};

} // namespace agentos
