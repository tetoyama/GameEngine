// =======================================================================
// 
// AgentConfig.h
// 
// =======================================================================
#pragma once

#include <string>
#include <cstdint>

// ============================
// AgentConfig
// ============================
//
// LLAMAAgent / LLAMAService 共通設定
// - llama_context_params
// - sampler
// - Agent 実行制御
//
struct AgentConfig final {

	// ============================
	// llama_context_params 対応
	// ============================

	// llama_context_params::n_ctx
	uint32_t n_ctx = 4096;

	// llama_context_params::n_threads
	uint32_t n_threads = 8;

	// ============================
	// sampler 設定
	// ============================

	// 最大生成トークン数
	uint32_t max_tokens = 2048;

	// 温度
	float temperature = 0.7f;

	// Top-K
	uint32_t top_k = 40;

	// Top-P
	float top_p = 0.9f;

	// 繰り返しペナルティ
	float repeat_penalty = 1.1f;

	// ============================
	// Agent 実行制御
	// ============================

	// 同時実行可能 Job 数
	uint32_t max_concurrent_jobs = 1;

	// ストリーミング出力を許可
	bool enable_streaming = true;

	// 実行中の中断を許可
	bool allow_abort = true;

	// ============================
	// Prompt
	// ============================

	// system prompt（空なら未使用）
	std::string system_prompt;

	// ============================
	// Debug
	// ============================

	// トークン単位のログ出力
	bool enable_token_log = false;

	// プロファイリング有効化
	bool enable_profiling = false;

	// ============================
	// Validation
	// ============================

	bool IsValid() const noexcept{
		if(n_ctx == 0) return false;
		if(n_threads == 0) return false;
		if(max_tokens == 0) return false;
		if(top_p <= 0.0f || top_p > 1.0f) return false;
		if(max_concurrent_jobs == 0) return false;
		return true;
	}
};
