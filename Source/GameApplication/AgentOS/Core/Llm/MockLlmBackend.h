// =======================================================================
//
// MockLlmBackend.h
//
// テスト用のILlmBackend実装（台本方式）。
// AddRuleで登録した部分文字列マッチが優先され、一致しなければFIFOキューを
// 消費する。どちらも無ければ "{}" を返す。
//
// =======================================================================
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "ILlmBackend.h"

namespace agentos {

class MockLlmBackend : public ILlmBackend {
public:
	// FIFOへレスポンスを積む。
	void EnqueueResponse(std::string response);

	// systemPrompt+userPromptにpromptSubstringが含まれていればresponseを返すルール。
	// ルールは消費されず何度でもマッチする。先に登録したルールが優先される。
	void AddRule(std::string promptSubstring, std::string response);

	std::string Generate(
		const std::string& systemPrompt,
		const std::string& userPrompt,
		LlmGenerationStats* statsOut = nullptr) override;

	// (systemPrompt, userPrompt) の呼び出し履歴。
	std::vector<std::pair<std::string, std::string>> GetCalls() const;

private:
	struct Rule {
		std::string promptSubstring;
		std::string response;
	};

	std::vector<Rule> rules_;
	std::vector<std::string> queue_;
	std::vector<std::pair<std::string, std::string>> calls_;
};

} // namespace agentos
