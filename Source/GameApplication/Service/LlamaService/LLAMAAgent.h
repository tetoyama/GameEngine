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
// ・会話コンテキスト（past tokens）を保持
// ・Context 溢れ時は SummaryAgent による要約を実行
// ・Editor / BRAIN からは State / Output のみ参照
//
class LLAMAAgent {
public:
	// ============================
	// State
	// ============================
	enum class State {
		Idle,       // 待機中
		Running,    // 推論実行中
		Stopping,   // 停止要求中
		Dead        // スレッド終了済み
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

	// 非同期実行（内部キューに積む）
	void RunAsync(const std::string& prompt);

	// 実行停止要求（即時停止は保証しない）
	void Stop();

	// 現在の状態取得
	State GetState() const noexcept{
		return m_state.load(std::memory_order_acquire);
	}

	// 現在の出力取得（ポーリング用）
	std::string GetOutput() const;

	// ============================
	// Summary Agent
	// ============================

	// SummaryAgent を外部から注入
	void SetSummaryAgent(std::shared_ptr<LLAMAAgent> agent);

	// 同期生成（SummaryAgent 専用）
	std::string GenerateSync(const std::string& prompt);

private:
	// ============================
	// Internal Thread
	// ============================

	// ワーカースレッド本体
	void WorkerMain();

	// 1 プロンプト分の推論処理
	void RunPromptInternal(const std::string& prompt);

	// ============================
	// Model / Config
	// ============================

	std::shared_ptr<LLAMAModelData>   m_model;
	std::shared_ptr<const AgentConfig> m_config;

	// ============================
	// llama Runtime
	// ============================

	llama_context* m_ctx = nullptr;
	llama_sampler* m_sampler = nullptr;

	// 会話コンテキスト
	std::vector<llama_token> m_pastTokens;
	int m_nPast = 0;

	// ============================
	// Thread Control
	// ============================

	std::thread m_thread;

	std::queue<std::string> m_jobQueue;
	std::mutex m_mutex;
	std::condition_variable m_cv;

	std::atomic<State> m_state{State::Idle};
	std::atomic<bool>  m_running{true};

	// ============================
	// Output
	// ============================

	mutable std::mutex m_outputMutex;
	std::string m_output;

	// ============================
	// Context Utilities
	// ============================

	// incomingTokens を考慮してコンテキストを維持
	void EnsureContextFits(size_t incomingTokens);

	// llama_context を初期状態に戻す
	void ResetContext();

	// pastTokens を文字列化
	std::string DecodeTokensToString() const;

	// ============================
	// Summary Support
	// ============================

	// Context 溢れ時の要約 → 再構築
	void SummarizeAndReset();

	// SummaryAgent 本体
	std::shared_ptr<LLAMAAgent> m_summaryAgent;

	// SummaryAgent 待機制御
	mutable std::mutex m_summaryMutex;
	std::condition_variable m_summaryCv;

	// 要約済みシステムプロンプト
	std::string m_summaryText;
	std::atomic<bool> m_isSummarizing{false};

};
