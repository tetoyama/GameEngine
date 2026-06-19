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
	// プロンプト処理（バッチ）用スレッド数にも同じ値を流用する。
	// 詳細は LLAMAAgent::CreateContext を参照。
	uint32_t n_threads = 8;

	// ============================
	// sampler 設定
	// ============================

	// 最大生成トークン数
	uint32_t max_tokens = 2048;

	// 温度
	// だったため、コメントを実態に合わせて修正した。0.4f にしたい場合は値を直接変更すること。
	// ゲーム制作のアシストやコード生成など、論理的で確実性の高い応答を求める場合は
	// 0.3f〜0.5f 程度の低めの温度が安定しやすい。
	float temperature = 0.7f;

	// Top-K
	uint32_t top_k = 40;

	// Top-P
	float top_p = 0.95f;

	// 繰り返しペナルティ
	// チャットモデルではこれ自体を 1.0f（無効）にするのが最も自然な応答になりやすい。
	// 会話が長くなって語彙のループが気になる場合のみ、隠し味程度に 1.01f〜1.02f を試すこと。
	float repeat_penalty = 1.01f;

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
		// max_tokens が n_ctx を超えていても物理的にエラーではない
		// （n_ctx 側で頭打ちになるだけ）が、設定ミスの典型例なので軽くガードしておく。
		if(max_tokens > n_ctx) return false;
		if(top_p <= 0.0f || top_p > 1.0f) return false;
		// 繰り返しペナルティが負の値にならないかチェック（安全ガード）
		if(repeat_penalty < 0.0f) return false;
		if(max_concurrent_jobs == 0) return false;
		return true;
	}
};