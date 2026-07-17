// =======================================================================
//
// AgentOSService.cpp
//
// =======================================================================
#include "AgentOSService.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <initializer_list>
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

void AppendYamlBlock(std::ostream& out, const std::string& key, const std::string& text) {
	out << key << ": |\n";
	std::istringstream stream(text);
	std::string line;
	while(std::getline(stream, line)){
		if(!line.empty() && line.back() == '\r') line.pop_back();
		out << "  " << line << "\n";
	}
}

std::string LowerAscii(std::string text) {
	std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c){
		return static_cast<char>(std::tolower(c));
	});
	return text;
}

bool ContainsAny(const std::string& text, std::initializer_list<const char*> needles) {
	for(const char* needle : needles){
		if(needle && text.find(needle) != std::string::npos) return true;
	}
	return false;
}

std::string TruncateForLog(const std::string& text, std::size_t maxChars = 16000) {
	if(text.size() <= maxChars) return text;
	return text.substr(0, maxChars) + "\n...(truncated)...";
}

std::string ModelFingerprint(const std::string& path) {
	std::error_code ec;
	const std::filesystem::path modelPath(path);
	if(!std::filesystem::exists(modelPath, ec) || ec) return "unavailable";

	const auto size = std::filesystem::file_size(modelPath, ec);
	if(ec) return "unavailable";
	const auto writeTime = std::filesystem::last_write_time(modelPath, ec);
	if(ec) return "size=" + std::to_string(size);

	return "size=" + std::to_string(size) +
		";mtimeTicks=" + std::to_string(writeTime.time_since_epoch().count());
}

enum class FastPathKind {
	None,
	StaticCapabilities,
	Tool,
};

struct FastPathRoute {
	FastPathKind kind = FastPathKind::None;
	std::string tool;
	Json arguments = Json::object();
};

FastPathRoute ResolveFastPath(const std::string& request) {
	const std::string lower = LowerAscii(request);

	if(ContainsAny(request, {"何ができますか", "何ができる", "できること"}) ||
	   ContainsAny(lower, {"what can you do", "capabilities"})){
		return {FastPathKind::StaticCapabilities, {}, Json::object()};
	}

	const bool asksList = ContainsAny(request, {"一覧", "列挙", "全部"}) ||
		ContainsAny(lower, {"list", "show all"});
	const bool asksEntities = ContainsAny(request, {"エンティティ"}) ||
		lower.find("entity") != std::string::npos;
	if(asksList && asksEntities){
		FastPathRoute route;
		route.kind = FastPathKind::Tool;
		route.tool = "ListEntities";
		route.arguments = Json::object({{"maxCount", 100}});
		return route;
	}

	const bool asksSystems = ContainsAny(request, {"システム"}) ||
		lower.find("system") != std::string::npos;
	if(asksList && asksSystems){
		FastPathRoute route;
		route.kind = FastPathKind::Tool;
		route.tool = "ListSystems";
		return route;
	}

	return {};
}

std::string CapabilitiesReply() {
	return
		"AgentOSは、現在のエンジン状態を読み取り専用で調査できる。\n\n"
		"- アクティブSceneのEntity一覧とEntity詳細の取得\n"
		"- Component値の読み取り\n"
		"- SystemTask、Read/Write宣言、依存関係の確認\n"
		"- Componentのフレーム間WriteTrace\n"
		"- 調査結果のEvidence化、仮説生成、Critic検証、監査ログ保存\n\n"
		"現段階では、Sceneやコードの自動変更は実行しない。Modify系はHuman Approval、"
		"Compile/Test/Rollbackを接続してから有効化する。";
}

std::string FormatToolResult(const std::string& tool, const Json& payload) {
	if(tool == "ListEntities"){
		const Json entities = payload.value("entities", Json::array());
		std::ostringstream out;
		out << "アクティブSceneのEntityを" << entities.size() << "件取得した。\n";
		for(const Json& entity : entities){
			out << "- id=" << entity.value("id", 0u)
				<< " generation=" << entity.value("generation", 0u);
			const std::string name = entity.value("name", std::string());
			if(!name.empty()) out << " name=\"" << name << "\"";
			out << "\n";
		}
		return out.str();
	}

	if(tool == "ListSystems"){
		const Json tasks = payload.value("tasks", Json::array());
		std::ostringstream out;
		out << "登録済みSystemTaskは" << payload.value("taskCount", tasks.size()) << "件。\n";
		for(const Json& task : tasks){
			out << "- " << task.value("name", std::string("(unnamed)"));
			const std::string domain = task.value("domain", std::string());
			const std::string phase = task.value("phase", std::string());
			if(!domain.empty()) out << " [" << domain;
			if(!phase.empty()) out << "/" << phase;
			if(!domain.empty()) out << "]";
			out << "\n";
		}
		return out.str();
	}

	return "Tool " + tool + " の実行結果:\n" + payload.dump(2);
}

} // namespace

