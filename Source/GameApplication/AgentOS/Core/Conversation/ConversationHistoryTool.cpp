// =======================================================================
//
// ConversationHistoryTool.cpp
//
// =======================================================================
#include "ConversationHistoryTool.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../Agents/AgentContext.h"
#include "../Command/CommandPipeline.h"
#include "../Store/TaskStore.h"
#include "../TextUtf8.h"

namespace agentos {

namespace {

// ---------------------------------
// ツール名について
// ---------------------------------
// RetrievalWorker.cpp の kDiscoveryTools へ必ず登録すること。
// Discovery Toolでない場合「引数は依存Evidenceに存在する文字列であること」という
// 束縛が掛かり、検索語が毎回 GroundingRejected で弾かれる。
// 履歴検索の検索語は起点であり、過去Evidenceに含まれているはずがない
// （CodeSearchを "SearchCode" と命名して同じ罠を踏んでいる）。
constexpr const char* kToolName = "GetConversationHistory";

// 1種別あたりの既定件数と上限。
constexpr int kDefaultLimit = 5;
constexpr int kMaxLimit = 20;

// 本文の切り詰め。UTF-8境界で切ること
// （バイト位置で切ると不正なUTF-8になり json::dump() が例外を投げる）。
constexpr std::size_t kTurnTextChars = 1200;
constexpr std::size_t kEvidenceClaimChars = 400;

const char* const kAllKinds[] = {"evidence", "userTurn", "threadState", "assistantTurn"};

bool IsKnownKind(const std::string& kind) {
	for (const char* known : kAllKinds) {
		if (kind == known) return true;
	}
	return false;
}

// 種別ごとの位置づけ。
//
// 「Conversation MemoryをEngine Evidenceとして扱わない」という既存の規律は、
// 過去のLLMの作文を観測として再流入させないためのもの。
// 過去のTool実行結果はそもそも観測なので、この規律の対象ではない。
//
// 4種別すべてEvidenceとして載せるが、仮説を支える資格は分ける。
// 実際の判定はCriticAgentのゲート#7が決定的に行う（プロンプトに守らせない）。
const char* KindRole(const std::string& kind) {
	if (kind == "evidence") return "observation"; // 過去のTool実行結果。根拠になれる
	return "reference";                            // 参照解決には使えるが根拠にはならない
}

Json TrimEntry(Json entry) {
	if (!entry.is_object()) return entry;
	const std::string kind = entry.value("kind", std::string());
	entry["role"] = KindRole(kind);

	if (entry.contains("text") && entry.at("text").is_string()) {
		entry["text"] = TruncateUtf8(
			entry.at("text").get<std::string>(), kTurnTextChars, "...(truncated)");
	}
	if (entry.contains("claim") && entry.at("claim").is_string()) {
		entry["claim"] = TruncateUtf8(
			entry.at("claim").get<std::string>(), kEvidenceClaimChars, "...(truncated)");
	}
	return entry;
}

class GetConversationHistoryTool final : public ICommandExecutor {
public:
	explicit GetConversationHistoryTool(TaskStore& store)
		: m_store(store)
		, m_descriptor{
			kToolName,
			"過去のやり取りの記録を引く。"
			"「さっきの5件」「その続き」のように、今回の入力だけでは対象が確定しない参照を解決するために使う。"
			"返す種別は evidence（過去のTool実行結果＝観測）、userTurn（ユーザの過去発話）、"
			"threadState（過去要求の構造化要約）、assistantTurn（過去のAgent応答）。"
			"断定の根拠にできるのは evidence だけであり、他は参照解決の手がかりとして扱うこと。",
			PermissionLevel::Read,
			Json::object({
				{"query", Json::object({
					{"type", "string"},
					{"required", false},
					{"description", "絞り込みたい語。省略すると直近から順に返す。"},
				})},
				{"kinds", Json::object({
					{"type", "array"},
					{"required", false},
					{"description",
					 "取得する種別（evidence / userTurn / threadState / assistantTurn）。省略時は全種別。"},
				})},
				{"limit", Json::object({
					{"type", "integer"},
					{"required", false},
					{"description", "種別ごとの件数（既定5、最大20）"},
				})},
			})
		} {}

	const ToolDescriptor& Descriptor() const override { return m_descriptor; }

	Result CheckPrecondition(const Json& arguments) override {
		if (arguments.contains("kinds")) {
			const Json& kinds = arguments.at("kinds");
			if (!kinds.is_array()) return Result::Fail("kinds は配列であること");
			for (const Json& kind : kinds) {
				if (!kind.is_string() || !IsKnownKind(kind.get<std::string>())) {
					return Result::Fail(
						"kinds の要素は evidence / userTurn / threadState / assistantTurn のいずれか");
				}
			}
		}
		return Result::Ok();
	}

	CommandResult Execute(const Json& arguments) override {
		const SessionId current = CurrentSessionId();
		if (current == kInvalidId) {
			// 境界が分からないと現在セッション自身の記録まで引いてしまう。
			return CommandResult::Fail(
				CommandStatus::PreconditionRejected, "現在セッションIDが未設定");
		}

		const std::string query = arguments.value("query", std::string());

		std::vector<std::string> kinds;
		if (arguments.contains("kinds") && arguments.at("kinds").is_array()) {
			for (const Json& kind : arguments.at("kinds")) {
				if (kind.is_string()) kinds.push_back(kind.get<std::string>());
			}
		}

		int limit = arguments.value("limit", kDefaultLimit);
		if (limit < 1) limit = 1;
		if (limit > kMaxLimit) limit = kMaxLimit;

		const Json rawEntries = m_store.SearchConversationHistory(current, query, kinds, limit);

		Json entries = Json::array();
		std::size_t observationCount = 0;
		for (const Json& entry : rawEntries) {
			Json trimmed = TrimEntry(entry);
			if (trimmed.value("role", std::string()) == "observation") ++observationCount;
			entries.push_back(std::move(trimmed));
		}

		Json payload = Json::object({
			{"count", entries.size()},
			{"observationCount", observationCount},
			{"entries", std::move(entries)},
		});
		if (!query.empty()) payload["query"] = query;

		// claimはEvidenceの見出しになる。0件を「過去に無かった」と言い切らない。
		if (payload.at("count").get<std::size_t>() == 0) {
			payload["claim"] = query.empty()
				? std::string("過去のやり取りの記録は見つからなかった。")
				: "過去のやり取りに「" + query + "」に一致する記録は見つからなかった。";
		} else {
			payload["claim"] =
				"過去のやり取りから" + std::to_string(payload.at("count").get<std::size_t>()) +
				"件の記録を取得した（うち観測=" + std::to_string(observationCount) + "件）。"
				"観測以外は参照解決の手がかりであり、断定の根拠にはできない。";
		}
		return CommandResult::Ok(std::move(payload));
	}

private:
	TaskStore& m_store;
	ToolDescriptor m_descriptor;
};

} // namespace

void RegisterConversationHistoryTool(CommandPipeline& pipeline, TaskStore* store) {
	if (store == nullptr) return;
	pipeline.RegisterTool(std::make_shared<GetConversationHistoryTool>(*store));
}

} // namespace agentos
