#include "SourceIndex.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <thread>
#include <mutex>
#include <atomic>
#include <regex>

namespace fs = std::filesystem;

// ============================
// File-level state
// ============================

static std::mutex              s_idxMutex;
static std::atomic<bool>       s_idxReady{false};
static std::atomic<bool>       s_idxBuilding{false};

// filepath → comma-separated keywords (persisted)
static std::map<std::string, std::string> s_fileKeywords;

// lowercase keyword → set of filepaths (runtime reverse index)
static std::map<std::string, std::set<std::string>> s_kwToFiles;

static std::string s_statusMsg = "Index not started";

static constexpr const char* kCacheFile    = "Asset/BRAIN/cache/source_index.txt";
static constexpr const char* kCacheVersion = "# source_index v1";

// ============================
// Internal helpers
// ============================

static std::string IdxToLower(const std::string& s) {
	std::string r = s;
	std::transform(r.begin(), r.end(), r.begin(),
		[](unsigned char c){ return static_cast<char>(std::tolower(c)); });
	return r;
}

static void SetStatus(const std::string& msg) {
	std::lock_guard<std::mutex> lock(s_idxMutex);
	s_statusMsg = msg;
}

// ファイルの最終更新時刻を秒単位の整数として返す
static int64_t GetMTime(const fs::path& p) {
	std::error_code ec;
	auto t = fs::last_write_time(p, ec);
	if (ec) return 0;
	// file_time_type は実装依存の epoch を持つが、比較に使うだけなので十分
	return static_cast<int64_t>(t.time_since_epoch().count());
}

// ソースファイルからキーワード（クラス名・構造体名・大文字始まりの識別子）を抽出。
// ファイルスコープに置くことで複数回の構築を避ける。
static const std::regex kClassRe(
	R"(\b(?:class|struct|enum(?:\s+class)?)\s+(\w+))");
static const std::regex kUpperRe(
	R"(\b([A-Z][A-Za-z0-9_]{2,})\b)");

static std::string ExtractKeywords(const fs::path& p) {
	std::ifstream ifs(p);
	if (!ifs.is_open()) return "";

	std::set<std::string> kws;

	// ファイルステム自体をキーワードに（例: "AudioSystem"）
	std::string stem = p.stem().string();
	if (!stem.empty()) kws.insert(stem);

	std::string line;
	bool inBlockComment = false;
	while (std::getline(ifs, line)) {
		// ブロックコメント・行コメントを除去（簡易）
		for (size_t i = 0; i < line.size(); ) {
			if (inBlockComment) {
				auto endPos = line.find("*/", i);
				if (endPos == std::string::npos) {
					line.clear();
					break;
				}
				line.erase(0, endPos + 2);
				i = 0;
				inBlockComment = false;
			} else {
				auto lc = line.find("//", i);
				auto bc = line.find("/*", i);
				if (bc != std::string::npos &&
				    (lc == std::string::npos || bc < lc)) {
					line.erase(bc);
					inBlockComment = true;
					break;
				}
				if (lc != std::string::npos) {
					line.erase(lc);
				}
				break;
			}
		}

		if (line.empty()) continue;

		auto it  = std::sregex_iterator(line.begin(), line.end(), kClassRe);
		auto end = std::sregex_iterator();
		for (; it != end; ++it) {
			kws.insert((*it)[1].str());
		}
		auto it2 = std::sregex_iterator(line.begin(), line.end(), kUpperRe);
		for (; it2 != end; ++it2) {
			kws.insert((*it2)[1].str());
		}
	}

	std::ostringstream ss;
	bool first = true;
	for (const auto& kw : kws) {
		if (!first) ss << ',';
		ss << kw;
		first = false;
	}
	return ss.str();
}

// ============================
// Cache I/O
// ============================

// キャッシュファイルを読み込む。戻り値: filepath → {mtime, keywords}
static std::map<std::string, std::pair<int64_t, std::string>> LoadDiskCache() {
	std::map<std::string, std::pair<int64_t, std::string>> cache;
	std::ifstream ifs(kCacheFile);
	if (!ifs.is_open()) return cache;

	std::string line;
	if (!std::getline(ifs, line) || line != kCacheVersion) {
		return cache;  // バージョン不一致 → 再構築
	}

	while (std::getline(ifs, line)) {
		if (line.empty() || line[0] == '#') continue;
		// フォーマット: PATH\tMTIME\tKEYWORDS
		auto t1 = line.find('\t');
		if (t1 == std::string::npos) continue;
		auto t2 = line.find('\t', t1 + 1);
		if (t2 == std::string::npos) continue;

		std::string path     = line.substr(0, t1);
		std::string mtimeStr = line.substr(t1 + 1, t2 - t1 - 1);
		std::string keywords = line.substr(t2 + 1);

		try {
			int64_t mtime = std::stoll(mtimeStr);
			cache[path]   = {mtime, keywords};
		} catch (...) {}
	}
	return cache;
}

