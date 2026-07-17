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

// ---------------------------------
// AgentOSServiceContext
// ---------------------------------
struct AgentOSServiceContext {
	SceneManager* sceneManager = nullptr;
	DebugLogService* debugLog = nullptr;
	LLAMAService* llamaService = nullptr;
	std::string modelPath;
	std::string dbPath = "Logs/AgentOS/agentos.db";
};

// ---------------------------------
// AgentOSService
// ---------------------------------
class AgentOSService : public IService {
public:
	// UIへ公開するスナップショット（コピーして返す。mutexで保護）。
	struct StateSnapshot {
		bool running = false;
		std::string stage;
		std::vector<std::pair<std::string, std::string>> chatLog; // (role, text)
		std::string lastReport;
		Json lastHypotheses = Json::object(); // OrchestratorResult::rankedHypotheses と同じ形
		Json progressDetail = Json::object();
		std::string errorMessage;
		// 現在セッションのTranscript（YAML）ファイルパス。共有・デバッグ用。
		std::string transcriptPath;
	};

	AgentOSService();
	// std::unique_ptr<Orchestrator>（前方宣言のみ）をメンバに持つため、
	// 暗黙destructorをヘッダでインスタンス化させないよう明示的に宣言し、
	// Orchestrator.hをincludeしたAgentOSService.cpp側で定義する。
	// （そうしないと、Orchestrator.hを知らない他TU（engineContext.cpp等）が
	// このクラスをdeleteする際に不完全型エラーになる）。
	~AgentOSService() override;

	void Initialize(AgentOSServiceContext context);
	void Shutdown() override;

	// 既にセッション実行中なら何もせず拒否する（IsBusy()参照）。
	void SubmitRequest(const std::string& text);

	// Editor描画スレッド（Main Thread）から毎フレーム呼ぶ。
	// MainThreadDispatcherのPumpと、WriteTracerのアクティブ時Sampleを行う。
	void PumpMainThread(std::int64_t frameCounter);

	StateSnapshot GetSnapshot() const;

	// CommandPipelineの監査ログをJSON化して返す（Auditタブ用）。
	Json GetAuditSnapshot() const;

	bool IsBusy() const;

private:
	void WorkerMain(std::string request);
	bool EnsureLlmReady();
	void AppendChat(const std::string& role, const std::string& text);
	void SetStage(const std::string& stage);

	// ---- Transcript（YAMLストリーム） ----
	// セッション単位で Logs/AgentOS/transcript_YYYYMMDD_HHMMSS.yaml を作成し、
	// user_request / stage / llm_call / result の全イベントを追記する。
	// 各イベントは "---" 区切りのYAMLドキュメントで、書き込みごとにflushする
	// （クラッシュしても直前までのログが残る）。
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
	// LlamaLlmBackendを包み、全LLM入出力をTranscriptへ記録するデコレータ。
	// Orchestratorへはこちらを渡す。
	std::unique_ptr<LoggingLlmBackend> m_loggingBackend;
	bool m_llmLoadAttempted = false;
	bool m_llmReady = false;

	std::unique_ptr<Orchestrator> m_orchestrator;

	std::thread m_worker;
	std::atomic<bool> m_running{false};

	mutable std::mutex m_stateMutex;
	StateSnapshot m_state;

	mutable std::mutex m_transcriptMutex;
	std::ofstream m_transcript;
	std::string m_transcriptPath;
};

} // namespace agentos
