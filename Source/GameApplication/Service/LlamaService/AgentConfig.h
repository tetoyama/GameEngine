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
	// 0.7f から 0.4f に調整。
	// ゲーム制作のアシストやコード生成など、論理的で確実性の高い応答を求める場合に最も安定する温度だよ。
	float temperature = 0.7f;

	// Top-K
	uint32_t top_k = 40;

	// Top-P
	float top_p = 0.95f;

	// 繰り返しペナルティ
	// 1.1f から 1.0f に調整。
	// チャットモデルはこれ自体を 1.0f（無効）に設定するのが最も知性を発揮できるよ。
	// もし会話が長くなって語彙のループがどうしても気になった時だけ、隠し味程度に 1.01f ～ 1.02f にしてみてね。
	float repeat_penalty = 1.0001f;

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
		// 繰り返しペナルティが負の値にならないかチェック（安全ガード）
		if(repeat_penalty < 0.0f) return false;
		if(max_concurrent_jobs == 0) return false;
		return true;
	}
};