// キャッシュファイルをディスクに書き込む
static void SaveDiskCache(
	const std::map<std::string, std::pair<int64_t, std::string>>& data)
{
	fs::path cacheDir = fs::path(kCacheFile).parent_path();
	std::error_code ec;
	fs::create_directories(cacheDir, ec);

	std::ofstream ofs(kCacheFile);
	if (!ofs.is_open()) return;

	ofs << kCacheVersion << '\n';
	for (const auto& [path, entry] : data) {
		ofs << path << '\t' << entry.first << '\t' << entry.second << '\n';
	}
}

// ============================
// BuildAsync
// ============================
void SourceIndex::BuildAsync() {
	bool expected = false;
	if (!s_idxBuilding.compare_exchange_strong(expected, true)) {
		return;  // 既にビルド中
	}

	std::thread([]() {
		try {
		SetStatus("Building index...");

		// ---- 1. ディスクキャッシュをロード ----
		auto diskCache = LoadDiskCache();  // path → {mtime, keywords}

		// ---- 2. Source/ を走査して現在のファイル一覧+mtime を収集 ----
		std::map<std::string, int64_t> currentFiles;  // path → mtime
		try {
			fs::path srcRoot("Source");
			std::error_code ec;
			if (fs::exists(srcRoot, ec) && !ec) {
				for (const auto& entry :
				     fs::recursive_directory_iterator(srcRoot, ec)) {
					std::error_code ec2;
					if (!entry.is_regular_file(ec2) || ec2) continue;
					std::string ext = entry.path().extension().string();
					if (ext != ".cpp" && ext != ".h" &&
					    ext != ".hpp" && ext != ".c") {
						continue;
					}
					currentFiles[entry.path().generic_string()] =
						GetMTime(entry.path());
				}
			}
		} catch (...) {}

		// ---- 3. 差分処理：変更・追加のみ再抽出、削除は無視 ----
		std::map<std::string, std::pair<int64_t, std::string>> newCache;
		int rebuilt = 0;
		int reused  = 0;

		for (const auto& [path, mtime] : currentFiles) {
			auto it = diskCache.find(path);
			if (it != diskCache.end() && it->second.first == mtime) {
				// キャッシュ有効：そのまま再利用
				newCache[path] = it->second;
				++reused;
			} else {
				// 新規または変更：キーワードを再抽出
				std::string kws;
				try {
					kws = ExtractKeywords(fs::path(path));
				} catch (...) {}
				newCache[path] = {mtime, kws};
				++rebuilt;
			}
		}
		// 削除されたファイルは newCache に入らないため自然にドロップされる

		// ---- 4. ディスクに保存 ----
		try {
			SaveDiskCache(newCache);
		} catch (...) {}

		// ---- 5. インメモリ逆インデックスをビルド ----
		std::map<std::string, std::string>           fileKws;
		std::map<std::string, std::set<std::string>> rev;

		for (const auto& [path, entry] : newCache) {
			fileKws[path] = entry.second;

			// ファイルステム（例: "AudioSystem"）
			std::string stemLower = IdxToLower(fs::path(path).stem().string());
			rev[stemLower].insert(path);

			if (entry.second.empty()) continue;
			std::istringstream ss(entry.second);
			std::string kw;
			while (std::getline(ss, kw, ',')) {
				if (!kw.empty()) {
					rev[IdxToLower(kw)].insert(path);
				}
			}
		}

		{
			std::lock_guard<std::mutex> lock(s_idxMutex);
			s_fileKeywords = std::move(fileKws);
			s_kwToFiles    = std::move(rev);
			s_statusMsg    = "[Index ready: " +
				std::to_string(currentFiles.size()) + " files, " +
				std::to_string(rebuilt) + " rebuilt, " +
				std::to_string(reused) + " from cache]";
		}

		s_idxReady.store(true);
		} catch (...) {
			SetStatus("[Index build failed]");
		}
		s_idxBuilding.store(false);
	}).detach();
}

// ============================
// SearchByKeyword
// ============================
std::string SourceIndex::SearchByKeyword(const std::string& keyword) {
	if (keyword.empty()) {
		return "[ERROR] keyword must not be empty";
	}

	if (!s_idxReady.load()) {
		std::lock_guard<std::mutex> lock(s_idxMutex);
		return "[Index not ready — " + s_statusMsg + "]";
	}

	std::string lowerKw = IdxToLower(keyword);
	std::set<std::string> results;
	static constexpr size_t kMaxResults = 50;

	{
		std::lock_guard<std::mutex> lock(s_idxMutex);
		for (const auto& [kw, paths] : s_kwToFiles) {
			if (kw.find(lowerKw) != std::string::npos) {
				results.insert(paths.begin(), paths.end());
				if (results.size() >= kMaxResults) break;
			}
		}
	}

	if (results.empty()) {
		return "[No files found in index for: " + keyword + "]";
	}

	std::ostringstream ss;
	bool capped = results.size() >= kMaxResults;
	ss << "[" << results.size() << (capped ? "+ " : " ")
	   << "file(s) matching \"" << keyword << "\"]\n";
	for (const auto& p : results) {
		ss << p << "\n";
	}
	if (capped) {
		ss << "[... results capped at " << kMaxResults << " — use a more specific keyword]\n";
	}
	return ss.str();
}

// ============================
// GetStatus
// ============================
std::string SourceIndex::GetStatus() {
	std::lock_guard<std::mutex> lock(s_idxMutex);
	return s_statusMsg;
}
