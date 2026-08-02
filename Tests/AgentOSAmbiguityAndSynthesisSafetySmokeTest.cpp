// =======================================================================
//
// AgentOSAmbiguityAndSynthesisSafetySmokeTest.cpp
//
// =======================================================================
#include "AgentOS/Core/Agents/AgentContext.h"
#include "AgentOS/Core/Agents/IntakeAgent.h"
#include "AgentOS/Core/Command/CapabilitySet.h"
#include "AgentOS/Core/Command/CommandPipeline.h"
#include "AgentOS/Core/Command/CommandTypes.h"
#include "AgentOS/Core/Llm/MockLlmBackend.h"
#include "AgentOS/Core/Llm/PromptTemplates.h"
#include "AgentOS/Core/Orchestrator/Orchestrator.h"
#include "AgentOS/Core/Store/TaskStore.h"

#include <cassert>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>

using namespace agentos;

namespace {

const char* kDbPath = "/tmp/agentos_ambiguity_synthesis_test.db";

void RemoveDb() {
    std::remove(kDbPath);
    std::remove((std::string(kDbPath) + "-wal").c_str());
    std::remove((std::string(kDbPath) + "-shm").c_str());
    std::remove((std::string(kDbPath) + "-journal").c_str());
}

class ListEntitiesTool final : public ICommandExecutor {
public:
    ListEntitiesTool() {
        descriptor_.name = "ListEntities";
        descriptor_.description = "Entity一覧を取得する";
        descriptor_.requiredPermission = PermissionLevel::Read;
        descriptor_.argumentSchema = Json::object();
    }
    const ToolDescriptor& Descriptor() const override { return descriptor_; }
    Result CheckPrecondition(const Json&) override { return Result::Ok(); }
    CommandResult Execute(const Json&) override {
        Json entities = Json::array();
        for (int i = 0; i < 41; ++i) {
  entities.push_back(Json::object({{"name", "CurrentEntity" + std::to_string(i)}}));
        }
        return CommandResult::Ok(Json::object({
  {"claim", "Entityを41件取得した"},
  {"count", 41},
  {"entities", std::move(entities)},
        }));
    }
private:
    ToolDescriptor descriptor_;
};

} // namespace

