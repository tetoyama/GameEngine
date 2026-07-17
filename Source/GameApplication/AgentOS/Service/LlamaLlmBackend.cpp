// =======================================================================
//
// LlamaLlmBackend.cpp
//
// =======================================================================
#include "LlamaLlmBackend.h"

#include <chrono>
#include <thread>

#include "Service/LlamaService/LLAMAAgent.h"

namespace agentos {

LlamaLlmBackend::LlamaLlmBackend(std::shared_ptr<LLAMAAgent> agent, std::int64_t timeoutMillis)
	: m_agent(std::move(agent))
	, m_timeoutMillis(timeoutMillis) {}

std::string LlamaLlmBackend::Generate(
	const std::string& systemPrompt,
	const std::string& userPrompt,
	LlmGenerationStats* statsOut
) {
	if(statsOut) *statsOut = LlmGenerationStats{};
	if(!m_agent) return std::string();

	// LLAMAAgent::RunAsyncは単一のプロンプト文字列しか取らないため、
	// systemPromptとuserPromptをここで結合する（BRAIN.cppのようにAgentConfig側の
	// system_promptへ一度だけ焼き込む方式ではなく、AgentOS側は呼び出しごとに
	// 自己完結したプロンプトを渡す設計のため）。
	const std::string prompt = systemPrompt.empty()
		? userPrompt
		: (systemPrompt + "\n\n" + userPrompt);

	const auto start = std::chrono::steady_clock::now();

	// 呼び出しごとにContextをResetしてから実行する（意図的な設計。AgentOsTypes.h参照）。
	m_agent->ResetContext();
	m_agent->RunAsync(prompt);

	// Running状態への遷移待ち（BRAIN.cpp WorkerLoopと同じ1msポーリング）。
	while(m_agent->GetState() != LLAMAAgent::State::Running){
		if(m_agent->GetState() == LLAMAAgent::State::Dead){
			break;
		}
		const auto elapsedMillis = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - start
		).count();
		if(elapsedMillis > m_timeoutMillis){
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	// 出力監視ループ（BRAIN.cpp WorkerLoopと同じ50msポーリング）。
	while(m_agent->GetState() == LLAMAAgent::State::Running){
		const auto elapsedMillis = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - start
		).count();
		if(elapsedMillis > m_timeoutMillis){
			m_agent->Stop();
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}

	const std::string output = m_agent->GetOutput();

	if(statsOut){
		const auto elapsedMillis = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - start
		).count();
		statsOut->promptChars = static_cast<std::int64_t>(prompt.size());
		statsOut->completionChars = static_cast<std::int64_t>(output.size());
		statsOut->elapsedMillis = elapsedMillis;
	}

	return output;
}

void LlamaLlmBackend::Cancel() {
	if(m_agent) m_agent->Stop();
}

} // namespace agentos