AgentOSService::AgentOSService() = default;
AgentOSService::~AgentOSService() = default;

void AgentOSService::Initialize(AgentOSServiceContext context) {
	m_context = std::move(context);
	m_shutdownRequested.store(false, std::memory_order_release);
	m_llmLoadState.store(LlmLoadState::Unloaded, std::memory_order_release);

	m_engineToolContext.sceneManager = m_context.sceneManager;
	m_engineToolContext.debugLog = m_context.debugLog;

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

void AgentOSService::Shutdown() {
	m_shutdownRequested.store(true, std::memory_order_release);
	SetStage("shutting_down");

	// 待機中のMainThread ToolとLLM生成を先に中断してからjoinする。
	m_dispatcher.CancelPending();
	if(m_llmBackend) m_llmBackend->Cancel();
	if(m_llmAgent) m_llmAgent->Stop();

	if(m_worker.joinable()){
		m_worker.join();
	}

	m_orchestrator.reset();
	m_loggingBackend.reset();
	m_llmBackend.reset();
	m_llmAgent.reset();
	m_pipeline.reset();
	m_llmLoadState.store(LlmLoadState::Unloaded, std::memory_order_release);

	{
		std::lock_guard<std::mutex> lock(m_transcriptMutex);
		if(m_transcript.is_open()) m_transcript.close();
	}
}

void AgentOSService::SubmitRequest(const std::string& text) {
	if(m_shutdownRequested.load(std::memory_order_acquire)){
		if(m_context.debugLog){
			m_context.debugLog->LOG_WARNING(
				"AgentOSService: request rejected during shutdown.");
		}
		return;
	}
	if(IsBusy()){
		if(m_context.debugLog){
			m_context.debugLog->LOG_WARNING(
				"AgentOSService: request rejected, a session is already running.");
		}
		return;
	}

	if(m_worker.joinable()) m_worker.join();
	if(m_llmBackend) m_llmBackend->ResetCancellation();

	m_running.store(true, std::memory_order_release);
	AppendChat("user", text);
	SetStage("starting");
	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		m_state.errorMessage.clear();
		m_state.progressDetail = Json::object();
	}

	OpenTranscriptForSession();
	WriteTranscriptEvent("user_request", {}, {{"text", text}});

	m_worker = std::thread(&AgentOSService::WorkerMain, this, text);
}

void AgentOSService::WorkerMain(std::string request) {
	struct RunningGuard {
		std::atomic<bool>& flag;
		~RunningGuard() { flag.store(false, std::memory_order_release); }
	} guard{m_running};

	if(m_shutdownRequested.load(std::memory_order_acquire)){
		SetStage("cancelled");
		return;
	}

	// 明白な能力質問と単一Read Tool要求はLLMを介さず決定的に処理する。
	// 実ログの「Entity一覧だけで約11分・9 LLM calls」を防ぐ。
	if(TryRunDeterministicFastPath(request)) return;

	if(!EnsureLlmReady()){
		const bool cancelled = m_shutdownRequested.load(std::memory_order_acquire);
		SetStage(cancelled ? "cancelled" : "error");
		std::lock_guard<std::mutex> lock(m_stateMutex);
		m_state.errorMessage = cancelled
			? "AgentOS session cancelled during shutdown."
			: ("LLM model failed to load: " + m_context.modelPath);
		return;
	}

	if(m_shutdownRequested.load(std::memory_order_acquire)){
		SetStage("cancelled");
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
					{{"detail", detail.dump(2)}}
				);
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
		 {"rankedHypotheses", result.rankedHypotheses.dump(2)}}
	);

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

