#include "LLAMAAgent.h"
#include <Windows.h>
#include "Resources/Data/llamaModelData.h"
#include "AgentConfig.h"

#include "Backends/llama/llama.h"

// ============================
// ctor / dtor
// ============================
LLAMAAgent::LLAMAAgent(
    const std::shared_ptr<LLAMAModelData>& model,
    const std::shared_ptr<const AgentConfig>& config)
    : m_model(model)
    , m_config(config) {}

LLAMAAgent::~LLAMAAgent() = default;

std::string LLAMAAgent::RunPrompt(const std::string& prompt) {
    return std::string();
}

void LLAMAAgent::RunPromptAsync(const std::string& prompt, std::function<void(const std::string&)> callback) {}

void LLAMAAgent::InternalRunPrompt(const std::string& prompt, std::string& output) {}
