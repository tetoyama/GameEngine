#include "BrainTools.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <functional>
#include <mutex>
#include <regex>

namespace fs = std::filesystem;

// ============================
// GetFileIndex (cached)
// ============================
// Source/ 以下のすべてのソースファイルを初回呼び出し時に走査してキャッシュする。
// LLM が抽象的な質問（"オーディオ再生はどこ？"など）に答える際に、
// 何度も list_directory を繰り返す必要なく正確なファイルパスを参照できる。

static std::string s_fileIndexCache;
static std::once_flag s_fileIndexOnce;

std::string BrainTools::GetFileIndex() {
	std::call_once(s_fileIndexOnce, []() {
		std::vector<std::string> paths;
		try {
			std::error_code ec;
			fs::path srcRoot("Source");

			if (fs::exists(srcRoot, ec) && !ec) {
				for (const auto& entry : fs::recursive_directory_iterator(srcRoot, ec)) {
					std::error_code ec2;
					if (!entry.is_regular_file(ec2) || ec2) continue;
					std::string ext = entry.path().extension().string();
					if (ext == ".cpp" || ext == ".h" || ext == ".hpp" || ext == ".c") {
						paths.push_back(entry.path().generic_string());
					}
				}
			}
		} catch (...) {
			// ファイルシステムエラーが発生しても収集済みのパスでキャッシュを構築する
		}
		std::sort(paths.begin(), paths.end());

		std::ostringstream ss;
		ss << "[Source file index — " << paths.size() << " files]\n";
		for (const auto& p : paths) {
			ss << p << "\n";
		}
		s_fileIndexCache = ss.str();
	});
	return s_fileIndexCache;
}


std::string BrainTools::ListSourceFiles() {
	return ListFilesRecursive("Source");
}

// ============================
// ListDirectory
// ============================
std::string BrainTools::ListDirectory(const std::string& path) {
	fs::path dir = path.empty() ? fs::path("Source") : fs::path(path);
	std::error_code ec;

	if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
		return "[ERROR] Directory not found: " + path;
	}

	std::ostringstream ss;
	ss << dir.generic_string() << "/\n";

	std::vector<fs::directory_entry> entries;
	for (const auto& e : fs::directory_iterator(dir, ec)) {
		entries.push_back(e);
	}
	std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
		if (a.is_directory() != b.is_directory()) return a.is_directory();
		return a.path().filename().string() < b.path().filename().string();
	});

	for (const auto& e : entries) {
		std::string name = e.path().filename().string();
		if (e.is_directory(ec)) {
			ss << "  [" << name << "]/\n";
		} else if (e.is_regular_file(ec)) {
			ss << "  " << name << "\n";
		}
	}
	return ss.str();
}

// ============================
// SearchFiles
// ============================
std::string BrainTools::SearchFiles(const std::string& query) {
	if (query.empty()) {
		return "[ERROR] query must not be empty";
	}

	// 大文字小文字を無視した部分一致検索
	std::string lowerQuery = query;
	std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(),
		[](unsigned char c){ return (char)std::tolower(c); });

	fs::path root("Source");
	std::error_code ec;
	if (!fs::exists(root, ec)) {
		return "[ERROR] Source directory not found";
	}

	std::ostringstream ss;
	int found = 0;
	for (const auto& entry : fs::recursive_directory_iterator(root, ec)) {
		if (!entry.is_regular_file(ec)) continue;
		std::string name = entry.path().filename().string();
		std::string lowerName = name;
		std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
			[](unsigned char c){ return (char)std::tolower(c); });
		if (lowerName.find(lowerQuery) != std::string::npos) {
			ss << entry.path().generic_string() << "\n";
			++found;
		}
	}

	if (found == 0) {
		return "[No files found matching: " + query + "]";
	}
	return ss.str();
}

// ============================
// GrepSource
// ============================
std::string BrainTools::GrepSource(const std::string& keyword) {
	if (keyword.empty()) {
		return "[ERROR] keyword must not be empty";
	}

	// 大文字小文字を無視した検索
	std::string lowerKeyword = keyword;
	std::transform(lowerKeyword.begin(), lowerKeyword.end(), lowerKeyword.begin(),
		[](unsigned char c){ return (char)std::tolower(c); });

	fs::path root("Source");
	std::error_code ec;
	if (!fs::exists(root, ec)) {
		return "[ERROR] Source directory not found";
	}

	std::ostringstream ss;
	int matchCount = 0;
	static constexpr int kMaxMatches = 30;      // 結果が多すぎる場合に切り詰め
	static constexpr int kContextLines = 2;     // マッチ前後の表示行数

	for (const auto& entry : fs::recursive_directory_iterator(root, ec)) {
		if (!entry.is_regular_file(ec)) continue;
		std::string ext = entry.path().extension().string();
		// ソースファイルのみ対象
		if (ext != ".cpp" && ext != ".h" && ext != ".hpp" && ext != ".c") continue;

		std::ifstream ifs(entry.path());
		if (!ifs.is_open()) continue;

		std::vector<std::string> lines;
		std::string line;
		while (std::getline(ifs, line)) {
			lines.push_back(line);
		}

		for (int i = 0; i < (int)lines.size(); ++i) {
			std::string lowerLine = lines[i];
			std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(),
				[](unsigned char c){ return (char)std::tolower(c); });
			if (lowerLine.find(lowerKeyword) == std::string::npos) continue;

			if (matchCount >= kMaxMatches) {
				ss << "[... more matches omitted (limit " << kMaxMatches << ") ...]\n";
				return ss.str();
			}

			ss << entry.path().generic_string() << ":" << (i + 1) << "\n";
			int startLine = (std::max)(0, i - kContextLines);
			int endLine   = (std::min)((int)lines.size() - 1, i + kContextLines);
			for (int j = startLine; j <= endLine; ++j) {
				ss << (j == i ? ">> " : "   ") << (j + 1) << ": " << lines[j] << "\n";
			}
			ss << "\n";
			++matchCount;
		}
	}

	if (matchCount == 0) {
		return "[No matches found for: " + keyword + "]";
	}
	return ss.str();
}


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
		R"re(<tool_call\s+name="([^"]+)"(?:\s+path="([^"]*)")?\s*(?:/>|>(?:[\s\S]*?)</tool_call>))re"
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
	if (call.name == "get_file_index") {
		return GetFileIndex();
	}
	if (call.name == "list_source_files") {
		return ListSourceFiles();
	}
	if (call.name == "list_directory") {
		return ListDirectory(call.path);
	}
	if (call.name == "search_files") {
		return SearchFiles(call.path);
	}
	if (call.name == "grep_source") {
		return GrepSource(call.path);
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
		R"re(<tool_call\s+name="([^"]+)"(?:\s+path="([^"]*)")?\s*(?:/>|>(?:[\s\S]*?)</tool_call>))re"
	);
	return std::regex_replace(response, re, "");
}
