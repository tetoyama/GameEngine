#pragma once


#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <functional>

// ============================
// 前方宣言
// ============================
class LLAMAModelData;
struct AgentConfig;
struct llama_context;
struct llama_sampler;

typedef int32_t llama_token;

// ============================
// LLAMAAgent
// ============================
class LLAMAAgent {
public:
    LLAMAAgent(
        const std::shared_ptr<LLAMAModelData>& model,
        const std::shared_ptr<const AgentConfig>& config
    );

    ~LLAMAAgent();

    // 同期実行
    std::string RunPrompt(const std::string& prompt);

    // 非同期実行
    void RunPromptAsync(
        const std::string& prompt,
        std::function<void(const std::string&)> callback
    );

private:
    void InternalRunPrompt(
        const std::string& prompt,
        std::string& output
    );

private:
    std::shared_ptr<LLAMAModelData> m_model;
    std::shared_ptr<const AgentConfig> m_config;
    
    llama_context* m_context = nullptr;
    llama_sampler* m_sampler = nullptr;

    std::vector<llama_token> m_pastTokens;
    int m_nPast = 0;

    std::mutex m_mutex;
};
