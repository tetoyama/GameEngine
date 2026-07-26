// =======================================================================
//
// CodeSearchTool.cpp
//
// =======================================================================
#include "CodeSearchTool.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

namespace agentos {

namespace {

// ---------------------------------
// ツール名について（重要）
// ---------------------------------
// "CodeSearch" は既存のAgentOS側が前提としている名前であり、勝手に変えてはいけない。
//   - RetrievalWorker.cpp の kDiscoveryTools に登録されている
//   - PlannerAgent / CriticAgent / Orchestrator がTask種別として認識する
//
// ここを "SearchCode" と命名した結果、Discovery Toolと認識されず
// 「引数は依存Evidenceに存在する文字列であること」という束縛が掛かり、
// 検索クエリが毎回 GroundingRejected で弾かれた（実機ログで確認）。
// 検索クエリは検索の起点なので、過去のEvidenceに含まれているはずがない。
constexpr const char* kCodeSearchToolName = "CodeSearch";
constexpr const char* kSymbolInfoToolName = "GetSymbolInfo";

// CodeSearchが1件あたりに載せる本文の最大文字数。
// 索引には17,000トークン超のチャンクも実在するため、
// 素で全文を返すとプロンプトが一撃で埋まる。
// 全文が要るときは GetSymbolInfo を使う。
constexpr std::size_t kSearchSnippetChars = 1200;

// GetSymbolInfoの上限。狙って1件を引く用途なので大きめに取る。
constexpr std::size_t kSymbolBodyChars = 8000;

constexpr int kMaxTopK = 10;
constexpr int kDefaultTopK = 5;

std::string Truncate(const std::string& text, std::size_t limit) {
	if(text.size() <= limit) return text;

	// UTF-8の途中で切らないよう、継続バイト(10xxxxxx)を避けて後退する。
	std::size_t cut = limit;
	while(cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80) --cut;
	return text.substr(0, cut) + "\n... (truncated)";
}

Json HitToJson(const CodeSearchResult& hit, std::size_t snippetLimit, bool withScore) {
	Json item = Json::object({
		{"name", hit.qualifiedName},
		{"kind", hit.kind},
		{"module", hit.moduleTag},
		{"file", hit.filePath},
		{"start_line", hit.startLine},
		{"end_line", hit.endLine},
		{"code", Truncate(hit.text, snippetLimit)},
	});
	if(withScore) item["score"] = hit.score;
	return item;
}

// 索引が使えない理由を返す。使えるなら空文字列。
std::string IndexUnavailableReason(const CodeIndexService& service) {
	if(service.IsReady()) return {};

	const CodeIndexStatus status = service.GetStatus();
	if(status.state == CodeIndexState::Failed) {
		return "コード索引の構築に失敗している: " + status.error;
	}
	return "コード索引を構築中（" +
		std::to_string(status.filesProcessed) + "/" +
		std::to_string(status.filesTotal) + " ファイル）。完了後に再試行のこと。";
}

// ---------------------------------
// CodeSearch
// ---------------------------------
class CodeSearchTool final : public ICommandExecutor {
public:
	explicit CodeSearchTool(CodeIndexService& service)
		: m_service(service)
		, m_descriptor{
			kCodeSearchToolName,
			"自作エンジンのソースコードから、関数や型の定義を検索する。"
			"シンボル名が分かっているときも、日本語の説明で探すときも使える。"
			"戻り値は定義位置(file / start_line / end_line)とコード抜粋。"
			"名前が正確に分かっていて全文が欲しい場合は GetSymbolInfo を使うこと。",
			PermissionLevel::Read,
			Json::object({
				{"query", Json::object({
					{"type", "string"},
					{"required", true},
					{"description", "検索したい内容。シンボル名でも日本語の説明でもよい。"},
				})},
				{"file", Json::object({
					{"type", "string"},
					{"required", false},
					{"description", "結果を絞り込むファイルパスの一部（例: \"Store/SqliteDb\"）"},
				})},
				{"topK", Json::object({
					{"type", "integer"},
					{"required", false},
					{"description", "返す件数（既定5、最大10）"},
				})},
			})
		} {}

	const ToolDescriptor& Descriptor() const override { return m_descriptor; }

	Result CheckPrecondition(const Json& arguments) override {
		// 索引が未完成なら明示的に弾く。黙って0件を返すと、
		// モデルが「そんなコードは存在しない」と誤って結論づけるため。
		const std::string unavailable = IndexUnavailableReason(m_service);
		if(!unavailable.empty()) return Result::Fail(unavailable);

		if(!arguments.contains("query")) return Result::Fail("query は必須");
		if(!arguments.at("query").is_string() ||
		   arguments.at("query").get<std::string>().empty()) {
			return Result::Fail("query は空でない文字列であること");
		}
		return Result::Ok();
	}

