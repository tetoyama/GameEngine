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
	if(!m_agent){
		if(statsOut) statsOut->stopReason = "backend_unavailable";
		return std::string();
	}
	if(m_cancelRequested.load(std::memory_order_acquire)){
		if(statsOut) statsOut->stopReason = "cancelled";
		return std::string();
	}

	const std::string prompt = systemPrompt.empty()
		? userPrompt
		: (systemPrompt + "\n\n" + userPrompt);

	const auto start = std::chrono::steady_clock::now();
	auto elapsedMillis = [&]() -> std::int64_t {
		return std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - start
		).count();
	};

	// 呼び出しごとに独立したContextへ戻す。
	m_agent->ResetContext();
	if(m_cancelRequested.load(std::memory_order_acquire)){
		if(statsOut) statsOut->stopReason = "cancelled";
		return std::string();
	}
	m_agent->RunAsync(prompt);

	bool timedOut = false;
	bool backendDead = false;

	// Running状態への遷移待ち。
	while(m_agent->GetState() != LLAMAAgent::State::Running){
		if(m_cancelRequested.load(std::memory_order_acquire)){
			m_agent->Stop();
			break;
		}
		if(m_agent->GetState() == LLAMAAgent::State::Dead){
			backendDead = true;
			break;
		}
		if(elapsedMillis() > m_timeoutMillis){
			timedOut = true;
			m_agent->Stop();
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	// 出力監視ループ。
	while(m_agent->GetState() == LLAMAAgent::State::Running){
		if(m_streamCallback){
			m_streamCallback(
				m_agent->GetOutput(),
				elapsedMillis(),
				m_agent->GetLastPromptTokenCount(),
				m_agent->GetLastGeneratedTokenCount());
		}
		if(m_cancelRequested.load(std::memory_order_acquire)){
			m_agent->Stop();
			break;
		}
		if(elapsedMillis() > m_timeoutMillis){
			timedOut = true;
			m_agent->Stop();
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}

	const bool cancelled = m_cancelRequested.load(std::memory_order_acquire);
	const std::string output = cancelled ? std::string() : m_agent->GetOutput();
	if(m_streamCallback){
		m_streamCallback(
			output,
			elapsedMillis(),
			m_agent->GetLastPromptTokenCount(),
			m_agent->GetLastGeneratedTokenCount());
	}

	if(statsOut){
		statsOut->promptTokens = m_agent->GetLastPromptTokenCount();
		statsOut->completionTokens = m_agent->GetLastGeneratedTokenCount();
		statsOut->promptChars = static_cast<std::int64_t>(prompt.size());
		statsOut->completionChars = static_cast<std::int64_t>(output.size());
		statsOut->elapsedMillis = elapsedMillis();
		statsOut->stopReason = cancelled
			? "cancelled"
			: (timedOut ? "timeout" : (backendDead ? "backend_dead" : "completed"));
	}

	return output;
}

void LlamaLlmBackend::Cancel() {
	m_cancelRequested.store(true, std::memory_order_release);
	if(m_agent) m_agent->Stop();
}

void LlamaLlmBackend::ResetCancellation() noexcept {
	m_cancelRequested.store(false, std::memory_order_release);
}

bool LlamaLlmBackend::IsCancellationRequested() const noexcept {
	return m_cancelRequested.load(std::memory_order_acquire);
}

void LlamaLlmBackend::SetStreamCallback(StreamCallback callback) {
	m_streamCallback = std::move(callback);
}

} // namespace agentos
