// =======================================================================
// 
// LLAMAService.cpp
// 
// =======================================================================
#include "LLAMAService.h"

#include <Windows.h>
#include <debugapi.h>
#include <algorithm>

#include "LLAMAAgent.h"
#include "AgentConfig.h"
#include "DebugTools/DebugSystem.h"

#include "Backends/llama/llama.h"
#include "Backends/llama/llama-graph.h"

#include "Resources/Data/llamaModelData.h"
#include "Resources/Loader/llamaModelLoader.h"
#include "Resources/resourceService.h"

#pragma comment(lib, "llama.lib")
#define LLAMA_SERVICE_LOG(level, msg) do { if(m_debugLog) { m_debugLog->Log(level, msg, __FUNCTION__, __FILE__, __LINE__); } } while(0)
// ============================
// Initialize / Shutdown
// ============================
void LLAMAService::Initialize(LLAMAServiceContext context){

	m_resourceService = context.resourceService;
	if(!m_resourceService){
		LLAMA_SERVICE_LOG(LogLevel::Error, "ResourceService が未設定のため LLAMAService の初期化を中止します");
		return;
	}

	LLAMA_SERVICE_LOG(LogLevel::Info, "LLAMAService の初期化を開始します");
	OutputDebugStringA("LLAMAService: Initialize called\n");

	llama_backend_init();

	m_threadRunning = true;
	m_workerThread = std::thread(&LLAMAService::WorkerThreadMain, this);
	LLAMA_SERVICE_LOG(LogLevel::Info, "LLAMAService の初期化が完了しました");
}

void LLAMAService::Shutdown(){
	LLAMA_SERVICE_LOG(LogLevel::Info, "LLAMAService を終了します");
	m_threadRunning = false;
	m_jobCV.notify_all();

	if(m_workerThread.joinable()){
		m_workerThread.join();
	}

	{
		std::lock_guard<std::mutex> lock(m_completedMutex);
		while(!m_completedCallbacks.empty()){
			m_completedCallbacks.pop();
		}
	}

	{
		std::lock_guard<std::mutex> lock(m_agentMutex);
		m_agents.clear();
	}

	{
		std::lock_guard<std::mutex> lock(m_modelMutex);
		m_models.clear();
		m_pendingCallbacks.clear();
	}

	llama_backend_free();
	LLAMA_SERVICE_LOG(LogLevel::Info, "LLAMAService の終了が完了しました");
}

// ============================
// モデル管理 (同期)
// ============================
bool LLAMAService::LoadModel(const std::string& path){
	LLAMA_SERVICE_LOG(LogLevel::Info, ("LLAMA モデルのロードを開始します: " + path));
	{
		std::lock_guard<std::mutex> lock(m_modelMutex);
		if(m_models.contains(path)){
			LLAMA_SERVICE_LOG(LogLevel::Debug, ("LLAMA モデルは既にロード済みです: " + path));
			return true;
		}
	}
	auto modelData = m_resourceService->Load<LLAMAModelData>(path);
	if(!modelData){
		LLAMA_SERVICE_LOG(LogLevel::Error, ("LLAMA モデルデータの取得に失敗しました: " + path));
		return false;
	}

	llama_model_params params = llama_model_default_params();
	params.n_gpu_layers = 0;

	modelData->m_model = llama_model_load_from_file(path.c_str(), params);
	if(!modelData->m_model){
		LLAMA_SERVICE_LOG(LogLevel::Error, ("LLAMA モデルのロードに失敗しました: " + path));
		return false;
	}

	modelData->m_vocab = llama_model_get_vocab(modelData->m_model);

	{
		std::lock_guard<std::mutex> lock(m_modelMutex);
		m_models[path] = modelData;
	}

	LLAMA_SERVICE_LOG(LogLevel::Info, ("LLAMA モデルのロードが完了しました: " + path));
	return true;
}

