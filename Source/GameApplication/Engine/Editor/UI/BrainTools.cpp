#include "BrainTools.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <functional>
#include <regex>

namespace fs = std::filesystem;

// ============================
// ListSourceFiles
// ============================
std::string BrainTools::ListSourceFiles() {
	return ListFilesRecursive("Source");
}

// ============================
// ListAssets
// ============================
std::string BrainTools::ListAssets(const std::string& subPath) {
	std::string path = subPath.empty() ? "Asset" : subPath;
	// アセットは深さ4階層まで
	return ListFilesRecursive(path, 4);
}

// ============================
// ReadFile
// ============================
std::string BrainTools::ReadFile(const std::string& path) {
	// パス正規化（/ または \ を統一）
	fs::path p = path;
	std::string normalized = p.lexically_normal().generic_string();

	// パストラバーサル防止（../ を禁止）
	if (normalized.find("..") != std::string::npos) {
		return "[ERROR] Invalid path: path traversal is not allowed.";
	}

	// Source/ または Asset/ 以下のみアクセス許可
	bool isSource = (normalized.size() >= 7 && normalized.substr(0, 7) == "Source/");
	bool isAsset  = (normalized.size() >= 6 && normalized.substr(0, 6) == "Asset/");
	if (!isSource && !isAsset) {
		return "[ERROR] Access denied: only paths under Source/ and Asset/ are allowed.";
	}

	std::error_code ec;
	if (!fs::exists(p, ec) || !fs::is_regular_file(p, ec)) {
		return "[ERROR] File not found: " + path;
	}

	auto fileSize = fs::file_size(p, ec);
	if (ec || fileSize > MAX_FILE_SIZE) {
		return "[ERROR] File is too large (limit: 32KB): " + path;
	}

	std::ifstream ifs(p);
	if (!ifs.is_open()) {
		return "[ERROR] Cannot open file: " + path;
	}

	std::ostringstream ss;
	ss << ifs.rdbuf();
	return ss.str();
}

// ============================
// ListFilesRecursive
// ============================
std::string BrainTools::ListFilesRecursive(const std::string& rootPath, int maxDepth) {
	fs::path root = rootPath;
	std::error_code ec;

	if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) {
		return "[ERROR] Directory not found: " + rootPath;
	}

	std::ostringstream ss;
	ss << rootPath << "/\n";

	std::function<void(const fs::path&, int, const std::string&)> walk;
	walk = [&](const fs::path& dir, int depth, const std::string& indent) {
		if (depth > maxDepth) return;

		std::vector<fs::directory_entry> entries;
		for (const auto& e : fs::directory_iterator(dir, ec)) {
			entries.push_back(e);
		}

		// ディレクトリ優先でアルファベット順にソート
		std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
			if (a.is_directory() != b.is_directory()) return a.is_directory();
			return a.path().filename().string() < b.path().filename().string();
		});

		for (const auto& e : entries) {
			std::string name = e.path().filename().string();
			if (e.is_directory(ec)) {
				ss << indent << "[" << name << "]/\n";
				walk(e.path(), depth + 1, indent + "  ");
			} else if (e.is_regular_file(ec)) {
				ss << indent << name << "\n";
			}
		}
	};

	walk(root, 1, "  ");
	return ss.str();
}

// ============================
// ParseToolCalls
// ============================
std::vector<BrainToolCall> BrainTools::ParseToolCalls(const std::string& response) {
	std::vector<BrainToolCall> calls;

	// 自己終了タグ:  <tool_call name="xxx"/>
	// または属性付き: <tool_call name="xxx" path="yyy"/>
	// または対タグ:   <tool_call name="xxx"></tool_call>
	std::regex re(
		R"(<tool_call\s+name="([^"]+)"(?:\s+path="([^"]*)")?\s*(?:/>|>(?:[\s\S]*?)</tool_call>))"
	);

	auto begin = std::sregex_iterator(response.begin(), response.end(), re);
	auto end   = std::sregex_iterator();

	for (auto it = begin; it != end; ++it) {
		BrainToolCall call;
		call.name = (*it)[1].str();
		call.path = (*it)[2].str();
		calls.push_back(call);
	}

	return calls;
}

// ============================
// ExecuteToolCall
// ============================
std::string BrainTools::ExecuteToolCall(const BrainToolCall& call) {
	if (call.name == "list_source_files") {
		return ListSourceFiles();
	}
	if (call.name == "read_source_file" || call.name == "read_file") {
		if (call.path.empty()) {
			return "[ERROR] path attribute is required for " + call.name;
		}
		return ReadFile(call.path);
	}
	if (call.name == "list_assets") {
		return ListAssets(call.path);
	}
	return "[ERROR] Unknown tool: " + call.name;
}

// ============================
// StripToolCalls
// ============================
std::string BrainTools::StripToolCalls(const std::string& response) {
	// ParseToolCalls と同じパターンを使用して一貫性を保つ
	std::regex re(
		R"(<tool_call\s+name="([^"]+)"(?:\s+path="([^"]*)")?\s*(?:/>|>(?:[\s\S]*?)</tool_call>))"
	);
	return std::regex_replace(response, re, "");
}
