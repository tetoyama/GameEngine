from pathlib import Path
from textwrap import dedent


def read(path):
    return Path(path).read_text(encoding='utf-8')


def write(path, text):
    Path(path).write_text(text, encoding='utf-8')


def replace_once(text, old, new, label):
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f'{label}: expected one occurrence, got {count}')
    return text.replace(old, new, 1)


path = 'Source/GameApplication/AgentOS/Core/Orchestrator/Orchestrator.cpp'
text = read(path)
block = r'''
\t// --- Conversation Reference Resolution mini-plan ---
\tif (NeedsConversationReferenceResolution(intake)) {
\t\tconstexpr int kMaxConversationRetrievalAttempts = 2;
\t\tstd::unordered_set<std::string> attemptedQueries;
\t\tstd::string query = userRequest;
\t\tbool referenceResolved = false;
\t\tstd::string resolutionFailure;

\t\tfor (int attempt = 0; attempt < kMaxConversationRetrievalAttempts; ++attempt) {
\t\t\tif (query.empty() || attemptedQueries.count(query) != 0) {
\t\t\t\tresolutionFailure = "同じConversation検索を繰り返さないため停止";
\t\t\t\tbreak;
\t\t\t}
\t\t\tattemptedQueries.insert(query);

\t\t\tJson taskSpec = Json::object({
\t\t\t\t{"taskId", "conversation_reference_" + std::to_string(attempt)},
\t\t\t\t{"type", "ConversationRetrieval"},
\t\t\t\t{"description", "現在入力の未解決な会話参照を構造化Thread Stateから解決する"},
\t\t\t\t{"dependencies", Json::array()},
\t\t\t\t{"allowedTools", Json::array({"SearchConversationContext"})},
\t\t\t\t{"commands", Json::array({Json::object({
\t\t\t\t\t{"tool", "SearchConversationContext"},
\t\t\t\t\t{"arguments", Json::object({
\t\t\t\t\t\t{"query", query},
\t\t\t\t\t\t{"beforeSessionId", sessionId},
\t\t\t\t\t\t{"limit", 5},
\t\t\t\t\t})},
\t\t\t\t})})},
\t\t\t});

\t\t\tconst TaskId retrievalTaskId = store_->CreateTask(
\t\t\t\tsessionId, rootTaskId, "ConversationRetrieval", taskSpec, 1);
\t\t\tReportProgress("ConversationRetrieve", Json::object({
\t\t\t\t{"taskId", retrievalTaskId}, {"attempt", attempt + 1}
\t\t\t}));
\t\t\tsupervisor.StartTask(retrievalTaskId);

\t\t\tstd::vector<Evidence> conversationEvidence;
\t\t\tJson retrievalSummary;
\t\t\tResult retrievalResult = RetrievalWorker::Run(
\t\t\t\tctx, retrievalTaskId, taskSpec, &conversationEvidence, &retrievalSummary);
\t\t\tif (!retrievalResult) {
\t\t\t\tsupervisor.FailTask(retrievalTaskId, retrievalResult.error);
\t\t\t\tresolutionFailure = retrievalResult.error;
\t\t\t\tbreak;
\t\t\t}
\t\t\tsupervisor.CompleteTask(retrievalTaskId, retrievalSummary);

\t\t\tJson retrievalPayload = Json::object();
\t\t\tfor (const Evidence& evidence : conversationEvidence) {
\t\t\t\tif (evidence.provenance.sourceType == "ConversationReferenceEvidence" &&
\t\t\t\t    evidence.payload.is_object()) {
\t\t\t\t\tretrievalPayload = evidence.payload;
\t\t\t\t\tbreak;
\t\t\t\t}
\t\t\t}
\t\t\tif (retrievalPayload.empty() || !retrievalPayload.value("found", false)) {
\t\t\t\tresolutionFailure = "参照可能な過去Thread Stateが見つからない";
\t\t\t\tbreak;
\t\t\t}

\t\t\tJson revised;
\t\t\tstd::string retryQuery;
\t\t\tbool resolved = false;
\t\t\tResult resolveResult = ResolveConversationReference(
\t\t\t\tctx, userRequest, intake, retrievalPayload, conversationEvidence,
\t\t\t\t&revised, &retryQuery, &resolved);
\t\t\tif (!resolveResult) {
\t\t\t\tresolutionFailure = resolveResult.error;
\t\t\t\tbreak;
\t\t\t}
\t\t\tif (resolved) {
\t\t\t\tconst TaskId revisionTaskId = store_->CreateTask(
\t\t\t\t\tsessionId, rootTaskId, "RequestRevision",
\t\t\t\t\tJson::object({{"source", "ConversationReferenceEvidence"}}), 1);
\t\t\t\tsupervisor.StartTask(revisionTaskId);
\t\t\t\tintake = std::move(revised);
\t\t\t\tconst Json selectedContext = intake.value("conversationContext", Json::object());
\t\t\t\tprompts::SetCurrentConversationRequestContext(selectedContext, intake);
\t\t\t\t(void)store_->SetConversationThreadState(
\t\t\t\t\tsessionId, BuildRevisedThreadState(sessionId, intake));
\t\t\t\tsupervisor.CompleteTask(revisionTaskId, intake);
\t\t\t\tReportProgress("Replan", Json::object({
\t\t\t\t\t{"requestRevision", intake.value("requestRevision", 0)}
\t\t\t\t}));
\t\t\t\treferenceResolved = true;
\t\t\t\tbreak;
\t\t\t}
\t\t\tif (retryQuery.empty() || attemptedQueries.count(retryQuery) != 0) {
\t\t\t\tresolutionFailure = "Conversation候補が曖昧で追加検索Queryを生成できない";
\t\t\t\tbreak;
\t\t\t}
\t\t\tquery = std::move(retryQuery);
\t\t}

\t\tif (!referenceResolved) {
\t\t\tconst std::string report =
\t\t\t\t"前の会話のどの対象を指しているか特定できませんでした。"
\t\t\t\t"対象名または該当する依頼をもう少し具体的に指定してください。";
\t\t\tstore_->UpdateSessionState(sessionId, "Stopped");
\t\t\tsessionResult.completed = false;
\t\t\tsessionResult.report = report;
\t\t\tsessionResult.stopInfo = Json::object({
\t\t\t\t{"reason", "conversation reference unresolved"},
\t\t\t\t{"detail", resolutionFailure},
\t\t\t\t{"attemptCount", attemptedQueries.size()},
\t\t\t});
\t\t\treturn sessionResult;
\t\t}
\t}

'''
text = replace_once(
    text,
    '\tsupervisor.CompleteTask(rootTaskId, intake);\n\n',
    '\tsupervisor.CompleteTask(rootTaskId, intake);\n\n' + dedent(block),
    'Conversation retrieval mini-plan')
write(path, text)

