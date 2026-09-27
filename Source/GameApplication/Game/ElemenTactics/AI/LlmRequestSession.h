#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace ElemenTactics {

enum class LlmAgentState : std::uint8_t {
	Idle,
	Running,
	Stopping,
	Dead
};

class ILlmAgentPort {
public:
	virtual ~ILlmAgentPort() = default;
	virtual void RunAsync(const std::string& prompt) = 0;
	virtual void Stop() = 0;
	virtual void ResetContext() = 0;
	virtual LlmAgentState GetState() const noexcept = 0;
	virtual std::string GetOutput() const = 0;
};

enum class LlmRequestStatus : std::uint8_t {
	None,
	Pending,
	Completed,
	TimedOut,
	Cancelled,
	Failed
};

struct LlmRequestResult {
	LlmRequestStatus status = LlmRequestStatus::None;
	std::uint64_t generation = 0;
	std::string output;
	std::string error;
};

class LlmRequestSession final {
public:
	explicit LlmRequestSession(ILlmAgentPort* agent = nullptr) noexcept
		: m_agent(agent){}

	void Attach(ILlmAgentPort* agent) noexcept;
	std::uint64_t Begin(
		const std::string& prompt,
		double nowSeconds,
		double timeoutSeconds,
		std::string* error = nullptr);
	LlmRequestResult Poll(double nowSeconds);
	LlmRequestResult Cancel();
	bool ResetForNewMatch(std::string* error = nullptr);
	void Detach() noexcept;

	bool HasActiveRequest() const noexcept{ return m_active; }
	std::uint64_t Generation() const noexcept{ return m_generation; }

private:
	LlmRequestResult Finish(LlmRequestStatus status, std::string output = {}, std::string error = {});

	ILlmAgentPort* m_agent = nullptr;
	std::uint64_t m_generation = 0;
	double m_deadlineSeconds = 0.0;
	bool m_active = false;
	bool m_seenRunning = false;
	std::string m_outputAtBegin;
};

} // namespace ElemenTactics
