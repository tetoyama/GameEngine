// =======================================================================
// 
// LLAMAAgent.h
// 
// =======================================================================
#pragma once

#include <string>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>

// ============================
// Forward Declarations
// ============================

struct LLAMAModelData;
struct AgentConfig;

struct llama_context;
struct llama_sampler;
using llama_token = int32_t;

// ============================
// LLAMAAgent
// ============================
// ・1 Agent = 1 llama_context
// ・内部ワーカースレッドで非同期推論
// ・RunAsync は何度でも呼べる
// ・プロンプトが長い場合は内部で同期要約
//
// [修正メモ]
// ・コンテキストのクリア/差分更新は llama_memory_* API（unified memory abstraction）
//   を使う前提に変更した。古い llama.cpp（llama_kv_cache_* がまだ ctx 直叩きの時代）
//   をビルドに使っている場合は LLAMAAgent.cpp 側の該当箇所を見て調整すること。
//
class LLAMAAgent {
public:
	// ============================
	// State
	// ============================

	enum class State {
		Idle,
		Running,
		Stopping,
		Dead
	};

	// 履歴管理用の内部構造体
	struct MessageEntry {
		std::string role; // "user" or "assistant"
		std::string text;
	};

	// ============================
	// ctor / dtor
	// ============================

	LLAMAAgent(
		std::shared_ptr<LLAMAModelData> model,
		std::shared_ptr<const AgentConfig> config
	);

	~LLAMAAgent();

	// ============================
	// Public API
	// ============================

	// 非同期実行（キューに積む）
	void RunAsync(const std::string& prompt);

	// 停止要求
	void Stop();
	// ============================
	// Context Utilities
	// ============================

	// context を初期状態に戻す
	void ResetContext();


	// 状態取得
	State GetState() const noexcept{
		return m_state.load(std::memory_order_acquire);
	}

	// 出力取得
	std::string GetOutput() const;

	// ============================
	// Token Info
	// ============================

	// 現在使用中のトークン数
	int GetTokenCount() const noexcept;

	// 最大コンテキストトークン数
	int GetMaxTokenCount() const noexcept;

	// 使用率（0.0f ～ 1.0f）
	float GetTokenUsageRate() const noexcept;


private:

	void ResetContextUnlocked();

	llama_context* CreateContext() const;
	llama_sampler* CreateSampler() const;

	// ============================
	// Internal Thread
	// ============================

	void WorkerMain();

	// [修正] retryDepth を追加。
	// コンテキストオーバーフロー時に SummarizeAndReset() を挟んで自分自身を
	// 再帰呼び出しする経路があるが、要約してもオーバーフローが解消しない
	// 異常系で無限再帰に陥らないようにするための深度ガード。
	void RunPromptInternal(const std::string& prompt, int retryDepth = 0);

	// ============================
	// Model / Config
	// ============================

	std::shared_ptr<LLAMAModelData>      m_model;
	std::shared_ptr<const AgentConfig>  m_config;

	// ============================
	// llama Runtime
	// ============================

	llama_context* m_ctx = nullptr;
	llama_sampler* m_sampler = nullptr;

	// 会話コンテキスト
	std::vector<llama_token> m_pastTokens;
	std::atomic<int> m_nPast = 0;

	// Agent側で一元管理する会話の履歴バッファ
	std::vector<MessageEntry> m_history;

	// ============================
	// Thread Control
	// ============================

	std::thread m_thread;

	std::queue<std::string> m_jobQueue;
	std::mutex m_mutex;
	std::condition_variable m_cv;

	std::atomic<State> m_state{State::Idle};
	std::atomic<bool>  m_running{true};
	std::atomic<bool>  m_cancelRequested{false}; // ジョブキャンセル用フラグ

	// ============================
	// Output
	// ============================

	mutable std::mutex m_outputMutex;
	std::string m_output;
	std::string m_visibleOutput; // UIへ安全に公開するためのクリーンなバッファ

	// ============================
	// Summary
	// ============================

	// Context 溢れ or prompt 過長時の要約
	// 内部で一時 llama_context を生成し同期実行
	void SummarizeAndReset();

	// 要約済みテキスト（次回プロンプトに注入）
	std::string m_summaryText;

	// 再入防止フラグ
	bool m_isSummarizing = false;
};