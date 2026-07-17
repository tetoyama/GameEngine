// =======================================================================
//
// AgentOSService.cpp
//
// =======================================================================
#include "AgentOSService.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <sstream>

#include "LlamaLlmBackend.h"
#include "../Core/Orchestrator/Orchestrator.h"
#include "../EngineTools/EngineToolRegistry.h"

#include "Service/LlamaService/LLAMAService.h"
#include "Service/LlamaService/LLAMAAgent.h"
#include "Service/LlamaService/AgentConfig.h"
#include "DebugTools/DebugSystem.h"

namespace agentos {

namespace {

// "20260717_153201" 形式と "2026-07-17 15:32:01" 形式のタイムスタンプを返す。
void MakeTimestamps(std::string* compact, std::string* readable) {
	const std::time_t now = std::time(nullptr);
	std::tm timeInfo{};
#ifdef _WIN32
	localtime_s(&timeInfo, &now);
#else
	localtime_r(&now, &timeInfo);
#endif
	char buffer[32]{};
	if(compact){
		std::strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", &timeInfo);
		*compact = buffer;
	}
	if(readable){
		std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeInfo);
		*readable = buffer;
	}
}

// 複数行テキストをYAMLのliteral block（key: | 形式）として書き出す。
// 行ごとに2スペースでインデントし、\rは除去する。
void AppendYamlBlock(std::ostream& out, const std::string& key, const std::string& text) {
	out << key << ": |\n";
	std::istringstream stream(text);
	std::string line;
	while(std::getline(stream, line)){
		if(!line.empty() && line.back() == '\r'){
			line.pop_back();
		}
		out << "  " << line << "\n";
	}
}

} // namespace

// unique_ptr<Orchestrator>/unique_ptr<LlamaLlmBackend>の完全型がそろうこのTUで
// コンストラクタ・デストラクタを定義する（ヘッダでの暗黙定義を避けるため）。
AgentOSService::AgentOSService() = default;
AgentOSService::~AgentOSService() = default;

// -----------------------------------------------------------------------
// Initialize
// -----------------------------------------------------------------------
void AgentOSService::Initialize(AgentOSServiceContext context) {
	m_context = std::move(context);

	m_engineToolContext.sceneManager = m_context.sceneManager;
	m_engineToolContext.debugLog = m_context.debugLog;

	// DBの保存先ディレクトリを用意してからTaskStoreを開く。
	std::error_code errorCode;
	const std::filesystem::path dbPath(m_context.dbPath);
	if(dbPath.has_parent_path()){
		std::filesystem::create_directories(dbPath.parent_path(), errorCode);
	}

	const Result openResult = m_taskStore.Open(m_context.dbPath);
	if(!openResult && m_context.debugLog){
		m_context.debugLog->LOG_ERROR(
			"AgentOSService: TaskStore::Open failed: " + openResult.error);
	}

	m_pipeline = std::make_unique<CommandPipeline>(&m_capabilityRegistry);
	RegisterEngineTools(*m_pipeline, m_engineToolContext, m_dispatcher, m_tracer);

	if(m_context.debugLog){
		m_context.debugLog->LOG_INFO("AgentOSService: initialized.");
	}
}

// -----------------------------------------------------------------------
// Shutdown
// -----------------------------------------------------------------------
void AgentOSService::Shutdown() {
	// RunSession()は内部でBudget上限まで自走して自然に終了する設計のため、
	// ここではWorkerの完了をjoinで待つのみ（強制中断はしない）。
	if(m_worker.joinable()){
		m_worker.join();
	}

	if(m_llmAgent){
		m_llmAgent->Stop();
	}

	m_orchestrator.reset();
	m_loggingBackend.reset();
	m_llmBackend.reset();
	m_llmAgent.reset();
	m_pipeline.reset();

	{
		std::lock_guard<std::mutex> lock(m_transcriptMutex);
		if(m_transcript.is_open()){
			m_transcript.close();
		}
	}

	// TaskStoreに公開Close()は無く、~SqliteDb()がClose()するため、
	// ここでは明示close不要（AgentOSServiceの破棄と共にRAIIで閉じる）。
}

// -----------------------------------------------------------------------
// SubmitRequest
// -----------------------------------------------------------------------
void AgentOSService::SubmitRequest(const std::string& text) {
	if(IsBusy()){
		if(m_context.debugLog){
			m_context.debugLog->LOG_WARNING(
				"AgentOSService: request rejected, a session is already running.");
		}
		return;
	}

	// 前回のWorkerはIsBusy()==falseの時点で完了している（WorkerMain末尾でm_running
	// をfalseに戻すため）ので、ここでのjoinは即座に返る。
	if(m_worker.joinable()){
		m_worker.join();
	}

	m_running.store(true);
	AppendChat("user", text);
	SetStage("starting");
	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		m_state.errorMessage.clear();
	}

	OpenTranscriptForSession();
	WriteTranscriptEvent("user_request", {}, {{"text", text}});

	m_worker = std::thread(&AgentOSService::WorkerMain, this, text);
}

