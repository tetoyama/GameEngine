#pragma once


#include <memory>
#include <string>
#include <mutex>
#include <functional>

// ============================
// 前方宣言
// ============================
class LLAMAModelData;
struct AgentConfig;

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

    std::mutex m_mutex;
};
