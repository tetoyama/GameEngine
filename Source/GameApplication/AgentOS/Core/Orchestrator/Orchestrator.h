// =======================================================================
//
// Orchestrator.h
//
// 垂直スライスのメインループ本体（構想§10 / 00_Architecture.md §9,10）。
// Intake → Plan → (Worker実行 → EvidenceBuild → Reason → Critic →
// [不足ならRepair→再実行]) → Synthesize を同期的に1回通す。
// 呼び出し側が専用スレッドで実行する前提（内部でスレッドは作らない）。
//
// =======================================================================
#pragma once

#include <functional>
#include <string>

#include "../AgentOsTypes.h"
#include "../Json.h"
#include "../Budget/Budget.h"
#include "../Command/CapabilitySet.h"
#include "../Command/CommandPipeline.h"
#include "../Command/CommandTypes.h"
#include "../Llm/ILlmBackend.h"
#include "../Store/TaskStore.h"

namespace agentos {

struct OrchestratorConfig {
	Budget budget;
	int maxRepairRounds = 2;
};

struct OrchestratorResult {
	bool completed = false;
	std::string report;
	Json stopInfo = Json::object();
	SessionId sessionId = kInvalidId;
	Json rankedHypotheses = Json::object();
};

class Orchestrator {
public:
	Orchestrator(ILlmBackend* llm, CommandPipeline* pipeline, TaskStore* store,
	             CapabilityRegistry* capabilityRegistry, OrchestratorConfig config = {});

	// 同期実行。呼び出し側がワーカースレッドから呼ぶ想定。
	// LLM/Tool/バリデーションのいずれが失敗しても例外・クラッシュせず、
	// stopInfo.reasonを埋めた劣化済みOrchestratorResultを返す。
	OrchestratorResult RunSession(const std::string& userRequest);

	// Intake/Plan/Retrieve/Reason/Critic/Repair/Synthesizeの各段階遷移で呼ばれる。
	// UIが進捗表示に使う。
	void SetProgressCallback(std::function<void(const std::string& stage, const Json& detail)> callback);

	// テスト・監査用: 直近のRunSessionでAgentへ発行したCapabilityTokenを返す。
	// （Modify権限は絶対に含まれないことをテストで直接検証できるようにするため）
	CapabilityToken GetLastIssuedToken() const;

private:
	void ReportProgress(const std::string& stage, const Json& detail);

	ILlmBackend* llm_ = nullptr;
	CommandPipeline* pipeline_ = nullptr;
	TaskStore* store_ = nullptr;
	CapabilityRegistry* capabilityRegistry_ = nullptr;
	OrchestratorConfig config_;

	std::function<void(const std::string&, const Json&)> progressCallback_;
	CapabilityToken lastToken_;
};

} // namespace agentos
