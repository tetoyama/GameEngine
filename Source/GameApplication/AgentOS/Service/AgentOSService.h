// =======================================================================
//
// AgentOSService.h
//
// AgentOSのエンジン側統合サービス。TaskStore/CapabilityRegistry/CommandPipeline/
// EngineTools/Orchestratorを所有し、EditorのAgentOSPanelから操作される（構想§12）。
//
// LLM（LLAMAAgent）は初回リクエスト時に遅延ロードする。Orchestratorも同様に、
// LLMバックエンドが準備できてから生成する（コンストラクタがILlmBackend*を要求するため）。
//
// SubmitRequestは1セッションのみを許可し、専用WorkerThreadでOrchestrator::RunSessionを
// 同期実行する。進捗・チャットログ・最終結果はmutexで保護したStateSnapshotとして
// PollingでUIへ公開する。
//
// =======================================================================
#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "Service/IService.h"

#include "../Core/AgentOsTypes.h"
#include "../Core/Json.h"
#include "../Core/Budget/Budget.h"
#include "../Core/Command/CapabilitySet.h"
#include "../Core/Command/CommandPipeline.h"
#include "../Core/Store/TaskStore.h"
#include "../Core/Llm/LoggingLlmBackend.h"

#include "../EngineTools/EngineToolContext.h"
#include "../EngineTools/MainThreadDispatcher.h"
#include "../EngineTools/WriteTrace.h"

class SceneManager;
class DebugLogService;
class LLAMAService;
class LLAMAAgent;

namespace agentos {

class Orchestrator;
class LlamaLlmBackend;

struct AgentOSServiceContext {
	SceneManager* sceneManager = nullptr;
	DebugLogService* debugLog = nullptr;
	LLAMAService* llamaService = nullptr;
	std::string modelPath;
	std::string dbPath = "Logs/AgentOS/agentos.db";
};

class AgentOSService : public IService {
public:
	struct ChatEntry {
		std::string role;
		std::string text;
		std::string processLog;
		std::int64_t elapsedMillis = 0;
		std::int64_t promptTokens = 0;
		std::int64_t completionTokens = 0;
	};

	struct StateSnapshot {
		bool running = false;
		std::string stage;
		std::vector<ChatEntry> chatLog;
		std::string lastReport;
		Json lastHypotheses = Json::object();
		Json progressDetail = Json::object();
		std::string errorMessage;
		std::string transcriptPath;
		std::string modelName;
		std::string targetEnvironment = "GameEngine / C++ / DirectX 11";
		bool generationActive = false;
		std::string liveThinking;
		std::string liveResponse;
		std::string sessionProcessLog;
		std::int64_t sessionElapsedMillis = 0;
		std::int64_t liveElapsedMillis = 0;
		std::int64_t livePromptTokens = 0;
		std::int64_t liveCompletionTokens = 0;
		std::int64_t totalPromptTokens = 0;
		std::int64_t totalCompletionTokens = 0;
		std::int64_t sessionPromptTokens = 0;
		std::int64_t sessionCompletionTokens = 0;
		double tokensPerSecond = 0.0;
	};

	AgentOSService();
	~AgentOSService() override;

	void Initialize(AgentOSServiceContext context);
	void Shutdown() override;

	void SubmitRequest(const std::string& text);
	void CancelCurrentRequest();
	void PumpMainThread(std::int64_t frameCounter);

	StateSnapshot GetSnapshot() const;
	Json GetAuditSnapshot() const;
	bool IsBusy() const;

private:
	enum class LlmLoadState : std::uint8_t {
		Unloaded,
		Loading,
		Ready,
		RetryableFailure,
	};

	void WorkerMain(std::string request);
	bool TryRunDeterministicFastPath(const std::string& request);
	bool EnsureLlmReady();
	void AppendChat(const std::string& role, const std::string& text);
	void AppendProcessEvent(const std::string& event);
	void SetStage(const std::string& stage);

	void OpenTranscriptForSession();
	void WriteTranscriptEvent(
		const std::string& kind,
		const std::vector<std::pair<std::string, std::string>>& scalarFields,
		const std::vector<std::pair<std::string, std::string>>& blockFields);

	AgentOSServiceContext m_context;

	TaskStore m_taskStore;
	CapabilityRegistry m_capabilityRegistry;
	std::unique_ptr<CommandPipeline> m_pipeline;
	MainThreadDispatcher m_dispatcher;
	WriteTracer m_tracer;
	EngineToolContext m_engineToolContext;

	std::shared_ptr<LLAMAAgent> m_llmAgent;
	std::unique_ptr<LlamaLlmBackend> m_llmBackend;
	std::unique_ptr<LoggingLlmBackend> m_loggingBackend;
	std::atomic<LlmLoadState> m_llmLoadState{LlmLoadState::Unloaded};

	std::unique_ptr<Orchestrator> m_orchestrator;
	mutable std::mutex m_backendMutex;

	std::thread m_worker;
	std::atomic<bool> m_running{false};
	std::atomic<bool> m_shutdownRequested{false};
	std::atomic<bool> m_cancelRequested{false};

	mutable std::mutex m_stateMutex;
	StateSnapshot m_state;
	std::chrono::steady_clock::time_point m_sessionStartedAt{};

	mutable std::mutex m_transcriptMutex;
	std::ofstream m_transcript;
	std::string m_transcriptPath;
};

} // namespace agentos