	CommandResult Execute(const Json& arguments) override {
		const std::string query = arguments.value("query", std::string());
		if(query.empty()) {
			return CommandResult::Fail(CommandStatus::SchemaRejected, "query が空");
		}
		const std::string fileFilter = arguments.value("file", std::string());

		int topK = arguments.value("topK", kDefaultTopK);
		topK = std::clamp(topK, 1, kMaxTopK);

		const std::vector<CodeSearchResult> hits =
			m_service.Search(query, static_cast<std::size_t>(topK), fileFilter);

		Json results = Json::array();
		for(const CodeSearchResult& hit : hits) {
			results.push_back(HitToJson(hit, kSearchSnippetChars, true));
		}

		Json payload = Json::object({
			{"query", query},
			{"count", hits.size()},
			{"results", std::move(results)},
		});
		if(!fileFilter.empty()) payload["file_filter"] = fileFilter;

		// claimはEvidenceの見出しになる。
		// 0件を「存在しない」と言い切らないのは、索引が
		// クラス外定義の関数と型宣言しか対象にしていないため。
		if(hits.empty()) {
			payload["claim"] =
				"「" + query + "」に一致するコードは索引内に見つからなかった。"
				"索引はクラス外定義の関数と型宣言のみを対象としており、"
				"自由関数やマクロ生成の定義は含まれない。";
		} else {
			payload["claim"] =
				"「" + query + "」に関連するコードを" +
				std::to_string(hits.size()) + "件見つけた。最有力は " +
				hits.front().qualifiedName + "（" + hits.front().filePath + ":" +
				std::to_string(hits.front().startLine) + "）。";
		}
		return CommandResult::Ok(std::move(payload));
	}

private:
	CodeIndexService& m_service;
	ToolDescriptor m_descriptor;
};

// ---------------------------------
// GetSymbolInfo
// ---------------------------------
// シンボル名を指定して定義の全文を取る。
//
// このツールを用意したのは、実機ログでモデルが存在しない GetSymbolInfo を
// 4回呼ぼうとして ToolAllowlistRejected になっていたため。
// 「名前が分かっているものをピンポイントで引く」という要求は自然であり、
// ランキング検索(CodeSearch)とは別物なので、素直に用意する方が良い。
class GetSymbolInfoTool final : public ICommandExecutor {
public:
	explicit GetSymbolInfoTool(CodeIndexService& service)
		: m_service(service)
		, m_descriptor{
			kSymbolInfoToolName,
			"シンボル名を指定して、その定義の全文と位置を取得する。"
			"名前が正確に分かっている場合はCodeSearchより確実で、コードを切り詰めずに返す。"
			"名前が曖昧・未知の場合はCodeSearchを使うこと。",
			PermissionLevel::Read,
			Json::object({
				{"name", Json::object({
					{"type", "string"},
					{"required", true},
					{"description",
					 "シンボル名。修飾付き(agentos::SqliteDb::Prepare)でも末尾のみ(Prepare)でもよい。"},
				})},
				{"file", Json::object({
					{"type", "string"},
					{"required", false},
					{"description", "同名シンボルを絞り込むファイルパスの一部"},
				})},
			})
		} {}

	const ToolDescriptor& Descriptor() const override { return m_descriptor; }

	Result CheckPrecondition(const Json& arguments) override {
		const std::string unavailable = IndexUnavailableReason(m_service);
		if(!unavailable.empty()) return Result::Fail(unavailable);

		if(!arguments.contains("name")) return Result::Fail("name は必須");
		if(!arguments.at("name").is_string() ||
		   arguments.at("name").get<std::string>().empty()) {
			return Result::Fail("name は空でない文字列であること");
		}
		return Result::Ok();
	}

	CommandResult Execute(const Json& arguments) override {
		const std::string name = arguments.value("name", std::string());
		if(name.empty()) {
			return CommandResult::Fail(CommandStatus::SchemaRejected, "name が空");
		}
		const std::string fileFilter = arguments.value("file", std::string());

		// 同名のオーバーロードや別ファイルの同名型があるため複数返しうる。
		const std::vector<CodeSearchResult> hits =
			m_service.FindSymbol(name, fileFilter, 5);

		Json results = Json::array();
		for(const CodeSearchResult& hit : hits) {
			results.push_back(HitToJson(hit, kSymbolBodyChars, false));
		}

		Json payload = Json::object({
			{"name", name},
			{"count", hits.size()},
			{"results", std::move(results)},
		});
		if(!fileFilter.empty()) payload["file_filter"] = fileFilter;

		if(hits.empty()) {
			payload["claim"] =
				"シンボル「" + name + "」の定義は索引内に見つからなかった。"
				"綴りが違うか、索引の対象外（自由関数・マクロ生成・Backends配下）の可能性がある。"
				"CodeSearchで曖昧検索を試すこと。";
		} else {
			const CodeSearchResult& top = hits.front();
			payload["claim"] =
				"シンボル「" + name + "」の定義は " + top.qualifiedName + "（" +
				top.filePath + ":" + std::to_string(top.startLine) + "-" +
				std::to_string(top.endLine) + "）。";
		}
		return CommandResult::Ok(std::move(payload));
	}

private:
	CodeIndexService& m_service;
	ToolDescriptor m_descriptor;
};

} // namespace

void RegisterCodeSearchTool(CommandPipeline& pipeline, CodeIndexService& service) {
	pipeline.RegisterTool(std::make_shared<CodeSearchTool>(service));
	pipeline.RegisterTool(std::make_shared<GetSymbolInfoTool>(service));
}

} // namespace agentos
