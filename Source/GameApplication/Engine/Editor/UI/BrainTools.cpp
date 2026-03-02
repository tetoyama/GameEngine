#include "BrainTools.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <functional>
#include <regex>

namespace fs = std::filesystem;

// ============================
// ParseChatCommand
// ============================
BrainChatCommand BrainTools::ParseChatCommand(const std::string& input) {
	BrainChatCommand result;
	if (input.empty() || input[0] != '/') {
		return result;
	}
	result.isCommand = true;

	size_t spacePos = input.find(' ');
	if (spacePos == std::string::npos) {
		result.name = input.substr(1);
	} else {
		result.name = input.substr(1, spacePos - 1);
		size_t argStart = spacePos + 1;
		while (argStart < input.size() && input[argStart] == ' ') ++argStart;
		result.arg = input.substr(argStart);
	}

	// コマンド名を小文字に正規化
	std::transform(result.name.begin(), result.name.end(),
	               result.name.begin(),
	               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return result;
}

// ============================
// ExecuteChatCommand
// ============================
std::string BrainTools::ExecuteChatCommand(const BrainChatCommand& cmd) {
	if (!cmd.isCommand) return "";

	if (cmd.name == "help" || cmd.name == "h" || cmd.name == "?") {
		return GetCommandHelp();
	}
	if (cmd.name == "list_source" || cmd.name == "ls") {
		return ListSourceFiles();
	}
	if (cmd.name == "list_assets" || cmd.name == "la") {
		return ListAssets(cmd.arg);
	}
	if (cmd.name == "read" || cmd.name == "r") {
		if (cmd.arg.empty()) {
			return "[ERROR] Usage: /read <path>\n"
			       "  例: /read Source/GameApplication/Engine/Editor/UI/BRAIN.cpp";
		}
		return ReadFile(cmd.arg);
	}

	return "[ERROR] Unknown command: /" + cmd.name +
	       "\nType /help for a list of available commands.";
}

// ============================
// GetCommandHelp
// ============================
std::string BrainTools::GetCommandHelp() {
	return
		"=== B.R.A.I.N. Chat Commands ===\n"
		"\n"
		"  /help                     このヘルプを表示\n"
		"  /list_source              ソースコード一覧 (Source/)\n"
		"  /list_assets [path]       アセット一覧 (デフォルト: Asset/)\n"
		"  /read <path>              ファイル内容を表示 (Source/ か Asset/ のみ)\n"
		"\n"
		"エイリアス:\n"
		"  /ls  = /list_source\n"
		"  /la  = /list_assets\n"
		"  /r   = /read\n"
		"\n"
		"例:\n"
		"  /list_source\n"
		"  /list_assets Asset/Model/\n"
		"  /read Source/GameApplication/Engine/Editor/UI/BRAIN.cpp\n"
		"\n"
		"AIに直接質問することもできます。\n"
		"AIは必要に応じて自動的にツールを使用します。";
}

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
