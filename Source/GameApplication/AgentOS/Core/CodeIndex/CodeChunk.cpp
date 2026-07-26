// =======================================================================
//
// CodeChunk.cpp
//
// =======================================================================
#include "CodeChunk.h"

namespace agentos {

namespace {

// コードの文字数→トークン数の換算係数。
// 英数字と記号が主体のC++では概ね 3.0〜3.5 文字/トークンに収まる。
constexpr double kCharsPerToken = 3.2;

} // namespace

const char* ToString(CodeChunkKind kind) noexcept {
	switch(kind) {
	case CodeChunkKind::Function: return "function";
	case CodeChunkKind::Type:     return "type";
	}
	return "unknown";
}

std::string CodeChunk::EmbedText() const {
	// 例:
	//   [AgentOS/Core/Store] agentos::SqliteDb::Prepare (function)
	//   Result SqliteDb::Prepare(const std::string& sql, Statement* out) {
	//       ...
	std::string header;
	header.reserve(moduleTag.size() + qualifiedName.size() + 32);
	header += '[';
	header += moduleTag;
	header += "] ";
	header += qualifiedName;
	header += " (";
	header += ToString(kind);
	header += ")\n";
	return header + text;
}

std::size_t CodeChunk::EstimatedTokens() const {
	const std::size_t chars = EmbedText().size();
	return static_cast<std::size_t>(static_cast<double>(chars) / kCharsPerToken);
}

Json CodeChunk::ToJson() const {
	return Json{
		{"kind",            ToString(kind)},
		{"file",            filePath},
		{"module",          moduleTag},
		{"name",            qualifiedName},
		{"start_line",      startLine},
		{"end_line",        endLine},
		{"estimated_tokens", EstimatedTokens()},
		{"text",            text},
	};
}

} // namespace agentos