int main() {
    std::cout << "=== AgentOS Ambiguity / Synthesis Safety Smoke Test ===\n";

    Json history = Json::object({
        {"summary", "以前はField, Light, SkyBox, Player, Cameraの5件と回答した"},
        {"totalTurns", 1},
        {"recentTurns", Json::array({Json::object({
  {"sessionId", 1},
  {"user", "Entityを教えて"},
  {"assistant", "Field, Light, SkyBox, Player, Cameraの5件です"},
        })})},
    });

    MockLlmBackend intakeLlm;
    intakeLlm.EnqueueResponse(
        "```json\n"
        "{\"goal\":\"現在SceneのEntity状況を取得する\","
        "\"resolvedRequest\":\"今のシーンのEntity状況を取得する\","
        "\"turnRelation\":\"refresh\",\"referencedSessionIds\":[],"
        "\"symptoms\":[],\"constraints\":[],\"requiredCapabilities\":[\"ListEntities\"],"
        "\"unresolvedReferences\":[],\"targetKind\":\"concept\","
        "\"targetConcept\":\"current scene\",\"resolvedEntityName\":null,"
        "\"requestType\":\"investigation\"}\n```"
    );
    // 以前は「今のシーン」等のキーワードでturnRelation/requestTypeをプログラム側から
    // 上書きしていたが、キーワード判定は全廃した。Intakeの判定をそのまま使う。
    AgentContext intakeContext;
    intakeContext.llm = &intakeLlm;
    Json freshIntake;
    assert(IntakeAgent::Run(intakeContext, "今のシーンの状況を教えて", history, &freshIntake));
    assert(freshIntake.value("turnRelation", std::string()) == "refresh");
    assert(freshIntake.value("requestType", std::string()) == "investigation");
    assert(freshIntake.at("conversationContext").at("recentTurns").empty());
    assert(freshIntake.value("rootGoal", std::string()) == freshIntake.value("goal", std::string()));

    MockLlmBackend correctionLlm;
    correctionLlm.EnqueueResponse(
        "```json\n"
        "{\"goal\":\"Entity数の訂正を反映する\","
        "\"resolvedRequest\":\"Entityは41個あるという訂正を検証する\","
        "\"turnRelation\":\"correct\",\"referencedSessionIds\":[],"
        "\"symptoms\":[],\"constraints\":[],\"requiredCapabilities\":[],"
        "\"unresolvedReferences\":[],\"requestType\":\"conversation\"}\n```"
    );
    intakeContext.llm = &correctionLlm;
    Json correctionIntake;
    assert(IntakeAgent::Run(intakeContext, "41個あるはず", history, &correctionIntake));
    assert(correctionIntake.value("turnRelation", std::string()) == "correct");
    assert(!correctionIntake.at("conversationContext").at("recentTurns").empty());

    prompts::SetCurrentConversationRequestContext(
        correctionIntake.at("conversationContext"), correctionIntake);
    Json revised;
    assert(prompts::ApplyCurrentRequestPatch(Json::object({
        {"goal", "ReadComponentのSchemaを確認する"},
        {"resolvedRequest", "権限付与手順を調査する"},
        {"constraints", Json::array()},
        {"reason", "repair subgoal"},
    }), &revised));
    assert(revised.value("rootGoal", std::string()) ==
 correctionIntake.value("rootGoal", std::string()));
    assert(revised.value("rootResolvedRequest", std::string()) ==
 correctionIntake.value("rootResolvedRequest", std::string()));
    prompts::ClearCurrentConversationRequestContext();

    RemoveDb();
    TaskStore store;
    assert(store.Open(kDbPath));
    CapabilityRegistry capabilities;
    CommandPipeline pipeline(&capabilities);
    pipeline.RegisterTool(std::make_shared<ListEntitiesTool>());

    MockLlmBackend llm;
    llm.EnqueueResponse(
        "```json\n"
        "{\"goal\":\"現在SceneのEntity数を取得する\","
        "\"resolvedRequest\":\"現在のシーンのEntity数を取得する\","
        "\"turnRelation\":\"refresh\",\"referencedSessionIds\":[],"
        "\"symptoms\":[],\"constraints\":[],\"requiredCapabilities\":[\"ListEntities\"],"
        "\"unresolvedReferences\":[],\"requestType\":\"investigation\"}\n```"
    );
    llm.EnqueueResponse(
        "```json\n"
        "{\"tasks\":[{\"taskId\":\"T1\",\"type\":\"RuntimeObservation\","
        "\"description\":\"現在Entity一覧を取得する\",\"dependencies\":[],"
        "\"allowedTools\":[\"ListEntities\"],\"searchHints\":[]}]}\n```"
    );
    llm.EnqueueResponse(
        "```json\n"
        "{\"hypotheses\":[{\"description\":\"現在のEntityは41件\","
        "\"rubricBase\":1.0,\"supports\":[1],\"contradicts\":[],\"missingEvidence\":[]}]}\n```"
    );
    llm.EnqueueResponse(
        "```json\n"
        "{\"scores\":{\"evidenceCoverage\":1.0,\"contradictionHandling\":1.0,"
        "\"causalCompleteness\":1.0,\"testability\":1.0},\"failures\":[],"
        "\"goalSatisfied\":true,\"unmetAspects\":[],\"requestPatch\":null,"
        "\"additionalTasksSuggested\":[]}\n```"
    );
    llm.EnqueueResponse(
        "```json\n"
        "{\"report\":\"現在のEntityはField, Light, SkyBox, Player, Cameraの5個です。\"}\n```"
    );

    OrchestratorConfig config;
    config.maxRepairRounds = 0;
    config.budget.maxLlmCalls = 8;
    config.budget.maxLlmChars = 200000;
    Orchestrator orchestrator(&llm, &pipeline, &store, &capabilities, config);
    const OrchestratorResult result = orchestrator.RunSession("今のシーンのEntity数を教えて");
    assert(!result.completed);
    assert(result.stopInfo.value("reason", std::string()).find("synthesis validation failed") !=
 std::string::npos);
    assert(result.report.find("41") != std::string::npos);
    assert(result.report.find("5個です") == std::string::npos);

    prompts::ClearCurrentConversationRequestContext();
    RemoveDb();
    std::cout << "=== ALL PASSED ===\n";
    return 0;
}
