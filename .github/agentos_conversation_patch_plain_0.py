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


# TaskStore declaration
path = 'Source/GameApplication/AgentOS/Core/Store/TaskStore.h'
text = read(path)
text = replace_once(
    text,
    '\tJson GetConversationContext(SessionId beforeSessionId);\n',
    '\tJson GetConversationContext(SessionId beforeSessionId);\n'
    '\tJson SearchConversationContext(\n'
    '\t\tconst std::string& query,\n'
    '\t\tSessionId beforeSessionId,\n'
    '\t\tint limit = 5);\n',
    'TaskStore search declaration')
write(path, text)


# TaskStore implementation
path = 'Source/GameApplication/AgentOS/Core/Store/TaskStore.cpp'
text = read(path)
text = replace_once(
    text,
    '#include <string>\n#include <vector>\n',
    '#include <algorithm>\n#include <cctype>\n#include <string>\n'
    '#include <unordered_set>\n#include <utility>\n#include <vector>\n',
    'TaskStore includes')

helper = r'''
std::string LowerAscii(std::string text) {
\tstd::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
\t\treturn static_cast<char>(std::tolower(ch));
\t});
\treturn text;
}

std::vector<std::string> SearchTokens(const std::string& text) {
\tstatic const std::unordered_set<std::string> kIgnored = {
\t\t"それ", "その", "これ", "この", "前の", "続けて", "続き",
\t\t"the", "that", "this", "previous", "continue", "again",
\t};
\tstd::vector<std::string> tokens;
\tstd::string ascii;
\tauto flush = [&]() {
\t\tif (ascii.size() >= 2) {
\t\t\tstd::string lower = LowerAscii(ascii);
\t\t\tif (kIgnored.count(lower) == 0 &&
\t\t\t    std::find(tokens.begin(), tokens.end(), lower) == tokens.end()) {
\t\t\t\ttokens.push_back(std::move(lower));
\t\t\t}
\t\t}
\t\tascii.clear();
\t};
\tfor (unsigned char ch : text) {
\t\tif (std::isalnum(ch) != 0 || ch == '_' || ch == '-') {
\t\t\tascii.push_back(static_cast<char>(ch));
\t\t} else {
\t\t\tflush();
\t\t}
\t}
\tflush();

\tfor (const char* known : {
\t\t"Player", "Camera", "Field", "Entity", "Component", "System",
\t\t"ジャンプ", "会話", "履歴", "メモリ", "修正", "調査"
\t}) {
\t\tif (text.find(known) != std::string::npos) {
\t\t\tstd::string token = LowerAscii(known);
\t\t\tif (kIgnored.count(token) == 0 &&
\t\t\t    std::find(tokens.begin(), tokens.end(), token) == tokens.end()) {
\t\t\t\ttokens.push_back(std::move(token));
\t\t\t}
\t\t}
\t}
\treturn tokens;
}

std::string TruncateConversationText(const std::string& text, std::size_t limit) {
\tif (text.size() <= limit) return text;
\treturn text.substr(0, limit) + "...(truncated)";
}
'''
text = replace_once(
    text,
    '\n} // namespace\n\nResult TaskStore::Open',
    '\n' + dedent(helper) + '\n} // namespace\n\nResult TaskStore::Open',
    'TaskStore search helpers')

