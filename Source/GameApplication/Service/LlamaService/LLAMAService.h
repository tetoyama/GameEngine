// =======================================================================
// 
// LLAMAService.h
// 
// =======================================================================
#pragma once


#include <cstddef>
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
	explicit LLAMAService(DebugLogService* debugLog = nullptr)
		: m_debugLog(debugLog)
	{}

    // ===== IService =====
    void Initialize(LLAMAServiceContext context);
    void Shutdown() override;

    // ===== 計算バックエンド =====
    //
    // llama.cppはバックエンドをDLLとして動的に読み込む（ggml-vulkan.dll 等）。
    // 探索は llama_backend_init() の中で ggml_backend_load_all() が行うため、
    // exeの隣にDLLを置くだけで登録される。無ければCPUのみで動作する。
    //
    // 注意: ggml_backend_* を直接呼ばないこと。
    // このプロジェクトがリンクしているのは llama.lib だけで（LLAMAService.cpp の
    // #pragma comment 参照）、ggml側のインポートライブラリが無いため
    // 未解決の外部シンボルになる。状態の取得は llama.h の API で行う。
    struct BackendInfo {
        // GPUオフロードが可能か。falseならn_gpu_layersを指定しても
        // 黙ってCPUへ落ちる（DLLの配置が必要）。
        bool gpuOffloadSupported = false;

        // オフロード先として扱えるデバイスの上限
        std::size_t maxDevices = 0;

        // llama_print_system_info() の内容。
        // 有効なバックエンドやCPU命令セットが列挙される。
        std::string systemInfo;
    };

    const BackendInfo& GetBackendInfo() const noexcept { return m_backendInfo; }

    // GPUバックエンドが使えるか。
    bool HasGpuBackend() const noexcept { return m_backendInfo.gpuOffloadSupported; }

    // ===== モデル管理 =====
    // gpuLayers: GPUへオフロードする層数。0でCPUのみ、-1で全層。
    //            llama_model_paramsはモデルロード時に決まるため、
    //            変更するにはモデルの再ロードが必要になる。
    bool LoadModel(const std::string& path, int gpuLayers = 0);

    // 指定モデルを解放して再ロードする（GPU設定の切り替え用）。
    bool ReloadModel(const std::string& path, int gpuLayers);
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

	// バックエンドの状態（Initialize時に一度だけ確定する）
	BackendInfo m_backendInfo;

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
    // llama.hのAPIでバックエンドの状態を取得してm_backendInfoを埋める。
    void DetectBackendInfo();

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
