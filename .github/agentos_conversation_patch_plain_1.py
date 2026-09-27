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


# Intake initial isolation and unresolved reference marker
path = 'Source/GameApplication/AgentOS/Core/Agents/IntakeAgent.cpp'
text = read(path)
text = text.replace(
    'Json BuildIntakePromptContext(const std::string& userRequest, const Json& fullContext) {',
    '[[maybe_unused]] Json BuildIntakePromptContext(const std::string& userRequest, const Json& fullContext) {',
    1)
old = '''\tJson fullContext = conversationContext;
\tif (RequiresPriorThreadContext(userRequest) &&
\t    (!fullContext.is_object() || fullContext.empty()) &&
\t    ctx.store != nullptr && ctx.sessionId != kInvalidId) {
\t\tfullContext = ctx.store->GetConversationContext(ctx.sessionId);
\t}
\tconst Json promptContext = BuildIntakePromptContext(userRequest, fullContext);
'''
new = '''\t// Initial Intake is intentionally history-free. A context-dependent input is
\t// resolved later through SearchConversationContext as audited Tool Evidence.
\t(void)conversationContext;
\tconst Json promptContext = EmptyStructuredContext("intake_without_history");
'''
text = replace_once(text, old, new, 'Intake history-free entry')
old = '''\tconst bool hasPreviousTurns =
\t\t(promptContext.contains("threadStates") && promptContext.at("threadStates").is_array() &&
\t\t !promptContext.at("threadStates").empty()) ||
\t\t(promptContext.contains("recentTurns") && promptContext.at("recentTurns").is_array() &&
\t\t !promptContext.at("recentTurns").empty());
\tif (IsFreshRuntimeRequest(userRequest)) {
\t\trelation = "refresh";
\t\trequestType = "investigation";
\t\tnormalized["referencedSessionIds"] = Json::array();
\t} else if (hasPreviousTurns && IsExplicitCorrection(userRequest)) {
\t\trelation = "correct";
\t}
\tnormalized["turnRelation"] = relation;
\tnormalized["requestType"] = requestType;
'''
new = '''\tif (IsFreshRuntimeRequest(userRequest)) {
\t\trelation = "refresh";
\t\trequestType = "investigation";
\t\tnormalized["referencedSessionIds"] = Json::array();
\t\tnormalized["unresolvedReferences"] = Json::array();
\t} else if (IsExplicitCorrection(userRequest)) {
\t\trelation = "correct";
\t} else if (IsContinuationReferenceInput(userRequest) &&
\t           (relation == "new" || relation == "refresh")) {
\t\trelation = "refer";
\t}
\tif (RequiresPriorThreadContext(userRequest) &&
\t    normalized.value("unresolvedReferences", Json::array()).empty()) {
\t\tnormalized["unresolvedReferences"] = Json::array({
\t\t\t"過去会話の参照対象: " + userRequest
\t\t});
\t}
\tnormalized["turnRelation"] = relation;
\tnormalized["requestType"] = requestType;
'''
text = replace_once(text, old, new, 'Intake unresolved reference routing')
write(path, text)


# Retrieval evidence type
path = 'Source/GameApplication/AgentOS/Core/Agents/RetrievalWorker.cpp'
text = read(path)
text = replace_once(
    text,
    '\tevidence.provenance.sourceType = "Tool:" + toolName;\n',
    '\tevidence.provenance.sourceType = toolName == "SearchConversationContext"\n'
    '\t\t? "ConversationReferenceEvidence"\n'
    '\t\t: "Tool:" + toolName;\n',
    'Conversation evidence provenance')
text = replace_once(
    text,
    '\t\t"ResolveEntity", "FindEntityByName", "CodeSearch", "SearchComponent", "SearchField",\n',
    '\t\t"ResolveEntity", "FindEntityByName", "CodeSearch", "SearchComponent", "SearchField",\n'
    '\t\t"SearchConversationContext",\n',
    'Conversation discovery tool')
write(path, text)

