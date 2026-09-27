// =======================================================================
//
// CodeIndexBuilder.cpp
//
// =======================================================================
#include "CodeIndexBuilder.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

namespace agentos {

namespace {

constexpr std::size_t kContextLimitSmall = 2048; // EmbeddingGemma-300M
constexpr std::size_t kContextLimitLarge = 8192; // BGE-M3 等

std::string NormalizeSeparators(std::string path) {
	for(char& c : path) {
		if(c == '\\') c = '/';
	}
	return path;
}

bool HasAnyExtension(const std::string& path, const std::vector<std::string>& extensions) {
	for(const std::string& ext : extensions) {
		if(path.size() < ext.size()) continue;
		if(path.compare(path.size() - ext.size(), ext.size(), ext) == 0) return true;
	}
	return false;
}

bool IsExcluded(const std::string& path, const std::vector<std::string>& excludes) {
	for(const std::string& needle : excludes) {
		if(!needle.empty() && path.find(needle) != std::string::npos) return true;
	}
	return false;
}

bool ReadWholeFile(const std::filesystem::path& path, std::string* out) {
	std::ifstream in(path, std::ios::binary);
	if(!in) return false;
	std::ostringstream ss;
	ss << in.rdbuf();
	*out = ss.str();
	return true;
}

} // namespace

// =======================================================================
// CodeIndexReport::ToJson
// =======================================================================
Json CodeIndexReport::ToJson() const {
	return Json{
		{"files_scanned",          filesScanned},
		{"files_skipped",          filesSkipped},
		{"files_failed",           filesFailed},
		{"function_chunks",        stats.functionCount},
		{"type_chunks",            stats.typeCount},
		{"declarations_skipped",   stats.forwardDeclarationCount},
		{"unterminated",           stats.unterminatedCount},
		{"total_chunks",           totalChunks},
		{"total_estimated_tokens", totalEstimatedTokens},
		{"over_2048_tokens",       over2048Tokens},
		{"over_8192_tokens",       over8192Tokens},
	};
}

// =======================================================================
// AccumulateChunks
// =======================================================================
void AccumulateChunks(
	const std::string& relativePath,
	const std::string& source,
	std::vector<CodeChunk>* outChunks,
	CodeIndexReport* outReport) {

	ParseStats stats;
	std::vector<CodeChunk> parsed = ParseSourceFile(relativePath, source, &stats);

	if(outReport) {
		outReport->stats.Merge(stats);
		for(const CodeChunk& chunk : parsed) {
			const std::size_t tokens = chunk.EstimatedTokens();
			outReport->totalEstimatedTokens += tokens;
			if(tokens > kContextLimitSmall) ++outReport->over2048Tokens;
			if(tokens > kContextLimitLarge) ++outReport->over8192Tokens;
		}
		outReport->totalChunks += parsed.size();
	}

	if(outChunks) {
		outChunks->insert(
			outChunks->end(),
			std::make_move_iterator(parsed.begin()),
			std::make_move_iterator(parsed.end()));
	}
}

// =======================================================================
// EnumerateSourceFiles
// =======================================================================
Result EnumerateSourceFiles(
	const CodeIndexOptions& options,
	std::vector<std::string>* outPaths,
	int* outSkipped) {

	namespace fs = std::filesystem;

	if(outPaths == nullptr) {
		return Result::Fail("EnumerateSourceFiles: outPathsがnullptr");
	}
	outPaths->clear();

	std::error_code ec;
	if(!fs::exists(options.root, ec) || ec) {
		return Result::Fail("EnumerateSourceFiles: root が存在しない: " + options.root);
	}

	fs::recursive_directory_iterator it(
		options.root, fs::directory_options::skip_permission_denied, ec);
	if(ec) {
		return Result::Fail("EnumerateSourceFiles: 走査に失敗: " + ec.message());
	}

	int skipped = 0;
	for(const fs::directory_entry& entry : it) {
		if(!entry.is_regular_file(ec) || ec) {
			ec.clear();
			continue;
		}
		const std::string path = NormalizeSeparators(entry.path().generic_string());

		if(!HasAnyExtension(path, options.extensions)) continue;
		if(IsExcluded(path, options.excludeSubstrings)) {
			++skipped;
			continue;
		}
		outPaths->push_back(path);
	}

	// 実行ごとに結果が変わらないよう順序を固定する。
	// 索引の差分比較や再現性のために効いてくる。
	std::sort(outPaths->begin(), outPaths->end());

	if(outSkipped) *outSkipped = skipped;
	return Result::Ok();
}

// =======================================================================
// ReadSourceFile
// =======================================================================
bool ReadSourceFile(const std::string& path, std::string* outContent) {
	if(outContent == nullptr) return false;
	return ReadWholeFile(std::filesystem::path(path), outContent);
}

// =======================================================================
// BuildCodeIndex
// =======================================================================
Result BuildCodeIndex(
	const CodeIndexOptions& options,
	std::vector<CodeChunk>* outChunks,
	CodeIndexReport* outReport) {

	std::vector<std::string> targets;
	int skipped = 0;
	const Result r = EnumerateSourceFiles(options, &targets, &skipped);
	if(!r) return r;

	if(outReport) outReport->filesSkipped += skipped;

	for(const std::string& path : targets) {
		std::string source;
		if(!ReadSourceFile(path, &source)) {
			if(outReport) ++outReport->filesFailed;
			continue;
		}
		if(outReport) ++outReport->filesScanned;
		AccumulateChunks(path, source, outChunks, outReport);
	}

	return Result::Ok();
}

// =======================================================================
// ChunksToJson
// =======================================================================
Json ChunksToJson(const std::vector<CodeChunk>& chunks) {
	Json array = Json::array();
	for(const CodeChunk& chunk : chunks) {
		array.push_back(chunk.ToJson());
	}
	return array;
}

} // namespace agentos
