// =======================================================================
//
// CodeChunk.h
//
// RAG索引の最小単位。ソース上の「意味のあるひと塊」を表す。
// 現状は関数定義と型宣言の2種（Docs/AgentOS/04_Execution_Engine_Roadmap.md
// のRAG下層 ステップ1）。
//
// 変数を独立チャンクにしない理由：
//   実測でメンバ変数の宣言は約9,400件あり関数の14倍になる。
//   加えて変数は単体では意味が定まらない（m_countの意味は所属クラスに依存する）。
//   よって変数は所属する型宣言チャンクの中に含める形で表現する。
//
// =======================================================================
#pragma once

#include <cstddef>
#include <string>

#include "../Json.h"

namespace agentos {

// ---------------------------------
// チャンク種別
// ---------------------------------
enum class CodeChunkKind {
	Function, // 関数定義（クラス外定義 Type Class::Method(...) {...}）
	Type,     // 型宣言（class / struct の本体）
};

const char* ToString(CodeChunkKind kind) noexcept;

// ---------------------------------
// CodeChunk
// ---------------------------------
struct CodeChunk {
	CodeChunkKind kind = CodeChunkKind::Function;

	// リポジトリルートからの相対パス（区切りは '/' に正規化済み）
	std::string filePath;

	// 所属モジュールの見出し。例: "AgentOS/Core/Store"
	// ディレクトリ階層はそのままドメインの区分になっているため、
	// 埋め込みテキストの文脈として有効に働く。
	std::string moduleTag;

	// 名前空間を含む修飾名。例: "agentos::SqliteDb::Prepare"
	std::string qualifiedName;

	// 1-based・両端を含む行範囲
	int startLine = 0;
	int endLine = 0;

	// 原文（ground truth。要約で置き換えず、常に保持する）
	std::string text;

	// 埋め込みモデルへ渡すテキスト。
	// 構造ヘッダを先頭へ足した「文脈つき原文」を返す。
	// 構造情報はパーサで決定論的に得られるため、生成コストも誤りもゼロ。
	std::string EmbedText() const;

	// トークン数の概算。コードはおよそ3.2文字/トークン。
	// モデルのコンテキスト上限に収まるかの事前判定に使う。
	std::size_t EstimatedTokens() const;

	Json ToJson() const;
};

} // namespace agentos
