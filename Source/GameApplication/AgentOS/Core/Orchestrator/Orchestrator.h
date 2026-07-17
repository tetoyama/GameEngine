// =======================================================================
//
// Orchestrator.h
//
// Intake → Plan → Retrieve → Evidence → Reason → Critic → Repair → Synthesize。
// conversationContextは、古いTurnの累積要約と最近のuser/assistant最終応答ペアを含む。
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
	Json resolvedRequest = Json::object();
};

class Orchestrator {
public:
	Orchestrator(ILlmBackend* llm, CommandPipeline* pipeline, TaskStore* store,
	             CapabilityRegistry* capabilityRegistry, OrchestratorConfig config = {});

	OrchestratorResult RunSession(
		const std::string& userRequest,
		const Json& conversationContext = Json::object());

	void SetProgressCallback(std::function<void(const std::string& stage, const Json& detail)> callback);
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
