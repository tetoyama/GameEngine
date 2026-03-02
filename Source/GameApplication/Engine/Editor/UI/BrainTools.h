#pragma once

#include <string>
#include <vector>

// ============================
// BrainToolCall
// ============================
// AIレスポンスから解析されたツール呼び出し
struct BrainToolCall {
	std::string name; // ツール名
	std::string path; // パス引数（オプション）
};

// ============================
// BrainTools
// ============================
// BRAIN AI用のファイルブラウジングツール
// ソースコードとアセットフォルダの閲覧をサポート
//
// 【AIツール呼び出し書式】AIレスポンス内で使用:
//   <tool_call name="list_source_files"/>
//   <tool_call name="list_directory" path="Source/some/path/"/>
//   <tool_call name="search_files" path="FileName"/>
//   <tool_call name="grep_source" path="keyword"/>
//   <tool_call name="read_source_file" path="Source/path/to/file.cpp"/>
//   <tool_call name="list_assets" path="Asset/"/>
//
// ツールが呼び出されると、チャットログに "> コマンド名 引数" + 結果が表示される。
//
class BrainTools {
public:
	// ============================
	// ファイルブラウジング
	// ============================

	// ソースファイル一覧（Source/ 以下）
	static std::string ListSourceFiles();

	// ディレクトリの直下一覧（非再帰、一段のみ）
	static std::string ListDirectory(const std::string& path);

	// ファイル名によるファイル検索（Source/ 以下、部分一致）
	static std::string SearchFiles(const std::string& query);

	// ソースコード内容のキーワード検索（Source/ 以下）
	static std::string GrepSource(const std::string& keyword);

	// アセット一覧（Asset/ 以下、サブパス指定可能）
	static std::string ListAssets(const std::string& subPath = "");

	// ファイル読み込み（ソースコードまたはテキストアセット）
	static std::string ReadFile(const std::string& path);

	// ============================
	// AIツール呼び出し
	// ============================

	// AIレスポンスからツール呼び出しを解析
	static std::vector<BrainToolCall> ParseToolCalls(const std::string& response);

	// ツール呼び出しを実行して結果を返す
	static std::string ExecuteToolCall(const BrainToolCall& call);

	// レスポンスからツール呼び出しタグを除去
	static std::string StripToolCalls(const std::string& response);

private:
	// ファイル読み込みサイズ上限（32KB）
	static constexpr size_t MAX_FILE_SIZE = 32 * 1024;

	static std::string ListFilesRecursive(const std::string& rootPath, int maxDepth = 10);
};
