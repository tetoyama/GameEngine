#pragma once


#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <queue>
#include <functional>
#include <condition_variable>
#include <atomic>
#include <thread>

#include "Service/IService.h"

// ============================
// 前方宣言のみ
// ============================
class ResourceService;

struct LLAMAModelData;
class LLAMAAgent;
struct AgentConfig;

struct LLAMAServiceContext
{
	ResourceService* resourceService = nullptr;
};

// ============================
// LLAMAService
// ============================
// LLAMAモデルのロード・管理とエージェント生成を行う
// 非同期ロード・生成をサポートし、スレッド安全性を厳密に管理
class LLAMAService final : public IService {
public:
	LLAMAService() = default;

    // ===== IService =====
    void Initialize(LLAMAServiceContext context);
    void Shutdown() override;

    // ===== モデル管理 =====
    bool LoadModel(const std::string& path);
    std::shared_ptr<LLAMAModelData> GetModel(const std::string& path);
    std::vector<std::shared_ptr<LLAMAModelData>> GetLoadedModels() const;

    // ===== 非同期ロード =====
    void LoadModelAsync(
        const std::string& path,
        std::function<void(bool)> callback
    );

    // ===== エージェント管理 =====
    std::shared_ptr<LLAMAAgent> CreateAgent(
        const std::string& modelPath,
        const std::shared_ptr<const AgentConfig>& config
    );

    void DestroyAgent(const std::shared_ptr<LLAMAAgent>& agent);

    // ===== 非同期エージェント生成 =====
    void CreateAgentAsync(
        const std::string& modelPath,
        const std::shared_ptr<const AgentConfig>& config,
        std::function<void(std::shared_ptr<LLAMAAgent>)> callback
    );

private:

	ResourceService* m_resourceService = nullptr;

    // ============================
    // 非同期ジョブ構造体
    // ============================
    struct ModelLoadJob {
        std::string path;
        std::function<void(bool)> callback;
    };

    struct AgentCreateJob {
        std::string modelPath;
        std::shared_ptr<const AgentConfig> config;
        std::function<void(std::shared_ptr<LLAMAAgent>)> callback;
    };

    // ============================
    // モデル管理
    // ============================
    std::unordered_map<std::string, std::shared_ptr<LLAMAModelData>> m_models;
    mutable std::mutex m_modelMutex;

    std::unordered_map<
        std::string,
        std::vector<std::function<void(bool)>>
    > m_pendingCallbacks;

    // ============================
    // エージェント管理
    // ============================
    std::vector<std::shared_ptr<LLAMAAgent>> m_agents;
    mutable std::mutex m_agentMutex;

    // ============================
    // 非同期ジョブキュー
    // ============================
    std::queue<ModelLoadJob>  m_modelJobQueue;
    std::queue<AgentCreateJob> m_agentJobQueue;

    std::mutex m_jobMutex;
    std::condition_variable m_jobCV;
	std::atomic<bool> m_threadRunning{false};
	std::thread m_workerThread;

	// -------------------------
	// LLM
	// -------------------------
	std::shared_ptr<LLAMAModelData> m_llamaModel;
	std::shared_ptr<AgentConfig>    m_agentConfig;
	std::shared_ptr<LLAMAAgent>     m_mainAgent;
	std::shared_ptr<LLAMAAgent>     m_summaryAgent;

	// -------------------------
	// UI state
	// -------------------------
	char inputBuffer[2048]{};
	bool m_scrollToBottom = false;

    // ============================
    // 内部処理
    // ============================
    void WorkerThreadMain();

    void ProcessModelLoadJob(const ModelLoadJob& job);
    void ProcessAgentCreateJob(const AgentCreateJob& job);

	// 完了コールバック（メインスレッド実行用）
	struct CompletedCallback {
		std::function<void()> fn;
	};

	std::queue<CompletedCallback> m_completedCallbacks;
	std::mutex m_completedMutex;

	void PumpCallbacks();
};
