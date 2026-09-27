// =======================================================================
//
// AgentOSCodeIndexSmokeTest.cpp
//
// AgentOS Core / CodeIndex（SourceMasker / CodeParser / CodeIndexBuilder）の
// 自己完結スモークテスト。
//
// 重点は「素朴な正規表現パーサが壊れる典型ケース」で壊れないこと。
//   - 文字列やコメントの中の波括弧を数えない
//   - 前方宣言を本体つき宣言と取り違えない
//   - 名前空間の修飾が正しく付く
//
// =======================================================================
#include "AgentOS/Core/CodeIndex/CodeIndexBuilder.h"
#include "AgentOS/Core/CodeIndex/CodeParser.h"
#include "AgentOS/Core/CodeIndex/SourceMasker.h"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

using namespace agentos;

namespace {

const CodeChunk* Find(const std::vector<CodeChunk>& chunks, const std::string& name) {
	for(const CodeChunk& c : chunks) {
		if(c.qualifiedName == name) return &c;
	}
	return nullptr;
}

// -----------------------------------------------------------------------
// SourceMasker
// -----------------------------------------------------------------------
void TestMaskerPreservesLayout() {
	const std::string src =
		"int a = 1; // } } }\n"
		"const char* s = \"){\";\n"
		"/* } } */ int b = 2;\n";

	const std::string masked = MaskLiteralsAndComments(src);

	// 長さと改行位置が保存されること（行番号の一致がこれに依存する）
	assert(masked.size() == src.size());
	std::size_t nlSrc = 0;
	std::size_t nlMasked = 0;
	for(char c : src) if(c == '\n') ++nlSrc;
	for(char c : masked) if(c == '\n') ++nlMasked;
	assert(nlSrc == nlMasked);

	// リテラル／コメント内の括弧が消えていること
	assert(masked.find('}') == std::string::npos);
	assert(masked.find('{') == std::string::npos);

	// コード部分は残っていること
	assert(masked.find("int a = 1;") != std::string::npos);
	assert(masked.find("int b = 2;") != std::string::npos);

	std::printf("  [ok] masker preserves layout and removes literal braces\n");
}

void TestMaskerHandlesEscapesAndRawStrings() {
	// エスケープされた引用符で終端を誤らないこと
	const std::string escaped = "const char* s = \"a\\\"{\"; int x = 0;";
	const std::string m1 = MaskLiteralsAndComments(escaped);
	assert(m1.find('{') == std::string::npos);
	assert(m1.find("int x = 0;") != std::string::npos);

	// 生文字列の中身が潰れること
	const std::string raw = "auto s = R\"json({\"k\":1})json\"; int y = 0;";
	const std::string m2 = MaskLiteralsAndComments(raw);
	assert(m2.find('{') == std::string::npos);
	assert(m2.find("int y = 0;") != std::string::npos);

	// 文字リテラルの括弧
	const std::string ch = "if(c == '{') { g(); }";
	const std::string m3 = MaskLiteralsAndComments(ch);
	std::size_t open = 0;
	for(char c : m3) if(c == '{') ++open;
	assert(open == 1); // リテラルの '{' は消え、ブロックの '{' だけ残る

	std::printf("  [ok] masker handles escapes, raw strings, char literals\n");
}

// -----------------------------------------------------------------------
// CodeParser
// -----------------------------------------------------------------------
void TestParsesFunctionAndType() {
	const std::string src =
		"#pragma once\n"                       // 1
		"namespace agentos {\n"                // 2
		"\n"                                   // 3
		"class Widget {\n"                     // 4
		"public:\n"                            // 5
		"    void Inline() { m_v = 0; }\n"     // 6
		"private:\n"                           // 7
		"    int m_v = 0;\n"                   // 8
		"};\n"                                 // 9
		"\n"                                   // 10
		"} // namespace agentos\n"             // 11
		"\n"                                   // 12
		"void agentos::Widget::Run(int n) {\n" // 13
		"    if(n > 0) { m_v = n; }\n"         // 14
		"}\n";                                 // 15

	ParseStats stats;
	const std::vector<CodeChunk> chunks = ParseSourceFile(
		"Source/GameApplication/AgentOS/Core/Widget.cpp", src, &stats);

	const CodeChunk* type = Find(chunks, "agentos::Widget");
	assert(type != nullptr);
	assert(type->kind == CodeChunkKind::Type);
	assert(type->startLine == 4);
	assert(type->endLine == 9);
	// インラインメンバは型チャンクの中に含まれる（情報は失われない）
	assert(type->text.find("void Inline()") != std::string::npos);
	assert(type->text.find("int m_v") != std::string::npos);

	const CodeChunk* fn = Find(chunks, "agentos::Widget::Run");
	assert(fn != nullptr);
	assert(fn->kind == CodeChunkKind::Function);
	assert(fn->startLine == 13);
	assert(fn->endLine == 15);

	assert(stats.functionCount == 1);
	assert(stats.typeCount == 1);

	std::printf("  [ok] parses function and type with correct line ranges\n");
}

void TestSkipsForwardDeclarations() {
	const std::string src =
		"class Forward;\n"
		"struct AlsoForward;\n"
		"class Real {\n"
		"    int x = 0;\n"
		"};\n";

	ParseStats stats;
	const std::vector<CodeChunk> chunks = ParseSourceFile("a/b/F.h", src, &stats);

	assert(Find(chunks, "Forward") == nullptr);
	assert(Find(chunks, "AlsoForward") == nullptr);
	assert(Find(chunks, "Real") != nullptr);
	assert(stats.typeCount == 1);
	assert(stats.forwardDeclarationCount >= 2);

	std::printf("  [ok] forward declarations are not indexed\n");
}

void TestBracesInLiteralsDoNotBreakRanges() {
	// 文字列とコメントに紛れた波括弧で終端を誤らないこと
	const std::string src =
		"void A::B() {\n"                  // 1
		"    const char* s = \"}\";\n"     // 2
		"    // }\n"                       // 3
		"    /* } */\n"                    // 4
		"    Log(s);\n"                    // 5
		"}\n"                              // 6
		"void A::C() {\n"                  // 7
		"    return;\n"                    // 8
		"}\n";                             // 9

	const std::vector<CodeChunk> chunks = ParseSourceFile("x/Y.cpp", src, nullptr);

	const CodeChunk* b = Find(chunks, "A::B");
	assert(b != nullptr);
	assert(b->startLine == 1);
	assert(b->endLine == 6); // 2〜4行目の '}' に釣られない

	const CodeChunk* c = Find(chunks, "A::C");
	assert(c != nullptr);
	assert(c->startLine == 7);
	assert(c->endLine == 9);

	std::printf("  [ok] literal/comment braces do not corrupt body ranges\n");
}

void TestControlFlowIsNotMistakenForDefinition() {
	// 行頭に来る制御構文や修飾名の参照を関数定義と誤認しないこと
	const std::string src =
		"void A::B() {\n"
		"int x = Foo::Bar(1);\n"        // 行頭だが代入式
		"return Baz::Qux(2);\n"         // 行頭だがreturn
		"}\n";

	ParseStats stats;
	const std::vector<CodeChunk> chunks = ParseSourceFile("x/Z.cpp", src, &stats);

	assert(stats.functionCount == 1);
	assert(Find(chunks, "A::B") != nullptr);
	assert(Find(chunks, "Baz::Qux") == nullptr);

	// 1行に収まった関数の、本体内の修飾呼び出しを定義と誤認しないこと。
	// 修飾名は行内で最初の '(' より前に無ければならない（実走査で発見）。
	const std::string oneLiner =
		"char LowerAscii(unsigned char c) { return static_cast<char>(std::tolower(c)); }\n"
		"int Widen(int v) { return Math::Scale(v); }\n";

	ParseStats s2;
	const std::vector<CodeChunk> c2 = ParseSourceFile("x/W.cpp", oneLiner, &s2);
	assert(Find(c2, "std::tolower") == nullptr);
	assert(Find(c2, "Math::Scale") == nullptr);
	assert(s2.functionCount == 0);

	std::printf("  [ok] control flow / call expressions are not definitions\n");
}

void TestFinalSpecifierIsNotTakenAsTypeName() {
	// "class Foo final : public Bar" の final を型名と誤認しないこと。
	// 誤ると final 指定の型が全て同名に潰れて索引が衝突する（実走査で発見）。
	const std::string src =
		"class Base { int b = 0; };\n"
		"class Derived final : public Base {\n"
		"    int d = 0;\n"
		"};\n"
		"struct MYAPI Exported {\n"
		"    int e = 0;\n"
		"};\n";

	const std::vector<CodeChunk> chunks = ParseSourceFile("q/R.h", src, nullptr);

	assert(Find(chunks, "Derived") != nullptr);
	assert(Find(chunks, "final") == nullptr);
	// 全大文字のトークンはエクスポートマクロとみなし、後ろを型名に採る
	assert(Find(chunks, "Exported") != nullptr);
	assert(Find(chunks, "MYAPI") == nullptr);

	std::printf("  [ok] 'final' specifier / export macro disambiguation\n");
}

void TestNestedNamespaceQualification() {
	const std::string src =
		"namespace a {\n"
		"namespace b {\n"
		"struct S {\n"
		"    int v = 0;\n"
		"};\n"
		"} // namespace b\n"
		"} // namespace a\n"
		"struct Top {\n"
		"    int w = 0;\n"
		"};\n";

	const std::vector<CodeChunk> chunks = ParseSourceFile("m/N.h", src, nullptr);

	assert(Find(chunks, "a::b::S") != nullptr);
	// 名前空間が閉じたあとの型に修飾が残らないこと
	assert(Find(chunks, "Top") != nullptr);

	std::printf("  [ok] nested namespace qualification opens and closes correctly\n");
}

// -----------------------------------------------------------------------
// CodeChunk / Builder
// -----------------------------------------------------------------------
void TestModuleTagAndEmbedText() {
	assert(DeriveModuleTag("Source/GameApplication/AgentOS/Core/Store/SqliteDb.cpp")
	       == "AgentOS/Core/Store");
	assert(DeriveModuleTag("Source/GameApplication/Engine/Editor/UI/MenuBar.cpp")
	       == "Engine/Editor/UI");

	CodeChunk chunk;
	chunk.kind = CodeChunkKind::Function;
	chunk.moduleTag = "AgentOS/Core/Store";
	chunk.qualifiedName = "agentos::SqliteDb::Prepare";
	chunk.text = "Result SqliteDb::Prepare() {}";

	const std::string embed = chunk.EmbedText();
	// 構造ヘッダが先頭に付き、原文がそのまま残っていること
	assert(embed.compare(0, 1, "[") == 0);
	assert(embed.find("[AgentOS/Core/Store] agentos::SqliteDb::Prepare (function)")
	       != std::string::npos);
	assert(embed.find(chunk.text) != std::string::npos);
	assert(chunk.EstimatedTokens() > 0);

	std::printf("  [ok] module tag derivation and embed text format\n");
}

void TestAccumulateReportsContextOverflow() {
	// 2048トークン相当を超える巨大な型宣言を作る
	std::string src = "struct Big {\n";
	for(int i = 0; i < 900; ++i) {
		src += "    int member_with_a_fairly_long_name_" + std::to_string(i) + " = 0;\n";
	}
	src += "};\n";

	std::vector<CodeChunk> chunks;
	CodeIndexReport report;
	AccumulateChunks("p/Q.h", src, &chunks, &report);

	assert(report.totalChunks == 1);
	assert(report.over2048Tokens == 1);
	assert(report.over8192Tokens == 1);
	assert(chunks[0].EstimatedTokens() > 8192);

	std::printf("  [ok] context-limit overflow is counted per chunk\n");
}

void TestBuilderRejectsMissingRoot() {
	CodeIndexOptions options;
	options.root = "definitely/not/a/real/path";
	const Result r = BuildCodeIndex(options, nullptr, nullptr);
	assert(!r);
	assert(!r.error.empty());

	std::printf("  [ok] builder fails cleanly on missing root\n");
}

} // namespace

int main() {
	std::printf("==== AgentOSCodeIndexSmokeTest ====\n");

	TestMaskerPreservesLayout();
	TestMaskerHandlesEscapesAndRawStrings();
	TestParsesFunctionAndType();
	TestSkipsForwardDeclarations();
	TestBracesInLiteralsDoNotBreakRanges();
	TestControlFlowIsNotMistakenForDefinition();
	TestFinalSpecifierIsNotTakenAsTypeName();
	TestNestedNamespaceQualification();
	TestModuleTagAndEmbedText();
	TestAccumulateReportsContextOverflow();
	TestBuilderRejectsMissingRoot();

	std::printf("==== AgentOSCodeIndexSmokeTest: PASSED ====\n");
	return 0;
}
