
#include "LLAMAService.h"

#include <Windows.h>
#include <debugapi.h>
#include <algorithm>

#include "LLAMAAgent.h"
#include "AgentConfig.h"

#include "Backends/llama/llama.h"
#include "Backends/llama/llama-graph.h"

#include "Resources/Data/llamaModelData.h"
#include "Resources/Loader/llamaModelLoader.h"
#include "Resources/resourceService.h"

#pragma comment(lib, "llama.lib")
// ============================
// Initialize / Shutdown
// ============================
void LLAMAService::Initialize(LLAMAServiceContext context){

	m_resourceService = context.resourceService;

	OutputDebugStringA("LLAMAService: Initialize called\n");

	llama_backend_init();

	m_threadRunning = true;
	m_workerThread = std::thread(&LLAMAService::WorkerThreadMain, this);
}

void LLAMAService::Shutdown(){
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
}

// ============================
// モデル管理 (同期)
// ============================
bool LLAMAService::LoadModel(const std::string& path){
	{
		std::lock_guard<std::mutex> lock(m_modelMutex);
		if(m_models.contains(path)){
			return true;
		}
	}
	auto modelData = m_resourceService->Load<LLAMAModelData>(path);

	llama_model_params params = llama_model_default_params();
	params.n_gpu_layers = 0;

	modelData->m_model = llama_model_load_from_file(path.c_str(), params);
	if(!modelData->m_model){
		return false;
	}

	modelData->m_vocab = llama_model_get_vocab(modelData->m_model);

	{
		std::lock_guard<std::mutex> lock(m_modelMutex);
		m_models[path] = modelData;
	}

	return true;
}

std::shared_ptr<LLAMAModelData> LLAMAService::GetModel(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_modelMutex);
    auto it = m_models.find(path);

	if (it != m_models.end()) {
		OutputDebugStringA(("LLAMAService: Model found: " + path + "\n").c_str());
	}

    return (it != m_models.end()) ? it->second : nullptr;
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
		{
			std::lock_guard<std::mutex> lock(m_modelMutex);

			if(m_models.contains(path)){
				m_pendingCallbacks[path].push_back(callback);
			} else if(m_pendingCallbacks.contains(path)){
				m_pendingCallbacks[path].push_back(callback);
				return;
			} else{
				m_pendingCallbacks[path].push_back(callback);
			}
		}

		{
			std::lock_guard<std::mutex> lock(m_jobMutex);
			m_modelJobQueue.push({path, nullptr});
		}

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
        OutputDebugStringA(("LLAMAService: Model not loaded: " + modelPath + "\n").c_str());
        return nullptr;
    }

    auto agent = std::make_shared<LLAMAAgent>(model, config);

    {
        std::lock_guard<std::mutex> lock(m_agentMutex);
        m_agents.push_back(agent);
    }

    return agent;
}

std::shared_ptr<LLAMAAgent> LLAMAService::CreateAgent(const std::shared_ptr<LLAMAModelData> model, const std::shared_ptr<const AgentConfig>& config){

	auto agent = std::make_shared<LLAMAAgent>(model, config);

	{
		std::lock_guard<std::mutex> lock(m_agentMutex);
		m_agents.push_back(agent);
	}

	return agent;
}


void LLAMAService::DestroyAgent(const std::shared_ptr<LLAMAAgent>& agent) {
    std::lock_guard<std::mutex> lock(m_agentMutex);
    auto it = std::find(m_agents.begin(), m_agents.end(), agent);
    if (it != m_agents.end()) {
        m_agents.erase(it);
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
        m_jobCV.notify_one();
}

// ============================
// ワーカースレッド
// ============================
void LLAMAService::WorkerThreadMain() {
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
}

// ============================
// 内部補助
// ============================
void LLAMAService::ProcessModelLoadJob(const ModelLoadJob& job){
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
}

void LLAMAService::ProcessAgentCreateJob(const AgentCreateJob& job){
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
}

void LLAMAService::PumpCallbacks(){
	std::queue<CompletedCallback> local;

	{
		std::lock_guard<std::mutex> lock(m_completedMutex);
		std::swap(local, m_completedCallbacks);
	}

	while(!local.empty()){
		local.front().fn();
		local.pop();
	}
}