std::shared_ptr<LLAMAModelData> LLAMAService::GetModel(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_modelMutex);
    auto it = m_models.find(path);

	if (it != m_models.end()) {
		LLAMA_SERVICE_LOG(LogLevel::Trace, ("LLAMA モデルを取得しました: " + path));
		OutputDebugStringA(("LLAMAService: Model found: " + path + "\n").c_str());
		return it->second;
	}

    return nullptr;
}

std::vector<std::shared_ptr<LLAMAModelData>> LLAMAService::GetLoadedModels() const {
    std::vector<std::shared_ptr<LLAMAModelData>> result;
    std::lock_guard<std::mutex> lock(m_modelMutex);
    for (const auto& [_, model] : m_models) {
        result.push_back(model);
    }
    return result;
}

// ============================
// モデル管理 (非同期)
// ============================
void LLAMAService::LoadModelAsync(
	const std::string& path,
	std::function<void(bool)> callback){
		bool shouldQueueJob = false;
		{
			std::lock_guard<std::mutex> lock(m_modelMutex);

			shouldQueueJob = !m_models.contains(path) && !m_pendingCallbacks.contains(path);
			m_pendingCallbacks[path].push_back(callback);
		}

		if(!shouldQueueJob){
			LLAMA_SERVICE_LOG(LogLevel::Trace, ("LLAMA モデルの非同期ロード要求を既存ジョブへ合流しました: " + path));
			return;
		}

		{
			std::lock_guard<std::mutex> lock(m_jobMutex);
			m_modelJobQueue.push({path, nullptr});
		}

		LLAMA_SERVICE_LOG(LogLevel::Debug, ("LLAMA モデルの非同期ロードをキューへ追加しました: " + path));
		m_jobCV.notify_one();
}


// ============================
// エージェント管理 (同期)
// ============================
std::shared_ptr<LLAMAAgent> LLAMAService::CreateAgent(
    const std::string& modelPath,
    const std::shared_ptr<const AgentConfig>& config) {

    auto model = GetModel(modelPath);
    if (!model) {
        LLAMA_SERVICE_LOG(LogLevel::Error, ("エージェント生成に必要なモデルが未ロードです: " + modelPath));
        OutputDebugStringA(("LLAMAService: Model not loaded: " + modelPath + "\n").c_str());
        return nullptr;
    }

    auto agent = std::make_shared<LLAMAAgent>(model, config);

    {
        std::lock_guard<std::mutex> lock(m_agentMutex);
        m_agents.push_back(agent);
    }

    LLAMA_SERVICE_LOG(LogLevel::Info, ("LLAMA エージェントを生成しました: " + modelPath));
    return agent;
}

std::shared_ptr<LLAMAAgent> LLAMAService::CreateAgent(const std::shared_ptr<LLAMAModelData> model, const std::shared_ptr<const AgentConfig>& config){

	auto agent = std::make_shared<LLAMAAgent>(model, config);

	{
		std::lock_guard<std::mutex> lock(m_agentMutex);
		m_agents.push_back(agent);
	}

	LLAMA_SERVICE_LOG(LogLevel::Info, "LLAMA エージェントを生成しました");
	return agent;
}


void LLAMAService::DestroyAgent(const std::shared_ptr<LLAMAAgent>& agent) {
    std::lock_guard<std::mutex> lock(m_agentMutex);
    auto it = std::find(m_agents.begin(), m_agents.end(), agent);
    if (it != m_agents.end()) {
        m_agents.erase(it);
        LLAMA_SERVICE_LOG(LogLevel::Debug, "LLAMA エージェントを破棄しました");
    }
}

// ============================
// エージェント管理 (非同期)
// ============================
void LLAMAService::CreateAgentAsync(
    const std::string& modelPath,
    const std::shared_ptr<const AgentConfig>& config,
    std::function<void(std::shared_ptr<LLAMAAgent>)> callback) {

        {
            std::lock_guard<std::mutex> lock(m_jobMutex);
            m_agentJobQueue.push({ modelPath, config, callback });
        }
        LLAMA_SERVICE_LOG(LogLevel::Debug, ("LLAMA エージェント生成ジョブをキューへ追加しました: " + modelPath));
        m_jobCV.notify_one();
}