bool AgentOSService::TryRunDeterministicFastPath(const std::string& request) {
	const FastPathRoute route = ResolveFastPath(request);
	if(route.kind == FastPathKind::None) return false;

	SetStage("direct");

	const SessionId sessionId = m_taskStore.CreateSession(
		Json::object({{"userRequest", request}, {"route", "deterministic_fast_path"}}));
	if(sessionId == kInvalidId){
		const std::string report = "決定的Fast PathのSession作成に失敗した。";
		AppendChat("assistant", report);
		{
			std::lock_guard<std::mutex> lock(m_stateMutex);
			m_state.lastReport = report;
			m_state.errorMessage = report;
		}
		SetStage("error");
		return true;
	}
	m_taskStore.UpdateSessionState(sessionId, "Running");

	const std::string taskType = route.kind == FastPathKind::Tool
		? "DirectReadTool"
		: "DirectReply";
	const TaskId taskId = m_taskStore.CreateTask(
		sessionId,
		kInvalidId,
		taskType,
		Json::object({{"request", request}, {"tool", route.tool}}),
		0
	);
	m_taskStore.UpdateTaskState(taskId, TaskState::Running);

	std::string report;
	bool completed = true;
	Json stopInfo = Json::object({{"reason", "deterministic fast path"}});

	if(route.kind == FastPathKind::StaticCapabilities){
		report = CapabilitiesReply();
		m_taskStore.SetTaskResult(taskId, Json::object({{"reply", report}}));
		m_taskStore.UpdateTaskState(taskId, TaskState::Succeeded);
	} else {
		const CapabilityToken token = m_capabilityRegistry.IssueToken(
			"DeterministicFastPath",
			{route.tool},
			PermissionLevel::Read
		);

		CommandRequest command;
		command.taskId = taskId;
		command.issuer = "DeterministicFastPath";
		command.tool = route.tool;
		command.arguments = route.arguments;
		command.capability = token;

		const bool hadAuditSink = m_pipeline && m_pipeline->HasAuditSinks();
		const CommandResult result = m_pipeline
			? m_pipeline->Submit(command)
			: CommandResult::Fail(CommandStatus::ExecutionFailed, "command pipeline unavailable");

		// 最初のSessionがFast Pathの場合はOrchestratorのSQLite AuditSinkがまだ無い。
		if(!hadAuditSink){
			const std::string status = ToString(result.status);
			const Json storedResult = result.IsOk()
				? result.payload
				: Json::object({{"error", result.error}});
			m_taskStore.RecordCommand(
				taskId,
				command.issuer,
				command.tool,
				command.arguments,
				status,
				status,
				storedResult
			);
		}

		m_capabilityRegistry.Revoke(token);

		WriteTranscriptEvent(
			"command",
			{{"tool", route.tool}, {"status", ToString(result.status)}},
			{{"arguments", route.arguments.dump(2)},
			 {"payload", TruncateForLog(result.payload.dump(2))},
			 {"error", result.error}}
		);

		if(result.IsOk()){
			report = FormatToolResult(route.tool, result.payload);
			m_taskStore.SetTaskResult(taskId, result.payload);
			m_taskStore.UpdateTaskState(taskId, TaskState::Succeeded);
		} else {
			completed = false;
			report = "Tool " + route.tool + " の実行に失敗した: " + result.error;
			stopInfo = Json::object({{"reason", "deterministic tool failed: " + result.error}});
			m_taskStore.SetTaskResult(taskId, Json::object({{"error", result.error}}));
			m_taskStore.UpdateTaskState(taskId, TaskState::Failed);
		}
	}

	m_taskStore.UpdateSessionState(sessionId, completed ? "Completed" : "Stopped");

	WriteTranscriptEvent(
		"result",
		{{"completed", completed ? "true" : "false"},
		 {"sessionId", std::to_string(sessionId)},
		 {"route", "deterministic_fast_path"}},
		{{"report", report}, {"stopInfo", stopInfo.dump(2)}}
	);

	AppendChat("assistant", report);
	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		m_state.lastReport = report;
		m_state.lastHypotheses = Json::object();
		if(!completed) m_state.errorMessage = report;
	}
	SetStage(completed ? "completed" : "stopped");
	return true;
}

