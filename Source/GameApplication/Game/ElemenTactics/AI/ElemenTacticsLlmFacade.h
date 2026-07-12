#pragma once

#include "LlmDecisionAdapter.h"
#include "LlmRequestSession.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// windows.h defines function-like min/max macros unless NOMINMAX was set
// before its first inclusion. ElemenTactics uses the standard algorithms, so
// remove any macros that entered through existing Engine headers.
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

class LLAMAService;
class LLAMAAgent;
struct AgentConfig;

namespace ElemenTactics {

enum class LlmDecisionStatus : std::uint8_t {
	None,
	Pending,
	Completed,
	TimedOut,
	Cancelled,
	Failed
};

struct LlmDecisionPollResult {
	LlmDecisionStatus status = LlmDecisionStatus::None;
	std::uint64_t generation = 0;
	std::optional<LlmDecision> decision;
	std::string rawOutput;
	std::string error;
};

class ElemenTacticsLlmFacade final {
public:
	ElemenTacticsLlmFacade();
	~ElemenTacticsLlmFacade();

	ElemenTacticsLlmFacade(const ElemenTacticsLlmFacade&) = delete;
	ElemenTacticsLlmFacade& operator=(const ElemenTacticsLlmFacade&) = delete;

	bool Initialize(
		LLAMAService* service,
		const std::string& modelPath,
		std::shared_ptr<const AgentConfig> config,
		std::string* error = nullptr);
	bool ResetForNewMatch(std::string* error = nullptr);
	std::uint64_t BeginDecision(
		const PublicGameView& view,
		const std::vector<GameAction>& legalActions,
		double nowSeconds,
		double timeoutSeconds,
		std::string* error = nullptr);
	LlmDecisionPollResult Poll(double nowSeconds);
	LlmDecisionPollResult Cancel();
	void Shutdown() noexcept;

	bool IsInitialized() const noexcept;
	bool HasActiveRequest() const noexcept{ return m_session.HasActiveRequest(); }

	double AdvanceRuntimeClock(double deltaSeconds) noexcept{
		if(deltaSeconds > 0.0) m_runtimeClock += deltaSeconds;
		return m_runtimeClock;
	}
	double RuntimeClock() const noexcept{ return m_runtimeClock; }
	void SetRequestStateSerial(std::uint64_t serial) noexcept{ m_requestStateSerial = serial; }
	std::uint64_t RequestStateSerial() const noexcept{ return m_requestStateSerial; }
	void ClearRequestStateSerial() noexcept{ m_requestStateSerial = 0; }

private:
	class AgentPort;
	static LlmDecisionStatus ConvertStatus(LlmRequestStatus status) noexcept;

	LLAMAService* m_service = nullptr;
	std::shared_ptr<LLAMAAgent> m_agent;
	std::unique_ptr<AgentPort> m_port;
	LlmRequestSession m_session;
	PublicGameView m_requestView;
	std::vector<GameAction> m_requestLegalActions;
	double m_runtimeClock = 0.0;
	std::uint64_t m_requestStateSerial = 0;
};

} // namespace ElemenTactics