// -----------------------------------------------------------------------
// WorkerMain
// -----------------------------------------------------------------------
void AgentOSService::WorkerMain(std::string request) {
	struct RunningGuard {
		std::atomic<bool>& flag;
		~RunningGuard() { flag.store(false); }
	} guard{m_running};

	if(!EnsureLlmReady()){
		SetStage("error");
		std::lock_guard<std::mutex> lock(m_stateMutex);
		m_state.errorMessage = "LLM model failed to load: " + m_context.modelPath;
		return;
	}

	if(!m_orchestrator){
		OrchestratorConfig config;
		config.budget = Budget{};
		config.maxRepairRounds = 2;

		m_orchestrator = std::make_unique<Orchestrator>(
			m_loggingBackend.get(),
			m_pipeline.get(),
			&m_taskStore,
			&m_capabilityRegistry,
			config
		);

		m_orchestrator->SetProgressCallback(
			[this](const std::string& stage, const Json& detail){
				SetStage(stage);
				WriteTranscriptEvent(
					"stage",
					{{"stage", stage}},
					{{"detail", detail.dump(2)}});
				std::lock_guard<std::mutex> lock(m_stateMutex);
				m_state.progressDetail = detail;
			}
		);
	}

	const OrchestratorResult result = m_orchestrator->RunSession(request);

	WriteTranscriptEvent(
		"result",
		{{"completed", result.completed ? "true" : "false"},
		 {"sessionId", std::to_string(result.sessionId)}},
		{{"report", result.report},
		 {"stopInfo", result.stopInfo.dump(2)},
		 {"rankedHypotheses", result.rankedHypotheses.dump(2)}});

	AppendChat("assistant", result.report);
	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		m_state.lastReport = result.report;
		m_state.lastHypotheses = result.rankedHypotheses;
		if(!result.completed){
			m_state.errorMessage = "session did not complete: " + result.stopInfo.dump();
		}
	}
	SetStage(result.completed ? "completed" : "stopped");
}

// -----------------------------------------------------------------------
// EnsureLlmReady
// -----------------------------------------------------------------------
bool AgentOSService::EnsureLlmReady() {
	if(m_llmReady) return true;
	if(!m_context.llamaService) return false;

	if(!m_llmLoadAttempted){
		m_llmLoadAttempted = true;
		SetStage("loading_model");

		const bool loaded = m_context.llamaService->LoadModel(m_context.modelPath);
		if(!loaded){
			if(m_context.debugLog){
				m_context.debugLog->LOG_ERROR(
					"AgentOSService: LoadModel failed: " + m_context.modelPath);
			}
			return false;
		}
	}

	auto model = m_context.llamaService->GetModel(m_context.modelPath);
	if(!model){
		if(m_context.debugLog){
			m_context.debugLog->LOG_ERROR(
				"AgentOSService: model unavailable after load: " + m_context.modelPath);
		}
		return false;
	}

	// AgentOSはプロンプトを毎回自己完結させる設計のため、Agent固有のsystem_promptは
	// 空にしておく（実際のsystemPromptはLlamaLlmBackend::Generateがユーザプロンプトへ
	// 都度結合する）。
	auto config = std::make_shared<AgentConfig>();
	config->n_ctx = 8192;
	config->max_tokens = 2048;
	config->system_prompt = "";
	// Thinkingを空欄でprefillして確定的にスキップする（/no_thinkが効かない対策。
	// 実測: Plannerが思考だけで300秒タイムアウトx2）。AgentConfig.hのコメント参照。
	config->response_prefix = "<think>\n\n</think>\n\n";
	// 推論スレッドをPコア数以下に抑え、エンジンの描画スレッドとの競合を減らす。
	config->n_threads = 6;

	m_llmAgent = m_context.llamaService->CreateAgent(model, config);
	if(!m_llmAgent){
		if(m_context.debugLog){
			m_context.debugLog->LOG_ERROR("AgentOSService: CreateAgent failed.");
		}
		return false;
	}

	m_llmBackend = std::make_unique<LlamaLlmBackend>(m_llmAgent);

	// 全LLM入出力をTranscriptへ記録するデコレータで包む。
	// 「チャットがうまくいかない」時の一次資料は生プロンプト/生出力なので、
	// ここで必ず経由させる。
	m_loggingBackend = std::make_unique<LoggingLlmBackend>(
		m_llmBackend.get(),
		[this](const std::string& systemPrompt, const std::string& userPrompt,
		       const std::string& output, const LlmGenerationStats& stats){
			WriteTranscriptEvent(
				"llm_call",
				{{"elapsedMs", std::to_string(stats.elapsedMillis)},
				 {"promptChars", std::to_string(stats.promptChars)},
				 {"completionChars", std::to_string(stats.completionChars)}},
				{{"system", systemPrompt},
				 {"user", userPrompt},
				 {"output", output}});
		});

	m_llmReady = true;
	return true;
}

