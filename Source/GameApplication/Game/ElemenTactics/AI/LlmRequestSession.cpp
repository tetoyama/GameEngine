#include "LlmRequestSession.h"

#include <algorithm>
#include <exception>

namespace ElemenTactics {

void LlmRequestSession::Attach(ILlmAgentPort* agent) noexcept{
	if(m_active && m_agent) m_agent->Stop();
	m_agent = agent;
	m_active = false;
	m_seenRunning = false;
	m_outputAtBegin.clear();
}

std::uint64_t LlmRequestSession::Begin(
	const std::string& prompt,
	double nowSeconds,
	double timeoutSeconds,
	std::string* error){
	if(!m_agent){
		if(error) *error = "LLM agent is not attached";
		return 0;
	}
	if(prompt.empty()){
		if(error) *error = "LLM prompt is empty";
		return 0;
	}
	if(timeoutSeconds <= 0.0){
		if(error) *error = "LLM timeout must be positive";
		return 0;
	}
	if(m_agent->GetState() == LlmAgentState::Dead){
		if(error) *error = "LLM agent is dead";
		return 0;
	}
	if(m_active) Cancel();

	++m_generation;
	m_deadlineSeconds = nowSeconds + timeoutSeconds;
	m_seenRunning = false;
	m_outputAtBegin = m_agent->GetOutput();
	m_active = true;
	try{
		m_agent->RunAsync(prompt);
	} catch(const std::exception& exception){
		m_active = false;
		if(error) *error = exception.what();
		return 0;
	} catch(...){
		m_active = false;
		if(error) *error = "LLM RunAsync threw an unknown exception";
		return 0;
	}
	return m_generation;
}

LlmRequestResult LlmRequestSession::Poll(double nowSeconds){
	if(!m_active){
		return LlmRequestResult{LlmRequestStatus::None, m_generation, {}, {}};
	}
	if(!m_agent){
		return Finish(LlmRequestStatus::Failed, {}, "LLM agent was detached while a request was active");
	}

	const LlmAgentState state = m_agent->GetState();
	const std::string output = m_agent->GetOutput();
	if(state == LlmAgentState::Running || state == LlmAgentState::Stopping){
		m_seenRunning = true;
	}
	if(state == LlmAgentState::Dead){
		return Finish(LlmRequestStatus::Failed, output, "LLM agent terminated during the request");
	}
	if(nowSeconds >= m_deadlineSeconds){
		m_agent->Stop();
		return Finish(LlmRequestStatus::TimedOut, {}, "LLM request timed out");
	}

	const bool outputChanged = output != m_outputAtBegin;
	if(state == LlmAgentState::Idle && (m_seenRunning || outputChanged)){
		if(output.empty()){
			return Finish(LlmRequestStatus::Failed, {}, "LLM request completed without output");
		}
		return Finish(LlmRequestStatus::Completed, output, {});
	}
	return LlmRequestResult{LlmRequestStatus::Pending, m_generation, output, {}};
}

LlmRequestResult LlmRequestSession::Cancel(){
	if(!m_active){
		return LlmRequestResult{LlmRequestStatus::None, m_generation, {}, {}};
	}
	if(m_agent) m_agent->Stop();
	return Finish(LlmRequestStatus::Cancelled, {}, {});
}

bool LlmRequestSession::ResetForNewMatch(std::string* error){
	if(!m_agent){
		if(error) *error = "LLM agent is not attached";
		return false;
	}
	if(m_active) Cancel();
	try{
		m_agent->ResetContext();
	} catch(const std::exception& exception){
		if(error) *error = exception.what();
		return false;
	} catch(...){
		if(error) *error = "LLM ResetContext threw an unknown exception";
		return false;
	}
	++m_generation;
	m_seenRunning = false;
	m_outputAtBegin = m_agent->GetOutput();
	return true;
}

void LlmRequestSession::Detach() noexcept{
	if(m_active && m_agent) m_agent->Stop();
	m_agent = nullptr;
	m_active = false;
	m_seenRunning = false;
	m_outputAtBegin.clear();
}

LlmRequestResult LlmRequestSession::Finish(
	LlmRequestStatus status,
	std::string output,
	std::string error){
	m_active = false;
	m_seenRunning = false;
	m_outputAtBegin.clear();
	return LlmRequestResult{status, m_generation, std::move(output), std::move(error)};
}

} // namespace ElemenTactics
