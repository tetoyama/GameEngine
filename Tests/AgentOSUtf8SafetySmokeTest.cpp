// =======================================================================
//
// AgentOSUtf8SafetySmokeTest.cpp
//
// 実機で踏んだクラッシュの再現と回帰防止。
//
// 症状:
//   LLM実行中に例外がスローされ、誰も捕まえずに terminate -> abort。
//   スタックは
//     evidence_prompt::Compress
//       -> json::dump
//         -> serializer::dump_escaped
//           -> throw
//
// 原因:
//   Evidenceやプロンプトの文字列を substr(0, n) でバイト単位に切っていた。
//   対象がほぼASCIIだった頃は問題にならなかったが、CodeSearchが
//   ソースコードの抜粋（日本語コメント入り）をEvidenceへ載せるようになり、
//   切断点が多バイト文字の途中に落ちて不正なUTF-8が生成された。
//   nlohmann::json の dump() は不正なUTF-8で例外を投げる。
//
// 対策は2層で、両方をここで検証する。
//   1. 切り詰めを文字境界で行う（TruncateUtf8）
//   2. dump時に error_handler_t::replace を使い、
//      万一不正な文字列が来てもプロセスを落とさない（SafeDump）
//
// =======================================================================
#include "AgentOS/Core/Evidence/EvidencePromptCompressor.h"
#include "AgentOS/Core/Json.h"
#include "AgentOS/Core/TextUtf8.h"

#include <cassert>
#include <cstdio>
#include <string>

using namespace agentos;

namespace {

// UTF-8として妥当か（3バイト文字までの簡易判定）
bool IsValidUtf8(const std::string& s) {
	std::size_t i = 0;
	while(i < s.size()) {
		const unsigned char c = static_cast<unsigned char>(s[i]);
		std::size_t len = 0;
		if(c < 0x80) len = 1;
		else if((c & 0xE0) == 0xC0) len = 2;
		else if((c & 0xF0) == 0xE0) len = 3;
		else if((c & 0xF8) == 0xF0) len = 4;
		else return false;                       // 継続バイトで始まっている
		if(i + len > s.size()) return false;     // 途中で切れている
		for(std::size_t k = 1; k < len; ++k) {
			if(!IsUtf8ContinuationByte(static_cast<unsigned char>(s[i + k]))) return false;
		}
		i += len;
	}
	return true;
}

// -----------------------------------------------------------------------
void TestTruncateNeverSplitsACharacter() {
	// 日本語は1文字3バイト。どのバイト位置で切っても壊れてはいけない。
	const std::string text = "軸平行境界ボックスの重なりを判定する";
	assert(IsValidUtf8(text));

	for(std::size_t limit = 0; limit <= text.size() + 2; ++limit) {
		const std::string cut = TruncateUtf8(text, limit);
		assert(IsValidUtf8(cut));
		assert(cut.size() <= text.size());
		// 切り詰め後は必ず元の接頭辞になっている
		assert(text.compare(0, cut.size(), cut) == 0);
	}

	// suffixを付けても壊れない
	for(std::size_t limit = 1; limit < text.size(); ++limit) {
		assert(IsValidUtf8(TruncateUtf8(text, limit, "...(truncated)...")));
	}

	std::printf("  [ok] TruncateUtf8 never splits a multi-byte character\n");
}

void TestAsciiIsUnaffected() {
	const std::string text = "Result SqliteDb::Prepare(const std::string& sql)";
	assert(TruncateUtf8(text, text.size()) == text);
	assert(TruncateUtf8(text, 6) == "Result");
	assert(TruncateUtf8(text, 6, "...") == "Result...");
	std::printf("  [ok] ASCII truncation is byte exact\n");
}

// -----------------------------------------------------------------------
// 実機の形を再現する
// -----------------------------------------------------------------------
void TestCompressHandlesJapaneseCodeSnippets() {
	// CodeSearchが返すEvidenceを模す。
	// 日本語コメント入りのコードを、圧縮で必ず切り詰められる長さだけ積む。
	Json evidences = Json::array();
	for(int i = 1; i <= 12; ++i) {
		std::string code;
		for(int line = 0; line < 40; ++line) {
			code += "\t// 軸平行境界ボックスの重なりを判定する処理をここに記述する\n";
			code += "\tif(a.min.x <= b.max.x && a.max.x >= b.min.x) { return true; }\n";
		}
		evidences.push_back(Json::object({
			{"id", i},
			{"taskId", 100 + i},
			{"claim", "「当たり判定の実装」に関連するコードを見つけた。最有力は Physics::AabbOverlap。"},
			{"payload", Json::object({
				{"query", "当たり判定 重なり 判定"},
				{"count", 5},
				{"results", Json::array({Json::object({
					{"name", "Physics::AabbOverlap"},
					{"file", "Source/GameApplication/Engine/Physics/Collision.cpp"},
					{"code", code},
				})})},
			})},
			{"provenance", Json::object({
				{"sourceType", "Tool:CodeSearch"},
				{"sourceUri", "CodeSearch"},
			})},
		}));
	}

	const Json built = Json::object({
		{"evidences", evidences},
		{"contradictions", Json::array()},
		{"coverage", 1.0},
		{"tasksWithoutEvidence", Json::array()},
		{"usableEvidenceCount", evidences.size()},
		{"failedEvidenceCount", 0},
		{"activeRevision", 0},
	});

	// 修正前はここで dump_escaped が投げ、terminateしていた。
	const Json compressed = evidence_prompt::Compress(built, 4000);
	const std::string text = evidence_prompt::CompressToString(built, 4000);

	assert(!text.empty());
	assert(IsValidUtf8(text));
	assert(compressed.contains("evidences"));

	// 予算内に収まっていること（圧縮の本来の目的）
	assert(text.size() <= 4000 + 512);

	std::printf("  [ok] Compress survives Japanese code snippets and stays valid UTF-8\n");
}

void TestCompressDoesNotThrowOnBrokenInput() {
	// 外部（ツール出力・LLM出力）から不正なUTF-8が来ても落ちないこと。
	// 切り詰めを直しても、入力そのものが壊れている可能性は消えない。
	std::string broken = "壊れた文字列";
	broken.push_back(static_cast<char>(0xE3)); // 3バイト文字の先頭だけ

	const Json built = Json::object({
		{"evidences", Json::array({Json::object({
			{"id", 1},
			{"taskId", 1},
			{"claim", broken},
			{"payload", Json::object({{"code", broken}})},
			{"provenance", Json::object({{"sourceType", "Tool:CodeSearch"}})},
		})})},
		{"contradictions", Json::array()},
		{"coverage", 1.0},
		{"tasksWithoutEvidence", Json::array()},
		{"usableEvidenceCount", 1},
		{"failedEvidenceCount", 0},
		{"activeRevision", 0},
	});

	bool threw = false;
	std::string text;
	try {
		text = evidence_prompt::CompressToString(built, 4000);
	} catch(...) {
		threw = true;
	}

	// 例外を投げないことが要件。プロセスを落とすより置換文字の方が遥かに良い。
	assert(!threw);
	assert(!text.empty());

	std::printf("  [ok] Compress does not throw even on invalid UTF-8 input\n");
}

} // namespace

int main() {
	std::printf("==== AgentOSUtf8SafetySmokeTest ====\n");

	TestTruncateNeverSplitsACharacter();
	TestAsciiIsUnaffected();
	TestCompressHandlesJapaneseCodeSnippets();
	TestCompressDoesNotThrowOnBrokenInput();

	std::printf("==== AgentOSUtf8SafetySmokeTest: PASSED ====\n");
	return 0;
}
