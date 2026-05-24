// =======================================================================
// 
// LLAMAService.h
// 
// =======================================================================
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
class DebugLogService;

struct LLAMAModelData;
class llamaagent;
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
	explicit LLAMAService(DebugLogService* debugLog = nullptr)
		: m_debugLog(debugLog)
	{}

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

	std::shared_ptr<LLAMAAgent> CreateAgent(
		const std::shared_ptr<LLAMAModelData> model,
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
	DebugLogService* m_debugLog = nullptr;

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
    std::unordered_map<std::string, std::shared_ptr<LLAMAModelData>> models;
    mutable std::mutex modelMutex;

    std::unordered_map<
        std::string,
        std::vector<std::function<void(bool)>>
    > m_pendingCallbacks;

    // ============================
    // エージェント管理
    // ============================
    std::vector<std::shared_ptr<LLAMAAgent>> agents;
    mutable std::mutex agentMutex;

    // ============================
    // 非同期ジョブキュー
    // ============================
    std::queue<ModelLoadJob> modelJobQueue;
    std::queue<AgentCreateJob> agentJobQueue;

    std::mutex jobMutex;
    std::condition_variable jobCv;
	std::atomic<bool> threadRunning{false};
	std::thread workerThread;

	// -------------------------
	// LLM
	// -------------------------
	std::shared_ptr<LLAMAModelData> llamaModel;
	std::shared_ptr<AgentConfig> agentConfig;
	std::shared_ptr<LLAMAAgent> mainAgent;
	std::shared_ptr<LLAMAAgent> summaryAgent;

	// -------------------------
	// UI state
	// -------------------------
	char inputBuffer[2048]{};
	bool scrollToBottom= false;

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

	std::queue<CompletedCallback> completedCallbacks;
	std::mutex completedMutex;

	void PumpCallbacks();
};
