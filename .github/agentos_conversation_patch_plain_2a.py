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


# Orchestrator tool and resolver
path = 'Source/GameApplication/AgentOS/Core/Orchestrator/Orchestrator.cpp'
text = read(path)
text = replace_once(
    text,
    '#include <cstdint>\n',
    '#include <algorithm>\n#include <cstdint>\n',
    'Orchestrator algorithm include')
helpers = r'''
class SearchConversationContextTool final : public ICommandExecutor {
public:
\tSearchConversationContextTool(TaskStore* store, SessionId currentSession)
\t\t: store_(store), currentSession_(currentSession) {
\t\tdescriptor_.name = "SearchConversationContext";
\t\tdescriptor_.description =
\t\t\t"過去会話の構造化Thread Stateを検索する。Raw assistant本文は返さない。";
\t\tdescriptor_.requiredPermission = PermissionLevel::Read;
\t\tdescriptor_.argumentSchema = Json::object({
\t\t\t{"query", Json::object({{"type", "string"}, {"required", true}})},
\t\t\t{"beforeSessionId", Json::object({{"type", "integer"}, {"required", false}})},
\t\t\t{"limit", Json::object({{"type", "integer"}, {"required", false}})},
\t\t});
\t}

\tconst ToolDescriptor& Descriptor() const override { return descriptor_; }

\tResult CheckPrecondition(const Json& arguments) override {
\t\tif (store_ == nullptr) return Result::Fail("TaskStore is unavailable");
\t\tif (!arguments.is_object() || !arguments.contains("query") ||
\t\t    !arguments.at("query").is_string() ||
\t\t    arguments.at("query").get<std::string>().empty()) {
\t\t\treturn Result::Fail("query must be a non-empty string");
\t\t}
\t\tif (arguments.contains("beforeSessionId") &&
\t\t    !arguments.at("beforeSessionId").is_number_integer()) {
\t\t\treturn Result::Fail("beforeSessionId must be an integer");
\t\t}
\t\tif (arguments.contains("limit") && !arguments.at("limit").is_number_integer()) {
\t\t\treturn Result::Fail("limit must be an integer");
\t\t}
\t\treturn Result::Ok();
\t}

\tCommandResult Execute(const Json& arguments) override {
\t\tconst SessionId requestedBefore = arguments.value("beforeSessionId", currentSession_);
\t\tconst SessionId before = requestedBefore <= 0
\t\t\t? currentSession_
\t\t\t: (std::min)(requestedBefore, currentSession_);
\t\treturn CommandResult::Ok(store_->SearchConversationContext(
\t\t\targuments.value("query", std::string()), before,
\t\t\targuments.value("limit", 5)));
\t}

private:
\tTaskStore* store_ = nullptr;
\tSessionId currentSession_ = kInvalidId;
\tToolDescriptor descriptor_;
};

bool NeedsConversationReferenceResolution(const Json& intake) {
\tif (!intake.is_object()) return false;
\tconst std::string relation = intake.value("turnRelation", std::string("new"));
\tconst bool relationNeedsContext =
\t\trelation == "continue" || relation == "correct" ||
\t\trelation == "clarify" || relation == "refer";
\tconst bool unresolved =
\t\tintake.contains("unresolvedReferences") &&
\t\tintake.at("unresolvedReferences").is_array() &&
\t\t!intake.at("unresolvedReferences").empty();
\treturn relationNeedsContext || unresolved;
}

Json BuildRevisedThreadState(SessionId sessionId, const Json& intake) {
\treturn Json::object({
\t\t{"sessionId", sessionId},
\t\t{"goal", intake.value("goal", std::string())},
\t\t{"resolvedRequest", intake.value("resolvedRequest", std::string())},
\t\t{"turnRelation", intake.value("turnRelation", std::string("new"))},
\t\t{"requestType", intake.value("requestType", std::string("investigation"))},
\t\t{"targets", Json::object({
\t\t\t{"kind", intake.value("targetKind", std::string("unknown"))},
\t\t\t{"concept", intake.contains("targetConcept") ? intake.at("targetConcept") : Json(nullptr)},
\t\t\t{"entityName", intake.contains("resolvedEntityName") ? intake.at("resolvedEntityName") : Json(nullptr)},
\t\t})},
\t\t{"constraints", intake.value("constraints", Json::array())},
\t\t{"unresolved", intake.value("unresolvedReferences", Json::array())},
\t\t{"status", intake.value("requestType", std::string()) == "conversation"
\t\t\t? "conversation" : "active"},
\t});
}

Result ResolveConversationReference(
\tAgentContext& ctx,
\tconst std::string& userRequest,
\tconst Json& intake,
\tconst Json& retrievalPayload,
\tconst std::vector<Evidence>& retrievalEvidence,
\tJson* revisedOut,
\tstd::string* retryQueryOut,
\tbool* resolvedOut) {

\tif (revisedOut == nullptr || retryQueryOut == nullptr || resolvedOut == nullptr) {
\t\treturn Result::Fail("ResolveConversationReference: output is null");
\t}
\t*resolvedOut = false;
\tretryQueryOut->clear();

\tJson compactIntake = intake;
\tcompactIntake.erase("conversationContext");
\tcompactIntake.erase("historyIdentifiers");
\tcompactIntake.erase("memoryDerivedNotes");

\tPromptPair prompt;
\tprompt.system =
\t\t"あなたはAgentOSのConversation Reference Resolverです。\\n"
\t\t"出力は単一の```jsonフェンス内のJSONオブジェクトだけにしてください。\\n"
\t\t"現在入力の代名詞・訂正・継続対象を、Toolが返した構造化Thread Stateだけから解決します。\\n"
\t\t"Raw assistant本文を要求・推測しないでください。候補が曖昧ならresolved=falseにします。\\n"
\t\t"requestPatchは履歴を知らないPlannerでも実行できるstandaloneな要求にしてください。\\n"
\t\t"出力スキーマ:\\n"
\t\t"{\\\"resolved\\\":boolean,\\\"confidence\\\":number,\\\"retryQuery\\\":string|null,"
\t\t"\\\"requestPatch\\\":{\\\"goal\\\":string,\\\"resolvedRequest\\\":string,"
\t\t"\\\"requestType\\\":\\\"conversation\\\"|\\\"investigation\\\","
\t\t"\\\"targetKind\\\":string,\\\"targetConcept\\\":string|null,"
\t\t"\\\"resolvedEntityName\\\":string|null,\\\"constraints\\\":[string],"
\t\t"\\\"requiredCapabilities\\\":[string],\\\"unresolvedReferences\\\":[string],"
\t\t"\\\"reason\\\":string}|null}\\n/no_think\\n";
\tprompt.user =
\t\t"現在のユーザー入力:\\n" + userRequest +
\t\t"\\n\\n履歴なしIntake:\\n" + prompts::Truncate(compactIntake.dump(2), 5000) +
\t\t"\\n\\nSearchConversationContext Evidence:\\n" +
\t\tprompts::Truncate(retrievalPayload.dump(2), 10000);

\tJson raw;
\tResult call = CallLlmJson(ctx, prompt, &raw);
\tif (!call) return call;
\tif (!raw.is_object()) return Result::Fail("conversation resolver returned no object");

\tif (raw.contains("retryQuery") && raw.at("retryQuery").is_string()) {
\t\t*retryQueryOut = raw.at("retryQuery").get<std::string>();
\t}
\tconst bool resolved = raw.value("resolved", false);
\tconst double confidence = raw.value("confidence", 0.0);
\tif (!resolved || confidence < 0.55 || !raw.contains("requestPatch") ||
\t    !raw.at("requestPatch").is_object()) {
\t\treturn Result::Ok();
\t}

\tconst Json& patch = raw.at("requestPatch");
\tJson revised = intake;
\tconst std::string immutableRootGoal =
\t\trevised.value("rootGoal", revised.value("goal", std::string()));
\tconst std::string immutableRootResolved =
\t\trevised.value("rootResolvedRequest", revised.value("resolvedRequest", std::string()));

\tfor (const char* key : {"goal", "resolvedRequest", "requestType", "targetKind"}) {
\t\tif (patch.contains(key) && patch.at(key).is_string() &&
\t\t    !patch.at(key).get<std::string>().empty()) revised[key] = patch.at(key);
\t}
\tfor (const char* key : {"targetConcept", "resolvedEntityName"}) {
\t\tif (patch.contains(key) && (patch.at(key).is_string() || patch.at(key).is_null())) {
\t\t\trevised[key] = patch.at(key);
\t\t}
\t}
\tfor (const char* key : {"constraints", "requiredCapabilities", "unresolvedReferences"}) {
\t\tif (patch.contains(key) && patch.at(key).is_array()) revised[key] = patch.at(key);
\t}
\trevised["rootGoal"] = immutableRootGoal;
\trevised["rootResolvedRequest"] = immutableRootResolved;
\trevised["requestRevision"] = revised.value("requestRevision", 0) + 1;
\trevised["requestRevisionReason"] = patch.value(
\t\t"reason", std::string("ConversationReferenceEvidenceで参照対象を解決"));
\trevised["conversationContext"] = Json::object({
\t\t{"summary", ""},
\t\t{"recentTurns", Json::array()},
\t\t{"threadStates", Json::array()},
\t\t{"selectionPolicy", "tool_resolved_no_raw_history"},
\t\t{"memoryPolicy", "conversation_evidence_only"},
\t});

\tJson evidenceIds = Json::array();
\tfor (const Evidence& evidence : retrievalEvidence) {
\t\tif (evidence.id != kInvalidId) evidenceIds.push_back(evidence.id);
\t}
\tJson selectedSessions = Json::array();
\tif (retrievalPayload.contains("matches") && retrievalPayload.at("matches").is_array()) {
\t\tfor (const Json& match : retrievalPayload.at("matches")) {
\t\t\tif (match.is_object() && match.contains("sessionId")) {
\t\t\t\tselectedSessions.push_back(match.at("sessionId"));
\t\t\t}
\t\t}
\t}
\trevised["conversationResolution"] = Json::object({
\t\t{"evidenceKind", "ConversationReferenceEvidence"},
\t\t{"evidenceIds", std::move(evidenceIds)},
\t\t{"candidateSessionIds", std::move(selectedSessions)},
\t\t{"confidence", confidence},
\t\t{"assistantTextIncluded", false},
\t});

\t*revisedOut = std::move(revised);
\t*resolvedOut = true;
\treturn Result::Ok();
}
'''
text = replace_once(
    text,
    '\n} // namespace\n\nOrchestrator::Orchestrator',
    '\n' + dedent(helpers) + '\n} // namespace\n\nOrchestrator::Orchestrator',
    'Orchestrator conversation helpers')
text = replace_once(
    text,
    '\tpipeline_->AddAuditSink(auditSink);\n',
    '\tpipeline_->AddAuditSink(auditSink);\n'
    '\tpipeline_->RegisterTool(std::make_shared<SearchConversationContextTool>(store_, sessionId));\n',
    'Register conversation tool')

write(path, text)
