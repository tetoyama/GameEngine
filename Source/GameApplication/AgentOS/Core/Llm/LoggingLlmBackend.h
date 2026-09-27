// =======================================================================
//
// LoggingLlmBackend.h
//
// ILlmBackendのデコレータ。内側のバックエンドへ委譲しつつ、
// 全Generate呼び出しの生入出力をコールバックへ通知する。
// AgentOSServiceがこれでLlamaLlmBackendを包み、Transcript（YAML）へ記録する。
// Core層のためエンジン非依存（ヘッダオンリー）。
//
// =======================================================================
#pragma once

#include <functional>
#include <string>
#include <utility>

#include "ILlmBackend.h"

namespace agentos {

class LoggingLlmBackend final : public ILlmBackend {
public:
	// (systemPrompt, userPrompt, output, stats) を受け取る。
	// 呼び出しはGenerateを実行したスレッド（Orchestrator Worker）上で行われる。
	using LogCallback = std::function<void(
		const std::string&, const std::string&,
		const std::string&, const LlmGenerationStats&)>;

	LoggingLlmBackend(ILlmBackend* inner, LogCallback callback)
		: m_inner(inner)
		, m_callback(std::move(callback)) {
	}

	std::string Generate(
		const std::string& systemPrompt,
		const std::string& userPrompt,
		LlmGenerationStats* statsOut = nullptr) override {

		LlmGenerationStats stats{};
		std::string output;
		if(m_inner){
			output = m_inner->Generate(systemPrompt, userPrompt, &stats);
		}

		if(m_callback){
			m_callback(systemPrompt, userPrompt, output, stats);
		}
		if(statsOut){
			*statsOut = stats;
		}
		return output;
	}

	void Cancel() override {
		if(m_inner){
			m_inner->Cancel();
		}
	}

private:
	ILlmBackend* m_inner = nullptr;
	LogCallback m_callback;
};

} // namespace agentos
