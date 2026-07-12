#include "../Flow/MatchFlowModel.h"
#include "ElemenTacticsLlmFacade.h"

#include "Editor/editorService.h"
#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Service/LlamaService/AgentConfig.h"
#include "Service/LlamaService/LLAMAService.h"

#include <memory>
#include <string>

namespace ElemenTactics {

namespace {

constexpr const char* ModelPath = "Asset/BRAIN/model/Qwen3.5-9B-Q4_K_M.gguf";
constexpr double DecisionTimeoutSeconds = 8.0;

LLAMAService* ResolveService(SceneContext* sceneContext){
	if(!sceneContext || !sceneContext->manager) return nullptr;
	if(sceneContext->manager->llama) return sceneContext->manager->llama;
	if(sceneContext->manager->editor) return sceneContext->manager->editor->llamaService;
	return nullptr;
}

std::shared_ptr<const AgentConfig> BuildGameAgentConfig(){
	auto config = std::make_shared<AgentConfig>();
	config->n_ctx = 4096;
	config->n_predict = 384;
	config->n_threads = 8;
	config->temperature = 0.35f;
	config->top_p = 0.90f;
	config->top_k = 24;
	config->repeat_penalty = 1.08f;
	return config;
}

AiStepResult PendingResult(){
	AiStepResult result;
	result.applied = true;
	result.usedLlm = true;
	result.pending = true;
	result.reasoning.currentGoal = "公開情報だけで局面を評価中";
	result.reasoning.actionReason = "Rules Engineが列挙した合法手から次の1行動を選択している";
	result.reasoning.confidence = 0.0;
	return result;
}

AiStepResult Fallback(
	GameState& state,
	PlayerId aiPlayer,
	std::uint64_t seed,
	std::string reason){
	AiStepResult result = AiTurnCoordinator::ExecuteNextStep(state, aiPlayer, seed);
	result.usedFallback = true;
	result.fallbackReason = std::move(reason);
	if(result.reasoning.currentGoal.empty()){
		result.reasoning.currentGoal = "合法手をヒューリスティック評価";
		result.reasoning.actionReason = "LLMを利用できないため公開情報ベースの安全な方策へ切り替えた";
		result.reasoning.confidence = 0.5;
	}
	return result;
}

} // namespace

AiStepResult AiTurnCoordinator::ExecuteNextStepWithFacade(
	ElemenTacticsLlmFacade& facade,
	SceneContext* sceneContext,
	float deltaTime,
	GameState& state,
	PlayerId aiPlayer,
	std::uint64_t seed){
	const double now = facade.AdvanceRuntimeClock(static_cast<double>(deltaTime));

	if(state.result.finished){
		facade.Cancel();
		AiStepResult result;
		result.error = "match is already finished";
		return result;
	}
	if(state.currentPlayer != aiPlayer){
		facade.Cancel();
		AiStepResult result;
		result.error = "it is not the AI player's turn";
		return result;
	}

	// Reorder is deterministic and does not need an LLM request.
	if(state.pendingReorder){
		facade.Cancel();
		return AiTurnCoordinator::ExecuteNextStep(state, aiPlayer, seed);
	}

	LLAMAService* service = ResolveService(sceneContext);
	if(!facade.IsInitialized() && service && service->GetModel(ModelPath)){
		std::string initializeError;
		if(!facade.Initialize(service, ModelPath, BuildGameAgentConfig(), &initializeError)){
			return Fallback(state, aiPlayer, seed, std::move(initializeError));
		}
	}

	if(facade.HasActiveRequest()){
		const LlmDecisionPollResult poll = facade.Poll(now);
		if(poll.status == LlmDecisionStatus::Pending){
			return PendingResult();
		}
		if(poll.status == LlmDecisionStatus::Completed && poll.decision){
			if(facade.RequestStateSerial() != state.actionSerial || state.currentPlayer != aiPlayer){
				facade.ClearRequestStateSerial();
				return Fallback(state, aiPlayer, seed, "LLM response became stale after the board state changed");
			}
			std::string legalError;
			if(!ElemenTacticsRules::IsLegalAction(state, poll.decision->action, &legalError)){
				facade.ClearRequestStateSerial();
				return Fallback(state, aiPlayer, seed, "LLM action failed final validation: " + legalError);
			}

			AiStepResult result;
			result.action = ElemenTacticsRules::ApplyAction(state, poll.decision->action);
			result.applied = result.action->applied;
			result.usedLlm = result.applied;
			result.reasoning = poll.decision->publicReasoning;
			if(!result.applied) result.error = result.action->error;
			facade.ClearRequestStateSerial();
			return result;
		}

		facade.ClearRequestStateSerial();
		const std::string reason = poll.error.empty()
			? "LLM request did not return a valid completed decision"
			: poll.error;
		return Fallback(state, aiPlayer, seed, reason);
	}

	if(!facade.IsInitialized()){
		return Fallback(
			state,
			aiPlayer,
			seed,
			service ? "LLM model is still loading" : "LLAMAService is unavailable");
	}

	const PublicGameView view = ElemenTacticsRules::BuildPublicView(state, aiPlayer);
	const std::vector<GameAction> legalActions = ElemenTacticsRules::GenerateLegalActions(state);
	if(legalActions.empty()){
		AiStepResult result;
		result.error = "no legal action exists";
		return result;
	}

	std::string beginError;
	const std::uint64_t generation = facade.BeginDecision(
		view,
		legalActions,
		now,
		DecisionTimeoutSeconds,
		&beginError);
	if(generation == 0){
		return Fallback(state, aiPlayer, seed, std::move(beginError));
	}
	facade.SetRequestStateSerial(state.actionSerial);
	return PendingResult();
}

} // namespace ElemenTactics
