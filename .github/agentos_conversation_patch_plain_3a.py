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


# Update legacy tests to the new isolation contract
path = 'Tests/AgentOSConversationMemorySmokeTest.cpp'
text = read(path)
text = replace_once(
    text,
    '''\tassert(calls.size() == 1);
\tassert(calls[0].second.find("現在のシーン全体を報告して") != std::string::npos);
\tassert(calls[0].second.find("Scene全体のEntityとSystemを報告しました") != std::string::npos);
\tassert(calls[0].second.find("そうじゃなくて、Playerだけ見て") != std::string::npos);

\tstd::puts("  - correction resolution uses complete history: OK");
''',
    '''\tassert(calls.size() == 1);
\tassert(calls[0].second.find("現在のシーン全体を報告して") == std::string::npos);
\tassert(calls[0].second.find("Scene全体のEntityとSystemを報告しました") == std::string::npos);
\tassert(calls[0].second.find("そうじゃなくて、Playerだけ見て") != std::string::npos);
\tassert(!intake.at("unresolvedReferences").empty());

\tstd::puts("  - correction Intake is history-free and marks unresolved reference: OK");
''',
    'Correction isolation assertions')
text = replace_once(
    text,
    '''\tconst auto calls = llm.GetCalls();
\tassert(calls.size() == 2);

\tconst Json context = store.GetConversationContext(current);
\tassert(context.value("totalTurns", 0) == 10);
\tassert(context.value("summarizedThroughSessionId", kInvalidId) == fourth);
\tassert(context.value("summary", std::string()).find("turn 0〜3") != std::string::npos);
\tassert(context.at("recentTurns").size() == 6);
\tassert(context.at("recentTurns")[0].value("user", std::string()) == "user turn 4");
\tassert(context.at("recentTurns")[5].value("assistant", std::string()) == "assistant final 9");

\tstd::puts("  - automatic compression retains recent raw turns: OK");
''',
    '''\tconst auto calls = llm.GetCalls();
\tassert(calls.size() == 1);

\tconst Json context = store.GetConversationContext(current);
\tassert(context.value("totalTurns", 0) == 10);
\tassert(context.value("summarizedThroughSessionId", kInvalidId) == kInvalidId);
\tassert(context.value("summary", std::string()).empty());
\tassert(context.at("recentTurns").size() == 10);
\tassert(context.at("recentTurns")[0].value("user", std::string()) == "user turn 0");
\tassert(context.at("recentTurns")[9].value("assistant", std::string()) == "assistant final 9");
\t(void)fourth;

\tstd::puts("  - initial Intake leaves raw turns in the audit store only: OK");
''',
    'Compression test isolation assertions')
text = replace_once(
    text,
    '''\tconst Json context = store.GetConversationContext(current);
\tassert(context.value("totalTurns", 0) == 10);
\tassert(context.value("summarizedThroughSessionId", kInvalidId) == fourth);
\tassert(context.value("summary", std::string()).find("deterministic conversation compression") != std::string::npos);
\tassert(context.at("recentTurns").size() == 6);
\tassert(context.at("recentTurns")[5].value("assistant", std::string()) == "fallback assistant 9");

\tstd::puts("  - failed compression falls back to bounded deterministic summary: OK");
''',
    '''\tconst Json context = store.GetConversationContext(current);
\tassert(context.value("totalTurns", 0) == 10);
\tassert(context.value("summarizedThroughSessionId", kInvalidId) == kInvalidId);
\tassert(context.value("summary", std::string()).empty());
\tassert(context.at("recentTurns").size() == 10);
\tassert(context.at("recentTurns")[9].value("assistant", std::string()) == "fallback assistant 9");
\t(void)fourth;

\tstd::puts("  - failed memory response cannot trigger hidden pre-Intake compression: OK");
''',
    'Compression fallback isolation assertions')
write(path, text)

