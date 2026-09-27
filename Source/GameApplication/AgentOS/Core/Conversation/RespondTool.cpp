// =======================================================================
//
// RespondTool.cpp
//
// =======================================================================
#include "RespondTool.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "../Command/CommandPipeline.h"
#include "../Llm/ILlmBackend.h"
#include "../Llm/JsonExtractor.h"
#include "../Llm/PromptTemplates.h"
#include "../TextUtf8.h"

namespace agentos {

namespace {

// RetrievalWorker.cpp の kDiscoveryTools へ登録すること。
// 応答文の指示は「調査の起点」であり、過去Evidenceに含まれているはずがない。
// Discovery Toolでないと毎回 GroundingRejected で弾かれる。
constexpr const char* kToolName = "Respond";

constexpr std::size_t kReplyChars = 4000;

class RespondTool final : public ICommandExecutor {
public:
	explicit RespondTool(std::function<ILlmBackend*()> llmProvider)
		: m_llmProvider(std::move(llmProvider))
		, m_descriptor{
			kToolName,
			"要求に対する応答文を作る。"
			"Engineやコードの観測を必要としない要求（挨拶、雑談、能力の説明、"
			"ユーザ個人に関する質問、収集済みEvidenceだけで答えられる要求）はこれで完結する。"
			"根拠は収集済みEvidenceに限られ、載っていないことは断定せず"
			"「分からない」と正直に答える。"
			"Engineの実データやコードが要る場合は、先にそれを取るToolを使うこと。",
			PermissionLevel::Read,
			Json::object({
				{"instruction", Json::object({
					{"type", "string"},
					{"required", true},
					{"description", "何に答えるか。要求そのものを書けばよい。"},
				})},
			})
		} {}

	const ToolDescriptor& Descriptor() const override { return m_descriptor; }

	Result CheckPrecondition(const Json& arguments) override {
		if (!arguments.contains("instruction")) return Result::Fail("instruction は必須");
		if (!arguments.at("instruction").is_string() ||
		    arguments.at("instruction").get<std::string>().empty()) {
			return Result::Fail("instruction は空でない文字列であること");
		}
		return Result::Ok();
	}

	CommandResult Execute(const Json& arguments) override {
		const std::string instruction = arguments.value("instruction", std::string());
		if (instruction.empty()) {
			return CommandResult::Fail(CommandStatus::SchemaRejected, "instruction が空");
		}

		ILlmBackend* llm = m_llmProvider ? m_llmProvider() : nullptr;
		if (llm == nullptr) {
			return CommandResult::Fail(
				CommandStatus::PreconditionRejected, "LLMバックエンドが未ロード");
		}

		const PromptPair prompt = prompts::Respond(instruction, prompts::CurrentBuiltEvidence());
		const std::string raw = llm->Generate(prompt.system, prompt.user);

		Json parsed;
		const Result extracted = JsonExtractor::Extract(raw, &parsed);
		std::string reply;
		if (extracted && parsed.is_object() && parsed.contains("reply") &&
		    parsed.at("reply").is_string()) {
			reply = parsed.at("reply").get<std::string>();
		}
		if (reply.empty()) {
			// 生成に失敗したことを失敗Evidenceとして残す。
			// 黙って空文字を返すと、Criticが「応答した」と誤認する。
			return CommandResult::Fail(
				CommandStatus::ExecutionFailed, "応答文の生成に失敗した（replyが空）");
		}
		reply = TruncateUtf8(reply, kReplyChars, "\n...(truncated)");

		// claimはEvidenceの見出しになる。応答本文そのものを載せる。
		// 見出しに要約を置くと、Synthesisが本文ではなく要約を最終応答にしてしまう。
		return CommandResult::Ok(Json::object({
			{"claim", reply},
			{"reply", reply},
			{"instruction", instruction},
		}));
	}

private:
	std::function<ILlmBackend*()> m_llmProvider;
	ToolDescriptor m_descriptor;
};

} // namespace

const char* RespondToolName() noexcept { return kToolName; }

void RegisterRespondTool(CommandPipeline& pipeline, std::function<ILlmBackend*()> llmProvider) {
	// 無条件に登録する。バックエンドの有無は実行時に見る。
	// 登録時点で判定すると、非同期ロードが終わる前は一覧から消えてしまう。
	pipeline.RegisterTool(std::make_shared<RespondTool>(std::move(llmProvider)));
}

} // namespace agentos
