// =======================================================================
//
// CodeIndexBuilder.h
//
// ディレクトリを走査して CodeChunk の集合を作る層。
//
// 意図的にllama.cppにもD3D11にも依存させていない。
// AgentOS/Core配下に置くことで Tests/AgentOS/Makefile からLinuxでビルド・
// 実行でき、MSVC実ビルドを待たずに検証できる
// （Docs/AgentOS/04_Execution_Engine_Roadmap.md §7 の制約回避）。
//
// =======================================================================
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "../AgentOsTypes.h"
#include "../Json.h"
#include "CodeChunk.h"
#include "CodeParser.h"

namespace agentos {

// ---------------------------------
// 走査オプション
// ---------------------------------
struct CodeIndexOptions {
	// 走査の起点（リポジトリルートからの相対パス）
	std::string root = "Source";

	// パスにこの文字列を含むものを除外する。
	// 既定で Backends を外すのは、そこがvendorコード（imgui / sqlite3 /
	// llama.cpp / assimp）であり、RAGで引きたい自作APIではないため。
	// 実測で全313k行のうち自作分は97.5k行しかない。
	std::vector<std::string> excludeSubstrings{"Backends"};

	// 対象拡張子
	std::vector<std::string> extensions{".cpp", ".h", ".hpp"};
};

// ---------------------------------
// 走査結果のレポート
// ---------------------------------
struct CodeIndexReport {
	int filesScanned = 0;
	int filesSkipped = 0;
	int filesFailed = 0;

	ParseStats stats;

	std::size_t totalChunks = 0;
	std::size_t totalEstimatedTokens = 0;

	// 埋め込みモデルのコンテキスト上限に収まらないチャンク数。
	// モデル選定に直結する数字なので明示的に持つ。
	std::size_t over2048Tokens = 0;
	std::size_t over8192Tokens = 0;

	Json ToJson() const;
};

// 対象ファイルのパス一覧だけを得る（パース前）。
// 差分更新では「ハッシュを比べて変わったものだけパースする」ため、
// 走査とパースを分けられる必要がある。
Result EnumerateSourceFiles(
	const CodeIndexOptions& options,
	std::vector<std::string>* outPaths,
	int* outSkipped);

// ファイル内容を読み出す。読めなければ false。
bool ReadSourceFile(const std::string& path, std::string* outContent);

// ディレクトリを走査してチャンクを構築する。
// ファイル順は決定論的（パス昇順）にソートされる。
Result BuildCodeIndex(
	const CodeIndexOptions& options,
	std::vector<CodeChunk>* outChunks,
	CodeIndexReport* outReport);

// 1ファイル分をチャンク化してレポートへ反映する（テストからも使う）。
void AccumulateChunks(
	const std::string& relativePath,
	const std::string& source,
	std::vector<CodeChunk>* outChunks,
	CodeIndexReport* outReport);

Json ChunksToJson(const std::vector<CodeChunk>& chunks);

} // namespace agentos