// ============================
// ワーカースレッド
// ============================
void LLAMAService::WorkerThreadMain() {
    LLAMA_SERVICE_LOG(LogLevel::Debug, "LLAMA ワーカースレッドを開始します");
    while (m_threadRunning) {
        ModelLoadJob modelJob;
        AgentCreateJob agentJob;
        bool hasModelJob = false;
        bool hasAgentJob = false;

        {
            std::unique_lock<std::mutex> lock(m_jobMutex);
            m_jobCV.wait(lock, [this]() {
                return !m_modelJobQueue.empty()
                    || !m_agentJobQueue.empty()
                    || !m_threadRunning;
            });

            if (!m_threadRunning) {
                break;
            }

            if (!m_modelJobQueue.empty()) {
                modelJob = m_modelJobQueue.front();
                m_modelJobQueue.pop();
                hasModelJob = true;
            } else if (!m_agentJobQueue.empty()) {
                agentJob = m_agentJobQueue.front();
                m_agentJobQueue.pop();
                hasAgentJob = true;
            }
        }

        if (hasModelJob) {
            ProcessModelLoadJob(modelJob);
        }
        if (hasAgentJob) {
            ProcessAgentCreateJob(agentJob);
        }
    }
    LLAMA_SERVICE_LOG(LogLevel::Debug, "LLAMA ワーカースレッドを終了します");
}

// ============================
// 内部補助
// ============================
void LLAMAService::ProcessModelLoadJob(const ModelLoadJob& job){
	LLAMA_SERVICE_LOG(LogLevel::Trace, ("LLAMA モデルロードジョブを処理します: " + job.path));
	bool success = LoadModel(job.path);

	std::vector<std::function<void(bool)>> callbacks;
	{
		std::lock_guard<std::mutex> lock(m_modelMutex);
		auto it = m_pendingCallbacks.find(job.path);
		if(it != m_pendingCallbacks.end()){
			callbacks = std::move(it->second);
			m_pendingCallbacks.erase(it);
		}
	}

	if(!m_threadRunning) return;

	{
		std::lock_guard<std::mutex> lock(m_completedMutex);
		for(auto& cb : callbacks){
			m_completedCallbacks.push({
				[cb, success](){ cb(success); }
									  });
		}
	}
	LLAMA_SERVICE_LOG(LogLevel::Debug, ("LLAMA モデルロードジョブの処理が完了しました: " + job.path));
}

void LLAMAService::ProcessAgentCreateJob(const AgentCreateJob& job){
	LLAMA_SERVICE_LOG(LogLevel::Trace, ("LLAMA エージェント生成ジョブを処理します: " + job.modelPath));
	auto agent = CreateAgent(job.modelPath, job.config);

	if(!job.callback || !m_threadRunning) return;

	{
		std::lock_guard<std::mutex> lock(m_completedMutex);
		m_completedCallbacks.push({
			[cb = job.callback, agent](){
				cb(agent);
			}
		});
	}
	LLAMA_SERVICE_LOG(LogLevel::Debug, ("LLAMA エージェント生成ジョブの処理が完了しました: " + job.modelPath));
}

void LLAMAService::PumpCallbacks(){
	std::queue<CompletedCallback> local;

	{
		std::lock_guard<std::mutex> lock(m_completedMutex);
		std::swap(local, m_completedCallbacks);
	}

	if(!local.empty()){
		LLAMA_SERVICE_LOG(LogLevel::Trace, "LLAMA 完了コールバックをポンプします");
	}
	while(!local.empty()){
		local.front().fn();
		local.pop();
	}
}

#undef LLAMA_SERVICE_LOG
