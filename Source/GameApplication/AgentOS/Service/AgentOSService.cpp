// =======================================================================
//
// AgentOSService.cpp
//
// =======================================================================
#include "AgentOSService.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <initializer_list>
#include <sstream>

#include "LlamaLlmBackend.h"
#include "../Core/Agents/AgentContext.h"
#include "../Core/CodeIndex/CodeSearchTool.h"
#include "../Core/Llm/PromptTemplates.h"
#include "../Core/Logic/ComponentQueryRouting.h"
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

// 単文だけでは意味が確定しない訂正・参照・継続Turnは、Serviceの決定的Fast Pathへ
// 入れず、必ずConversation Memory付きIntakeへ送る。
bool IsContextDependentRequest(const std::string& request) {
	const std::string lower = LowerAscii(request);
	return ContainsAny(request, {
		"そうじゃなく", "そうではなく", "違う", "ちがう", "じゃなく", "ではなく",
		"それ", "その", "これ", "前の", "さっき", "先ほど", "今の",
		"続けて", "続きを", "続き", "もう一度", "やっぱり", "同じ", "代わりに"
	}) || ContainsAny(lower, {
		"not that", "instead", "previous", "continue", "keep going",
		"the same", "that one", "as before", "again"
	});
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

struct GeneratedOutputParts {
	std::string thinking;
	std::string response;
};

GeneratedOutputParts SplitGeneratedOutput(const std::string& output) {
	GeneratedOutputParts parts;
	constexpr const char* openTag = "<think>";
	constexpr const char* closeTag = "</think>";
	const std::size_t open = output.find(openTag);
	if(open == std::string::npos){
		parts.response = output;
		return parts;
	}

	const std::size_t thoughtStart = open + std::char_traits<char>::length(openTag);
	const std::size_t close = output.find(closeTag, thoughtStart);
	if(close == std::string::npos){
		parts.thinking = output.substr(thoughtStart);
		return parts;
	}

	parts.thinking = output.substr(thoughtStart, close - thoughtStart);
	parts.response = output.substr(close + std::char_traits<char>::length(closeTag));
	return parts;
}

enum class FastPathKind {
	None,
	Capabilities,
	Tool,
};

struct FastPathRoute {
	FastPathKind kind = FastPathKind::None;
	std::string tool;
	Json arguments = Json::object();
};

// Fast PathはComponentQueryRouting::Resolve()が確定的（厳密一致前提）に
// entity/componentを抽出できた場合にのみ発火させる。あいまい一致（Fuzzy Match）は
// 意図的にここへは組み込まない: Fast Pathはtool結果を機械的に整形して即応答する
// 経路であり、LLMによる根拠検証（Evidence/Critic）を経ないため、あいまいな解決を
// ここで行うと「なぜその解釈になったか」の監査証跡が残らない。
// あいまいなEntity/Component解決はResolveEntity/ResolveComponent Tool経由で
// Orchestrator（Plan/Retrieve/Evidence/Reason/Critic）を通し、根拠が監査可能な
// 形で残るようにする。
FastPathRoute ResolveFastPath(const std::string& request) {
	const componentquery::Route observationRoute = componentquery::Resolve(request);
	if(observationRoute.IsValid()) {
		FastPathRoute route;
		route.kind = FastPathKind::Tool;
		route.tool = observationRoute.tool;
		route.arguments = observationRoute.arguments;
		return route;
	}
	if(IsContextDependentRequest(request)) return {};

	const std::string lower = LowerAscii(request);

	if(ContainsAny(request, {"何ができますか", "何ができる", "できること"}) ||
	   ContainsAny(lower, {"what can you do", "capabilities"})){
		return {FastPathKind::Capabilities, "CapabilityCatalog", Json::object()};
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

Json BuildCapabilityPayload(CommandPipeline* pipeline) {
	Json payload = Json::object();
	payload["claim"] = "AgentOSで現在利用可能な能力と制約の一覧。";
	payload["tools"] = pipeline ? pipeline->DescribeTools() : Json::array();
	payload["executionPolicy"] = Json::object({
		{"routing", "C++ deterministic routing"},
		{"responseGeneration", "local LLM"},
		{"llmCommandsAreProposals", true},
		{"modifyEnabled", false},
		{"humanApprovalRequiredForModify", true},
	});
	payload["pipeline"] = Json::array({
		"Intake",
		"Plan",
		"Retrieve",
		"Evidence",
		"Reason",
		"Critic",
		"Repair",
		"Synthesize",
	});
	return payload;
}

Result GenerateFastPathReply(
	AgentContext& context,
	const std::string& userRequest,
	const std::string& sourceName,
	const Json& sourcePayload,
	std::string* reportOut
) {
	if(reportOut == nullptr){
		return Result::Fail("GenerateFastPathReply: reportOut is null");
	}

	const PromptPair prompt = prompts::FormatToolResult(
		userRequest,
		sourceName,
		sourcePayload
	);

	Json generated;
	Result generationResult = CallLlmJson(context, prompt, &generated);
	if(!generationResult){
		return Result::Fail(
			"fast-path response generation failed: " + generationResult.error);
	}

	if(!generated.is_object() ||
	   !generated.contains("reply") ||
	   !generated.at("reply").is_string() ||
	   generated.at("reply").get<std::string>().empty()){
		return Result::Fail(
			"fast-path response generation returned no non-empty reply");
	}

	*reportOut = generated.at("reply").get<std::string>();
	return Result::Ok();
}

} // namespace

AgentOSService::AgentOSService() = default;
AgentOSService::~AgentOSService() = default;

void AgentOSService::Initialize(AgentOSServiceContext context) {
	m_context = std::move(context);
	m_shutdownRequested.store(false, std::memory_order_release);
	m_llmLoadState.store(LlmLoadState::Unloaded, std::memory_order_release);
	m_cancelRequested.store(false, std::memory_order_release);
	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		const std::filesystem::path modelPath(m_context.modelPath);
		m_state.modelName = modelPath.filename().string();
		if(m_state.modelName.empty()) m_state.modelName = "Local model";
	}

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

	// --- コード索引（RAG下層） ---
	// EngineToolsが「実行中のシーンを引く手」なのに対して、
	// これは「ソースコードを引く手」。同じCommandPipelineに載せることで、
	// Orchestratorからは他のretrievalツールと区別なく扱える。
	{
		// 索引DBもTaskStoreと同じくディレクトリを用意しておく
		const std::filesystem::path indexPath(m_context.codeIndexDbPath);
		if(indexPath.has_parent_path()){
			std::error_code indexErrorCode;
			std::filesystem::create_directories(indexPath.parent_path(), indexErrorCode);
		}

		CodeIndexServiceContext indexContext;
		indexContext.sourceRoot = m_context.codeIndexRoot;
		indexContext.databasePath = m_context.codeIndexDbPath;
		indexContext.buildOnStart = m_context.buildCodeIndexOnStart;
		// 埋め込みは未接続。この状態でも字句検索は成立する。
		// LlamaEmbeddingBackend 接続後に SetCodeIndexEmbedding() で差し込む。
		indexContext.embedding = nullptr;

		const Result indexResult = m_codeIndex.Initialize(std::move(indexContext));
		if(!indexResult){
			// 索引が無くてもAgentOS本体は動くべきなので、失敗しても止めない。
			if(m_context.debugLog){
				m_context.debugLog->LOG_ERROR(
					"AgentOSService: CodeIndexService::Initialize failed: " + indexResult.error);
			}
		} else {
			RegisterCodeSearchTool(*m_pipeline, m_codeIndex);
			if(m_context.debugLog){
				m_context.debugLog->LOG_INFO(
					"AgentOSService: CodeSearch / GetSymbolInfo tools registered (index: "
					+ m_context.codeIndexDbPath + ").");
			}
		}
	}

	if(m_context.debugLog){
		m_context.debugLog->LOG_INFO("AgentOSService: initialized.");
	}
}

void AgentOSService::Shutdown() {
	m_shutdownRequested.store(true, std::memory_order_release);
	SetStage("shutting_down");

	m_dispatcher.CancelPending();
	{
		std::lock_guard<std::mutex> backendLock(m_backendMutex);
		if(m_llmBackend) m_llmBackend->Cancel();
		if(m_llmAgent) m_llmAgent->Stop();
	}

	if(m_worker.joinable()){
		m_worker.join();
	}

	// pipelineがSearchCodeツール経由でm_codeIndexを参照しているため、
	// 索引スレッドを畳むのはpipelineを壊す前でなければならない。
	m_codeIndex.Shutdown();

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

// =======================================================================
// 推論バックエンド（CPU / GPU）
// =======================================================================
bool AgentOSService::IsGpuBackendAvailable() const {
	if(!m_context.llamaService) return false;
	return m_context.llamaService->HasGpuBackend();
}

std::vector<std::string> AgentOSService::GetBackendSummary() const {
	std::vector<std::string> summary;
	if(!m_context.llamaService) return summary;

	const auto& info = m_context.llamaService->GetBackendInfo();

	summary.push_back(std::string("GPU offload: ") +
		(info.gpuOffloadSupported ? "supported" : "not available"));
	summary.push_back("Max devices: " + std::to_string(info.maxDevices));

	// llama_print_system_info() は "AVX = 1 | AVX2 = 1 | ..." のように
	// パイプ区切りで返る。UIで読みやすいよう1項目ずつに割る。
	if(!info.systemInfo.empty()){
		std::string field;
		std::istringstream stream(info.systemInfo);
		while(std::getline(stream, field, '|')){
			// 前後の空白を落とす
			const std::size_t begin = field.find_first_not_of(" \t\r\n");
			if(begin == std::string::npos) continue;
			const std::size_t end = field.find_last_not_of(" \t\r\n");
			summary.push_back(field.substr(begin, end - begin + 1));
		}
	}
	return summary;
}

bool AgentOSService::SetGpuLayers(int gpuLayers) {
	// モデルの再ロードを伴うため、生成中は受け付けない。
	// 進行中のセッションが使っているllama_model*を破棄することになる。
	if(IsBusy()) return false;
	if(!m_context.llamaService) return false;

	const int previous = m_gpuLayers.exchange(gpuLayers, std::memory_order_acq_rel);
	if(previous == gpuLayers) return true; // 変更なし

	if(m_context.debugLog){
		m_context.debugLog->LOG_INFO(
			"AgentOSService: GPU layers " + std::to_string(previous) +
			" -> " + std::to_string(gpuLayers) + " (model will be reloaded)");
	}

	// 次回の生成時に作り直させる。ここで即座に再ロードすると
	// UIスレッドが数GBのロードでブロックするため、状態だけ落とす。
	{
		std::lock_guard<std::mutex> backendLock(m_backendMutex);
		m_orchestrator.reset();
		m_loggingBackend.reset();
		m_llmBackend.reset();
		m_llmAgent.reset();
	}
	m_llmLoadState.store(LlmLoadState::Unloaded, std::memory_order_release);

	// 旧設定のモデルを解放する。ResourceLoaderのキャッシュキーには
	// 引数が含まれるため、明示的に落とさないと両設定分のメモリを抱える。
	m_context.llamaService->ReloadModel(m_context.modelPath, gpuLayers);
	return true;
}

void AgentOSService::CancelCurrentRequest() {
	if(!IsBusy()) return;
	m_cancelRequested.store(true, std::memory_order_release);
	SetStage("cancelling");
	AppendProcessEvent("Cancellation requested");
	std::lock_guard<std::mutex> backendLock(m_backendMutex);
	if(m_llmBackend) m_llmBackend->Cancel();
	if(m_llmAgent) m_llmAgent->Stop();
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
	{
		std::lock_guard<std::mutex> backendLock(m_backendMutex);
		if(m_llmBackend) m_llmBackend->ResetCancellation();
	}
	m_cancelRequested.store(false, std::memory_order_release);

	m_running.store(true, std::memory_order_release);
	AppendChat("user", text);
	SetStage("starting");
	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		m_state.errorMessage.clear();
		m_state.progressDetail = Json::object();
		m_state.generationActive = false;
		m_state.liveThinking.clear();
		m_state.liveResponse.clear();
		m_state.sessionProcessLog.clear();
		m_state.sessionElapsedMillis = 0;
		m_state.liveElapsedMillis = 0;
		m_state.livePromptTokens = 0;
		m_state.liveCompletionTokens = 0;
		m_state.sessionPromptTokens = 0;
		m_state.sessionCompletionTokens = 0;
		m_state.tokensPerSecond = 0.0;
		m_sessionStartedAt = std::chrono::steady_clock::now();
	}
	AppendProcessEvent("Request accepted");

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

	if(!EnsureLlmReady()){
		const bool cancelled = m_shutdownRequested.load(std::memory_order_acquire);
		SetStage(cancelled ? "cancelled" : "error");
		std::lock_guard<std::mutex> lock(m_stateMutex);
		m_state.errorMessage = cancelled
			? "AgentOS session cancelled during shutdown."
			: ("LLM model failed to load: " + m_context.modelPath);
		return;
	}
	if(m_cancelRequested.load(std::memory_order_acquire)){
		std::lock_guard<std::mutex> backendLock(m_backendMutex);
		if(m_llmBackend) m_llmBackend->Cancel();
		SetStage("cancelled");
		return;
	}

	if(m_shutdownRequested.load(std::memory_order_acquire)){
		SetStage("cancelled");
		return;
	}

	if(TryRunDeterministicFastPath(request)) return;

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
	if(result.sessionId != kInvalidId && !result.report.empty()){
		(void)m_taskStore.SetConversationResponse(result.sessionId, result.report);
	}

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
	SetStage(m_cancelRequested.load(std::memory_order_acquire)
		? "cancelled"
		: (result.completed ? "completed" : "stopped"));
}

bool AgentOSService::TryRunDeterministicFastPath(const std::string& request) {
	const FastPathRoute route = ResolveFastPath(request);
	if(route.kind == FastPathKind::None) return false;

	SetStage("direct_route");

	const SessionId sessionId = m_taskStore.CreateSession(
		Json::object({
			{"userRequest", request},
			{"route", "deterministic_route_with_llm_generation"},
		}));
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
		? "DirectReadToolDeterministic"
		: "CapabilityReplyWithGeneration";
	const TaskId taskId = m_taskStore.CreateTask(
		sessionId,
		kInvalidId,
		taskType,
		Json::object({
			{"request", request},
			{"source", route.tool},
			{"arguments", route.arguments},
		}),
		0
	);
	m_taskStore.UpdateTaskState(taskId, TaskState::Running);

	Json sourcePayload;
	bool sourceSucceeded = true;
	std::string sourceError;

	CapabilityToken token;
	if(route.kind == FastPathKind::Capabilities){
		sourcePayload = BuildCapabilityPayload(m_pipeline.get());
	} else {
		token = m_capabilityRegistry.IssueToken(
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
			: CommandResult::Fail(
				CommandStatus::ExecutionFailed,
				"command pipeline unavailable");

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
			sourcePayload = result.payload;
			if(!componentquery::PayloadSatisfied(sourcePayload)) {
				sourceSucceeded = false;
				sourceError = sourcePayload.value("error", std::string("requested observation was not satisfied"));
			}
		} else {
			sourceSucceeded = false;
			sourceError = result.error;
			sourcePayload = Json::object({
				{"claim", "要求されたToolの実行に失敗した。"},
				{"tool", route.tool},
				{"arguments", route.arguments},
				{"status", ToString(result.status)},
				{"error", result.error},
			});
		}
	}

	SetStage("generate_reply");

	Budget fastBudget;
	fastBudget.maxToolCalls = 1;
	fastBudget.maxLlmCalls = 2;
	fastBudget.maxRetries = 1;
	fastBudget.maxDepth = 1;
	fastBudget.maxLlmChars = 120000;
	fastBudget.maxMillis = 300000;
	BudgetTracker budgetTracker(fastBudget);

	AgentContext generationContext;
	generationContext.llm = m_loggingBackend.get();
	generationContext.pipeline = m_pipeline.get();
	generationContext.store = &m_taskStore;
	generationContext.budget = &budgetTracker;
	generationContext.token = token;
	generationContext.sessionId = sessionId;

	std::string report;
	Result generationResult;
	if(route.kind == FastPathKind::Tool) {
		componentquery::Route observationRoute{route.tool, route.arguments};
		report = componentquery::BuildReply(observationRoute, sourcePayload);
		generationResult = report.empty()
			? Result::Fail("deterministic tool response formatter returned no reply")
			: Result::Ok();
	} else {
		generationResult = GenerateFastPathReply(
			generationContext,
			request,
			route.tool,
			sourcePayload,
			&report
		);
	}

	const bool completed = static_cast<bool>(generationResult);
	Json stopInfo;
	if(completed){
		stopInfo = Json::object({
			{"reason", "deterministic route completed with local LLM generation"},
			{"sourceSucceeded", sourceSucceeded},
		});
		m_taskStore.SetTaskResult(
			taskId,
			Json::object({
				{"source", sourcePayload},
				{"reply", report},
				{"generatedBy", route.kind == FastPathKind::Tool
					? "deterministic_formatter"
					: "local_llm"},
			})
		);
		m_taskStore.UpdateTaskState(
			taskId,
			sourceSucceeded ? TaskState::Succeeded : TaskState::Failed);
	} else {
		report = "Fast Pathの最終応答生成に失敗した: " + generationResult.error;
		if(!sourceError.empty()){
			report += " / Tool error: " + sourceError;
		}
		stopInfo = Json::object({
			{"reason", generationResult.error},
			{"sourceSucceeded", sourceSucceeded},
		});
		m_taskStore.SetTaskResult(
			taskId,
			Json::object({
				{"source", sourcePayload},
				{"generationError", generationResult.error},
			})
		);
		m_taskStore.UpdateTaskState(taskId, TaskState::Failed);
	}

	(void)m_taskStore.SetConversationResponse(sessionId, report);
	m_taskStore.UpdateSessionState(sessionId, completed ? "Completed" : "Stopped");

	WriteTranscriptEvent(
		"result",
		{{"completed", completed ? "true" : "false"},
		 {"sessionId", std::to_string(sessionId)},
		 {"route", "deterministic_route_with_llm_generation"},
		 {"responseGeneratedBy", route.kind == FastPathKind::Tool
			 ? "deterministic_formatter"
			 : "local_llm"}},
		{{"sourcePayload", TruncateForLog(sourcePayload.dump(2))},
		 {"report", report},
		 {"stopInfo", stopInfo.dump(2)}}
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

	const bool loaded = m_context.llamaService->LoadModel(
		m_context.modelPath, m_gpuLayers.load(std::memory_order_acquire));
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
	// 固定値6をやめ、実機のコア数から決める。
	// ゲームエンジン内で動くため描画/JobSystem用に物理コアを2つ残す。
	config->n_threads = AgentConfig::RecommendedThreads(2);

	auto llmAgent = m_context.llamaService->CreateAgent(model, config);
	if(!llmAgent){
		m_llmLoadState.store(LlmLoadState::RetryableFailure, std::memory_order_release);
		if(m_context.debugLog){
			m_context.debugLog->LOG_ERROR("AgentOSService: CreateAgent failed.");
		}
		return false;
	}

	auto llmBackend = std::make_unique<LlamaLlmBackend>(llmAgent);
	llmBackend->ResetCancellation();
	llmBackend->SetStreamCallback(
		[this](const std::string& output, std::int64_t elapsedMillis,
		       std::int64_t promptTokens, std::int64_t completionTokens){
			const GeneratedOutputParts parts = SplitGeneratedOutput(output);
			std::lock_guard<std::mutex> lock(m_stateMutex);
			m_state.generationActive = true;
			m_state.liveThinking = parts.thinking;
			m_state.liveResponse = parts.response;
			m_state.liveElapsedMillis = elapsedMillis;
			m_state.livePromptTokens = promptTokens;
			m_state.liveCompletionTokens = completionTokens;
			m_state.tokensPerSecond = elapsedMillis > 0
				? (static_cast<double>(completionTokens) * 1000.0 /
				   static_cast<double>(elapsedMillis))
				: 0.0;
		}
	);

	auto loggingBackend = std::make_unique<LoggingLlmBackend>(
		llmBackend.get(),
		[this](const std::string& systemPrompt, const std::string& userPrompt,
		       const std::string& output, const LlmGenerationStats& stats){
			const GeneratedOutputParts parts = SplitGeneratedOutput(output);
			{
				std::lock_guard<std::mutex> lock(m_stateMutex);
				m_state.generationActive = false;
				m_state.liveThinking = parts.thinking;
				m_state.liveResponse = parts.response;
				m_state.liveElapsedMillis = stats.elapsedMillis;
				m_state.livePromptTokens = stats.promptTokens;
				m_state.liveCompletionTokens = stats.completionTokens;
				m_state.totalPromptTokens += stats.promptTokens;
				m_state.totalCompletionTokens += stats.completionTokens;
				m_state.sessionPromptTokens += stats.promptTokens;
				m_state.sessionCompletionTokens += stats.completionTokens;
				m_state.tokensPerSecond = stats.elapsedMillis > 0
					? (static_cast<double>(stats.completionTokens) * 1000.0 /
					   static_cast<double>(stats.elapsedMillis))
					: 0.0;
				if(!parts.thinking.empty()){
					if(!m_state.sessionProcessLog.empty()) m_state.sessionProcessLog += "\n\n";
					m_state.sessionProcessLog += parts.thinking;
				}
			}
			WriteTranscriptEvent(
				"llm_call",
				{{"elapsedMs", std::to_string(stats.elapsedMillis)},
				 {"promptTokens", std::to_string(stats.promptTokens)},
				 {"completionTokens", std::to_string(stats.completionTokens)},
				 {"promptChars", std::to_string(stats.promptChars)},
				 {"completionChars", std::to_string(stats.completionChars)},
				 {"stopReason", stats.stopReason}},
				{{"system", systemPrompt},
				 {"user", userPrompt},
				 {"output", output}}
			);
		}
	);
	{
		std::lock_guard<std::mutex> backendLock(m_backendMutex);
		m_llmAgent = std::move(llmAgent);
		m_llmBackend = std::move(llmBackend);
		m_loggingBackend = std::move(loggingBackend);
	}

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
	if(snapshot.running && m_sessionStartedAt.time_since_epoch().count() != 0){
		snapshot.sessionElapsedMillis =
			std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now() - m_sessionStartedAt).count();
	}
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
	ChatEntry entry;
	entry.role = role;
	entry.text = text;
	if(role == "assistant"){
		entry.processLog = m_state.sessionProcessLog;
		entry.elapsedMillis = m_state.sessionElapsedMillis;
		if(m_sessionStartedAt.time_since_epoch().count() != 0){
			entry.elapsedMillis =
				std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::steady_clock::now() - m_sessionStartedAt).count();
		}
		entry.promptTokens = m_state.sessionPromptTokens;
		entry.completionTokens = m_state.sessionCompletionTokens;
		m_state.sessionElapsedMillis = entry.elapsedMillis;
		m_state.generationActive = false;
	}
	m_state.chatLog.push_back(std::move(entry));
}

void AgentOSService::SetStage(const std::string& stage) {
	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		m_state.stage = stage;
	}
	AppendProcessEvent(stage);
}

void AgentOSService::AppendProcessEvent(const std::string& event) {
	if(event.empty()) return;
	std::lock_guard<std::mutex> lock(m_stateMutex);
	const std::string line = "[" + event + "]";
	if(m_state.sessionProcessLog.size() >= line.size() &&
	   m_state.sessionProcessLog.compare(
		   m_state.sessionProcessLog.size() - line.size(), line.size(), line) == 0){
		return;
	}
	if(!m_state.sessionProcessLog.empty()) m_state.sessionProcessLog += '\n';
	m_state.sessionProcessLog += line;
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
			{{"transcriptVersion", "4"},
			 {"buildRevision", "unknown"},
			 {"modelPath", m_context.modelPath},
			 {"dbPath", m_context.dbPath},
			 {"modelFingerprint", ModelFingerprint(m_context.modelPath)},
			 {"fastPathResponsePolicy", "context-dependent turns bypass deterministic fast path"},
			 {"conversationMemoryPolicy", "all raw turns retained; old turns cumulatively summarized"}},
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