method = r'''
Json TaskStore::SearchConversationContext(
\tconst std::string& query,
\tSessionId beforeSessionId,
\tint limit) {

\tstd::lock_guard<std::mutex> lock(mutex_);
\tconst int boundedLimit = (std::max)(1, (std::min)(limit, 5));
\tconst std::vector<std::string> queryTokens = SearchTokens(query);

\tstruct Candidate {
\t\tSessionId sessionId = kInvalidId;
\t\tJson state = Json::object();
\t\tstd::string userText;
\t\tdouble score = 0.0;
\t};

\tstd::vector<Candidate> candidates;
\tStatement stmt;
\tResult r = db_.Prepare(
\t\t"SELECT cts.session_id, cts.state_json, COALESCE(ct.user_text, '') "
\t\t"FROM ConversationThreadState cts "
\t\t"LEFT JOIN ConversationTurn ct ON ct.session_id=cts.session_id "
\t\t"WHERE cts.session_id < ?1 "
\t\t"ORDER BY cts.session_id DESC LIMIT 32;",
\t\t&stmt);
\tif (!r) {
\t\treturn Json::object({
\t\t\t{"found", false},
\t\t\t{"failure", true},
\t\t\t{"error", r.error},
\t\t\t{"matches", Json::array()},
\t\t});
\t}
\tstmt.BindInt64(1, beforeSessionId);

\tint rank = 0;
\twhile (stmt.Step() == Statement::StepResult::Row) {
\t\tJson state = Json::parse(stmt.ColumnText(1), nullptr, false);
\t\tif (!state.is_object() || state.is_discarded()) {
\t\t\t++rank;
\t\t\tcontinue;
\t\t}
\t\tCandidate candidate;
\t\tcandidate.sessionId = stmt.ColumnInt64(0);
\t\tcandidate.state = std::move(state);
\t\tcandidate.userText = stmt.ColumnText(2);

\t\tconst std::string searchable = LowerAscii(
\t\t\tcandidate.userText + "\\n" + candidate.state.dump());
\t\tconst double recency = 0.45 / static_cast<double>(rank + 1);
\t\tconst bool active =
\t\t\tcandidate.state.value("status", std::string()) == "active" ||
\t\t\tcandidate.state.value("requestType", std::string()) == "investigation";
\t\tdouble overlap = 0.0;
\t\tfor (const std::string& token : queryTokens) {
\t\t\tif (!token.empty() && searchable.find(token) != std::string::npos) overlap += 0.12;
\t\t}
\t\tcandidate.score = (std::min)(1.0, recency + (active ? 0.35 : 0.10) +
\t\t\t(std::min)(overlap, 0.35));
\t\tcandidates.push_back(std::move(candidate));
\t\t++rank;
\t}

\tstd::stable_sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
\t\tif (a.score != b.score) return a.score > b.score;
\t\treturn a.sessionId > b.sessionId;
\t});

\tJson matches = Json::array();
\tconst std::size_t count = (std::min)(
\t\tstatic_cast<std::size_t>(boundedLimit), candidates.size());
\tfor (std::size_t i = 0; i < count; ++i) {
\t\tconst Candidate& candidate = candidates[i];
\t\tJson state = candidate.state;
\t\tstate["sessionId"] = candidate.sessionId;
\t\tmatches.push_back(Json::object({
\t\t\t{"sessionId", candidate.sessionId},
\t\t\t{"score", candidate.score},
\t\t\t{"selectionReason", i == 0
\t\t\t\t? "best_structured_thread_match"
\t\t\t\t: "alternate_structured_thread_match"},
\t\t\t{"threadState", std::move(state)},
\t\t\t{"userExcerpt", TruncateConversationText(candidate.userText, 600)},
\t\t}));
\t}

\tconst bool found = !matches.empty();
\tconst double confidence = found ? matches[0].value("score", 0.0) : 0.0;
\treturn Json::object({
\t\t{"found", found},
\t\t{"claim", found
\t\t\t? "過去会話の構造化Thread State候補を取得した"
\t\t\t: "参照可能な過去会話Thread Stateが見つからなかった"},
\t\t{"evidenceKind", "ConversationReferenceEvidence"},
\t\t{"query", query},
\t\t{"beforeSessionId", beforeSessionId},
\t\t{"assistantTextIncluded", false},
\t\t{"confidenceHint", confidence},
\t\t{"matches", std::move(matches)},
\t});
}

'''
text = replace_once(
    text,
    '\nResult TaskStore::UpdateConversationSummary(',
    '\n' + dedent(method) + 'Result TaskStore::UpdateConversationSummary(',
    'TaskStore search method')
write(path, text)

