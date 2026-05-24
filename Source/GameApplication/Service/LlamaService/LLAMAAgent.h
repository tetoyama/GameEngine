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
class llamaagent{
public:
	// ============================
	// State
	// ============================

	enum class state{
		Idle,
		Running,
		Stopping,
		Dead
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
	void RunPromptInternal(const std::string& prompt);

	// ============================
	// Model / Config
	// ============================

	std::shared_ptr<LLAMAModelData> m_Model;
	std::shared_ptr<const AgentConfig> m_Config;

	// ============================
	// llama Runtime
	// ============================

	llama_context* m_pCtx = nullptr;
	llama_sampler* m_pSampler = nullptr;

	// 会話コンテキスト
	std::vector<llama_token> m_PastTokens;
	int m_NPast= 0;

	// ============================
	// Thread Control
	// ============================

	std::thread m_Thread;

	std::queue<std::string> m_JobQueue;
	std::mutex m_Mutex;
	std::condition_variable m_Cv;

	std::atomic<State> m_State{State::Idle};
	std::atomic<bool> m_Running{true};

	// ============================
	// Output
	// ============================

	mutable std::mutex m_OutputMutex;
	std::string m_Output;

	// ============================
	// Summary
	// ============================

	// Context 溢れ or prompt 過長時の要約
	// 内部で一時 llama_context を生成し同期実行
	void SummarizeAndReset();

	// 要約済みテキスト（次回プロンプトに注入）
	std::string m_SummaryText;

	// 再入防止フラグ
	bool m_IsSummarizing= false;
};
