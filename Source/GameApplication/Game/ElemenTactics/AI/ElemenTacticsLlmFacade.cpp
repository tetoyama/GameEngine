#include "ElemenTacticsLlmFacade.h"

#include "Service/LlamaService/AgentConfig.h"
#include "Service/LlamaService/LLAMAAgent.h"
#include "Service/LlamaService/LLAMAService.h"

#include <exception>
#include <utility>

namespace ElemenTactics {

class ElemenTacticsLlmFacade::AgentPort final : public ILlmAgentPort {
public:
	explicit AgentPort(std::shared_ptr<LLAMAAgent> agent)
		: m_agent(std::move(agent)){}

	void RunAsync(const std::string& prompt) override{
		if(m_agent) m_agent->RunAsync(prompt);
	}

	void Stop() override{
		if(m_agent) m_agent->Stop();
	}

	void ResetContext() override{
		if(m_agent) m_agent->ResetContext();
	}

	LlmAgentState GetState() const noexcept override{
		if(!m_agent) return LlmAgentState::Dead;
		switch(m_agent->GetState()){
		case LLAMAAgent::State::Idle: return LlmAgentState::Idle;
		case LLAMAAgent::State::Running: return LlmAgentState::Running;
		case LLAMAAgent::State::Stopping: return LlmAgentState::Stopping;
		case LLAMAAgent::State::Dead: return LlmAgentState::Dead;
		default: return LlmAgentState::Dead;
		}
	}

	std::string GetOutput() const override{
		return m_agent ? m_agent->GetOutput() : std::string{};
	}

private:
	std::shared_ptr<LLAMAAgent> m_agent;
};

ElemenTacticsLlmFacade::ElemenTacticsLlmFacade() = default;

ElemenTacticsLlmFacade::~ElemenTacticsLlmFacade(){
	Shutdown();
}

bool ElemenTacticsLlmFacade::Initialize(
	LLAMAService* service,
	const std::string& modelPath,
	std::shared_ptr<const AgentConfig> config,
	std::string* error){
	Shutdown();
	if(!service){
		if(error) *error = "LLAMAService is null";
		return false;
	}
	if(modelPath.empty()){
		if(error) *error = "LLM model path is empty";
		return false;
	}
	if(!config || !config->IsValid()){
		if(error) *error = "LLM AgentConfig is missing or invalid";
		return false;
	}

	try{
		if(!service->GetModel(modelPath) && !service->LoadModel(modelPath)){
			if(error) *error = "failed to load the LLM model";
			return false;
		}
		m_agent = service->CreateAgent(modelPath, config);
	} catch(const std::exception& exception){
		if(error) *error = exception.what();
		return false;
	} catch(...){
		if(error) *error = "LLM service threw an unknown exception while creating the agent";
		return false;
	}
	if(!m_agent){
		if(error) *error = "LLAMAService could not create an agent";
		return false;
	}

	m_service = service;
	m_port = std::make_unique<AgentPort>(m_agent);
	m_session.Attach(m_port.get());
	return true;
}

bool ElemenTacticsLlmFacade::ResetForNewMatch(std::string* error){
	m_requestLegalActions.clear();
	m_requestView = PublicGameView{};
	return m_session.ResetForNewMatch(error);
}

std::uint64_t ElemenTacticsLlmFacade::BeginDecision(
	const PublicGameView& view,
	const std::vector<GameAction>& legalActions,
	double nowSeconds,
	double timeoutSeconds,
	std::string* error){
	if(!IsInitialized()){
		if(error) *error = "ElemenTactics LLM facade is not initialized";
		return 0;
	}
	if(view.result.finished || view.currentPlayer != view.viewer){
		if(error) *error = "public view is not an active turn for the LLM player";
		return 0;
	}
	if(view.pendingReorder){
		if(error) *error = "center reorder is resolved by the deterministic AI policy";
		return 0;
	}
	if(legalActions.empty()){
		if(error) *error = "there are no legal actions to offer the LLM";
		return 0;
	}

	m_requestView = view;
	m_requestLegalActions = legalActions;
	const std::string prompt = LlmDecisionAdapter::BuildPrompt(view, legalActions);
	const std::uint64_t generation = m_session.Begin(prompt, nowSeconds, timeoutSeconds, error);
	if(generation == 0){
		m_requestLegalActions.clear();
		m_requestView = PublicGameView{};
	}
	return generation;
}

LlmDecisionPollResult ElemenTacticsLlmFacade::Poll(double nowSeconds){
	const LlmRequestResult raw = m_session.Poll(nowSeconds);
	LlmDecisionPollResult result;
	result.status = ConvertStatus(raw.status);
	result.generation = raw.generation;
	result.rawOutput = raw.output;
	result.error = raw.error;

	if(raw.status == LlmRequestStatus::Completed){
		std::string parseError;
		result.decision = LlmDecisionAdapter::ParseAndValidate(
			m_requestView,
			m_requestLegalActions,
			raw.output,
			&parseError);
		if(!result.decision){
			result.status = LlmDecisionStatus::Failed;
			result.error = std::move(parseError);
		}
	}
	if(raw.status != LlmRequestStatus::Pending && raw.status != LlmRequestStatus::None){
		m_requestLegalActions.clear();
		m_requestView = PublicGameView{};
	}
	return result;
}

LlmDecisionPollResult ElemenTacticsLlmFacade::Cancel(){
	const LlmRequestResult raw = m_session.Cancel();
	m_requestLegalActions.clear();
	m_requestView = PublicGameView{};
	return LlmDecisionPollResult{
		ConvertStatus(raw.status), raw.generation, std::nullopt, raw.output, raw.error};
}

void ElemenTacticsLlmFacade::Shutdown() noexcept{
	if(m_session.HasActiveRequest()) m_session.Cancel();
	m_session.Detach();
	m_port.reset();
	if(m_service && m_agent){
		try{
			m_agent->Stop();
			m_service->DestroyAgent(m_agent);
		} catch(...){
			// Destruction must remain noexcept. Releasing the final shared_ptr still joins the agent worker.
		}
	}
	m_agent.reset();
	m_service = nullptr;
	m_requestLegalActions.clear();
	m_requestView = PublicGameView{};
}

bool ElemenTacticsLlmFacade::IsInitialized() const noexcept{
	return m_service != nullptr && m_agent != nullptr && m_port != nullptr;
}

LlmDecisionStatus ElemenTacticsLlmFacade::ConvertStatus(LlmRequestStatus status) noexcept{
	switch(status){
	case LlmRequestStatus::None: return LlmDecisionStatus::None;
	case LlmRequestStatus::Pending: return LlmDecisionStatus::Pending;
	case LlmRequestStatus::Completed: return LlmDecisionStatus::Completed;
	case LlmRequestStatus::TimedOut: return LlmDecisionStatus::TimedOut;
	case LlmRequestStatus::Cancelled: return LlmDecisionStatus::Cancelled;
	case LlmRequestStatus::Failed: return LlmDecisionStatus::Failed;
	default: return LlmDecisionStatus::Failed;
	}
}

} // namespace ElemenTactics