bool AgentOSService::EnsureLlmReady() {
	if(m_llmLoadState.load(std::memory_order_acquire) == LlmLoadState::Ready) return true;
	if(m_shutdownRequested.load(std::memory_order_acquire)) return false;
	if(!m_context.llamaService) return false;

	m_llmLoadState.store(LlmLoadState::Loading, std::memory_order_release);
	SetStage("loading_model");

	const bool loaded = m_context.llamaService->LoadModel(m_context.modelPath);
	if(!loaded){
		m_llmLoadState.store(LlmLoadState::RetryableFailure, std::memory_order_release);
		if(m_context.debugLog){
			m_context.debugLog->LOG_ERROR(
				"AgentOSService: LoadModel failed: " + m_context.modelPath);
		}
		return false;
	}

	auto model = m_context.llamaService->GetModel(m_context.modelPath);
	if(!model){
		m_llmLoadState.store(LlmLoadState::RetryableFailure, std::memory_order_release);
		if(m_context.debugLog){
			m_context.debugLog->LOG_ERROR(
				"AgentOSService: model unavailable after load: " + m_context.modelPath);
		}
		return false;
	}

	auto config = std::make_shared<AgentConfig>();
	config->n_ctx = 8192;
	config->max_tokens = 2048;
	config->system_prompt = "";
	config->response_prefix = "<think>\n\n</think>\n\n";
	config->n_threads = 6;

	m_llmAgent = m_context.llamaService->CreateAgent(model, config);
	if(!m_llmAgent){
		m_llmLoadState.store(LlmLoadState::RetryableFailure, std::memory_order_release);
		if(m_context.debugLog){
			m_context.debugLog->LOG_ERROR("AgentOSService: CreateAgent failed.");
		}
		return false;
	}

	m_llmBackend = std::make_unique<LlamaLlmBackend>(m_llmAgent);
	m_llmBackend->ResetCancellation();

	m_loggingBackend = std::make_unique<LoggingLlmBackend>(
		m_llmBackend.get(),
		[this](const std::string& systemPrompt, const std::string& userPrompt,
		       const std::string& output, const LlmGenerationStats& stats){
			WriteTranscriptEvent(
				"llm_call",
				{{"elapsedMs", std::to_string(stats.elapsedMillis)},
				 {"promptChars", std::to_string(stats.promptChars)},
				 {"completionChars", std::to_string(stats.completionChars)},
				 {"stopReason", stats.stopReason}},
				{{"system", systemPrompt},
				 {"user", userPrompt},
				 {"output", output}}
			);
		}
	);

	m_llmLoadState.store(LlmLoadState::Ready, std::memory_order_release);
	return true;
}

void AgentOSService::PumpMainThread(std::int64_t frameCounter) {
	m_dispatcher.Pump();
	if(m_tracer.IsActive()) m_tracer.Sample(frameCounter);
}

AgentOSService::StateSnapshot AgentOSService::GetSnapshot() const {
	std::lock_guard<std::mutex> lock(m_stateMutex);
	StateSnapshot snapshot = m_state;
	snapshot.running = m_running.load(std::memory_order_acquire);
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
	return m_running.load(std::memory_order_acquire);
}

void AgentOSService::AppendChat(const std::string& role, const std::string& text) {
	std::lock_guard<std::mutex> lock(m_stateMutex);
	m_state.chatLog.emplace_back(role, text);
}

void AgentOSService::SetStage(const std::string& stage) {
	std::lock_guard<std::mutex> lock(m_stateMutex);
	m_state.stage = stage;
}

void AgentOSService::OpenTranscriptForSession() {
	std::string path;
	{
		std::lock_guard<std::mutex> lock(m_transcriptMutex);

		if(m_transcript.is_open()) m_transcript.close();

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
		path = m_transcriptPath;
	}

	{
		std::lock_guard<std::mutex> stateLock(m_stateMutex);
		m_state.transcriptPath = path;
	}

	if(!path.empty()){
		WriteTranscriptEvent(
			"session_context",
			{{"transcriptVersion", "2"},
			 {"buildRevision", "unknown"},
			 {"modelPath", m_context.modelPath},
			 {"dbPath", m_context.dbPath},
			 {"modelFingerprint", ModelFingerprint(m_context.modelPath)}},
			{}
		);
	}
}

void AgentOSService::WriteTranscriptEvent(
	const std::string& kind,
	const std::vector<std::pair<std::string, std::string>>& scalarFields,
	const std::vector<std::pair<std::string, std::string>>& blockFields) {

	std::lock_guard<std::mutex> lock(m_transcriptMutex);
	if(!m_transcript.is_open()) return;

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
	m_transcript.flush();
}

} // namespace agentos
