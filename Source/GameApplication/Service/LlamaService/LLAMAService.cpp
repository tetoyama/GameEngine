
#include "LLAMAService.h"

#include <Windows.h>
#include <debugapi.h>
#include <algorithm>

// ===== 実体定義は cpp 側で include =====
#include "Resources/Data/llamaModelData.h"
#include "LLAMAAgent.h"
#include "AgentConfig.h"

#include "Backends/llama/llama.h"
#include "Backends/llama/llama-graph.h"

#pragma comment(lib, "llama.lib")
// ============================
// Initialize / Shutdown
// ============================
void LLAMAService::Initialize() {
    OutputDebugStringA("LLAMAService: Initialize called\n");

    llama_backend_init();

    m_threadRunning = true;
    m_workerThread = std::thread(&LLAMAService::WorkerThreadMain, this);
}

void LLAMAService::Shutdown() {
    OutputDebugStringA("LLAMAService: Shutdown called\n");

    // ワーカースレッド停止
    m_threadRunning = false;
    m_jobCV.notify_all();
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    // エージェント破棄
    {
        std::lock_guard<std::mutex> lock(m_agentMutex);
        m_agents.clear();
    }

    // モデル破棄
    {
        std::lock_guard<std::mutex> lock(m_modelMutex);
        m_models.clear();
        m_pendingCallbacks.clear();
    }
}

// ============================
// モデル管理 (同期)
// ============================
bool LLAMAService::LoadModel(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_modelMutex);

    if (m_models.contains(path)) {
        OutputDebugStringA(("LLAMAService: Model already loaded: " + path + "\n").c_str());
        return true;
    }

    auto modelData = std::make_shared<LLAMAModelData>();
    modelData->m_path = path;

    llama_model_params params = llama_model_default_params();
    params.n_gpu_layers = 0;

    modelData->m_model = llama_model_load_from_file(path.c_str(), params);
    if (!modelData->m_model) {
        OutputDebugStringA(("LLAMAService: Failed to load model: " + path + "\n").c_str());
        return false;
    }

    modelData->m_vocab = llama_model_get_vocab(modelData->m_model);
    m_models[path] = modelData;

    OutputDebugStringA(("LLAMAService: Model loaded: " + path + "\n").c_str());
    return true;
}

void LLAMAService::UnloadModel(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_modelMutex);
    m_models.erase(path);
}

std::shared_ptr<LLAMAModelData> LLAMAService::GetModel(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_modelMutex);
    auto it = m_models.find(path);
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
    std::function<void(bool)> callback) {

        {
            std::lock_guard<std::mutex> lock(m_modelMutex);

            if (m_models.contains(path)) {
                callback(true);
                return;
            }

            if (m_pendingCallbacks.contains(path)) {
                m_pendingCallbacks[path].push_back(callback);
                return;
            }

            m_pendingCallbacks[path].push_back(callback);
        }

        {
            std::lock_guard<std::mutex> lock(m_jobMutex);
            m_modelJobQueue.push({ path, callback });
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
void LLAMAService::ProcessModelLoadJob(const ModelLoadJob& job) {
    bool success = LoadModel(job.path);

    std::vector<std::function<void(bool)>> callbacks;
    {
        std::lock_guard<std::mutex> lock(m_modelMutex);
        auto it = m_pendingCallbacks.find(job.path);
        if (it != m_pendingCallbacks.end()) {
            callbacks = std::move(it->second);
            m_pendingCallbacks.erase(it);
        }
    }

    for (auto& cb : callbacks) {
        cb(success);
    }
}

void LLAMAService::ProcessAgentCreateJob(const AgentCreateJob& job) {
    auto agent = CreateAgent(job.modelPath, job.config);
    if (job.callback) {
        job.callback(agent);
    }
}
