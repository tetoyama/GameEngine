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
// BrainChatCommand
// ============================
// ユーザーが / から始まる文字列を入力したときのコマンド
struct BrainChatCommand {
	bool        isCommand = false; // / で始まるか
	std::string name;              // コマンド名（/ を除いた部分）
	std::string arg;               // コマンド引数（スペース以降）
};

// ============================
// BrainTools
// ============================
// BRAIN AI用のファイルブラウジングツール
// ソースコードとアセットフォルダの閲覧をサポート
//
// 【チャットコマンド】ユーザーが / から始まる入力を行うと直接実行:
//   /help                    - 利用可能なコマンド一覧を表示
//   /list_source             - Source/ 以下のファイル一覧
//   /list_assets [path]      - Asset/ 以下のファイル・フォルダ一覧
//   /read <path>             - ファイルの内容を表示
//
// 【AIツール呼び出し書式】AIレスポンス内で使用:
//   <tool_call name="list_source_files"/>
//   <tool_call name="read_source_file" path="Source/path/to/file.cpp"/>
//   <tool_call name="list_assets" path="Asset/"/>
//
class BrainTools {
public:
	// ============================
	// チャットコマンド
	// ============================

	// ユーザー入力をチャットコマンドとして解析（/ で始まる場合のみ有効）
	static BrainChatCommand ParseChatCommand(const std::string& input);

	// チャットコマンドを実行して結果テキストを返す
	static std::string ExecuteChatCommand(const BrainChatCommand& cmd);

	// 利用可能なコマンドのヘルプ文字列を返す
	static std::string GetCommandHelp();

	// ============================
	// ファイルブラウジング
	// ============================

	// ソースファイル一覧（Source/ 以下）
	static std::string ListSourceFiles();

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