// -----------------------------------------------------------------------
// PumpMainThread
// -----------------------------------------------------------------------
void AgentOSService::PumpMainThread(std::int64_t frameCounter) {
	m_dispatcher.Pump();
	if(m_tracer.IsActive()){
		m_tracer.Sample(frameCounter);
	}
}

// -----------------------------------------------------------------------
// GetSnapshot / GetAuditSnapshot / IsBusy
// -----------------------------------------------------------------------
AgentOSService::StateSnapshot AgentOSService::GetSnapshot() const {
	std::lock_guard<std::mutex> lock(m_stateMutex);
	StateSnapshot snapshot = m_state;
	snapshot.running = m_running.load();
	return snapshot;
}

Json AgentOSService::GetAuditSnapshot() const {
	Json array = Json::array();
	if(!m_pipeline) return array;

	for(const auto& [request, result] : m_pipeline->GetAuditLog()){
		Json entry = Json::object();
		entry["commandId"] = request.id;
		entry["taskId"] = request.taskId;
		entry["issuer"] = request.issuer;
		entry["tool"] = request.tool;
		entry["arguments"] = request.arguments;
		entry["dryRun"] = request.dryRun;
		entry["status"] = ToString(result.status);
		entry["error"] = result.error;
		entry["payload"] = result.payload;
		array.push_back(entry);
	}
	return array;
}

bool AgentOSService::IsBusy() const {
	return m_running.load();
}

// -----------------------------------------------------------------------
// AppendChat / SetStage
// -----------------------------------------------------------------------
void AgentOSService::AppendChat(const std::string& role, const std::string& text) {
	std::lock_guard<std::mutex> lock(m_stateMutex);
	m_state.chatLog.emplace_back(role, text);
}

void AgentOSService::SetStage(const std::string& stage) {
	std::lock_guard<std::mutex> lock(m_stateMutex);
	m_state.stage = stage;
}

// -----------------------------------------------------------------------
// Transcript
// -----------------------------------------------------------------------
void AgentOSService::OpenTranscriptForSession() {
	std::lock_guard<std::mutex> lock(m_transcriptMutex);

	if(m_transcript.is_open()){
		m_transcript.close();
	}

	std::string compact;
	MakeTimestamps(&compact, nullptr);

	std::error_code errorCode;
	std::filesystem::create_directories("Logs/AgentOS", errorCode);

	m_transcriptPath = "Logs/AgentOS/transcript_" + compact + ".yaml";
	m_transcript.open(m_transcriptPath, std::ios::out | std::ios::trunc);

	if(!m_transcript.is_open()){
		if(m_context.debugLog){
			m_context.debugLog->LOG_WARNING(
				"AgentOSService: failed to open transcript: " + m_transcriptPath);
		}
		m_transcriptPath.clear();
	}

	{
		std::lock_guard<std::mutex> stateLock(m_stateMutex);
		m_state.transcriptPath = m_transcriptPath;
	}
}

void AgentOSService::WriteTranscriptEvent(
	const std::string& kind,
	const std::vector<std::pair<std::string, std::string>>& scalarFields,
	const std::vector<std::pair<std::string, std::string>>& blockFields) {

	std::lock_guard<std::mutex> lock(m_transcriptMutex);
	if(!m_transcript.is_open()){
		return;
	}

	std::string readable;
	MakeTimestamps(nullptr, &readable);

	m_transcript << "---\n";
	m_transcript << "kind: " << kind << "\n";
	m_transcript << "time: " << readable << "\n";
	for(const auto& [key, value] : scalarFields){
		m_transcript << key << ": " << value << "\n";
	}
	for(const auto& [key, value] : blockFields){
		AppendYamlBlock(m_transcript, key, value);
	}
	// クラッシュしても直前のイベントまでは残るよう毎回flushする。
	m_transcript.flush();
}

} // namespace agentos
