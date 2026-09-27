#include "Game/ElemenTactics/AI/LlmRequestSession.h"

#include <cassert>
#include <iostream>
#include <string>

using namespace ElemenTactics;

namespace {

class FakeAgent final : public ILlmAgentPort {
public:
	void RunAsync(const std::string& prompt) override{
		lastPrompt = prompt;
		++runCount;
	}

	void Stop() override{
		++stopCount;
		state = LlmAgentState::Stopping;
	}

	void ResetContext() override{
		++resetCount;
		state = LlmAgentState::Idle;
		output.clear();
	}

	LlmAgentState GetState() const noexcept override{ return state; }
	std::string GetOutput() const override{ return output; }

	LlmAgentState state = LlmAgentState::Idle;
	std::string output;
	std::string lastPrompt;
	int runCount = 0;
	int stopCount = 0;
	int resetCount = 0;
};

void TestObservedRunningCompletion(){
	FakeAgent agent;
	LlmRequestSession session(&agent);
	std::string error;
	const std::uint64_t generation = session.Begin("decision", 10.0, 5.0, &error);
	assert(generation != 0 && agent.lastPrompt == "decision");
	assert(session.Poll(10.1).status == LlmRequestStatus::Pending);
	agent.state = LlmAgentState::Running;
	assert(session.Poll(10.2).status == LlmRequestStatus::Pending);
	agent.output = "{\"action_type\":\"scout\"}";
	agent.state = LlmAgentState::Idle;
	const LlmRequestResult completed = session.Poll(10.3);
	assert(completed.status == LlmRequestStatus::Completed);
	assert(completed.generation == generation && !completed.output.empty());
	assert(!session.HasActiveRequest());
}

void TestFastCompletionWithoutRunningPoll(){
	FakeAgent agent;
	agent.output = "previous";
	LlmRequestSession session(&agent);
	const std::uint64_t generation = session.Begin("fast", 0.0, 2.0);
	assert(generation != 0);
	agent.output = "new result";
	agent.state = LlmAgentState::Idle;
	const LlmRequestResult completed = session.Poll(0.1);
	assert(completed.status == LlmRequestStatus::Completed);
	assert(completed.output == "new result");
}

void TestTimeoutAndCancel(){
	FakeAgent agent;
	LlmRequestSession session(&agent);
	assert(session.Begin("slow", 1.0, 1.0) != 0);
	agent.state = LlmAgentState::Running;
	const LlmRequestResult timedOut = session.Poll(2.0);
	assert(timedOut.status == LlmRequestStatus::TimedOut);
	assert(agent.stopCount == 1 && !session.HasActiveRequest());

	agent.state = LlmAgentState::Idle;
	assert(session.Begin("cancel", 3.0, 5.0) != 0);
	const LlmRequestResult cancelled = session.Cancel();
	assert(cancelled.status == LlmRequestStatus::Cancelled);
	assert(agent.stopCount == 2 && !session.HasActiveRequest());
}

void TestResetAndDeadAgent(){
	FakeAgent agent;
	LlmRequestSession session(&agent);
	const std::uint64_t before = session.Generation();
	assert(session.ResetForNewMatch());
	assert(agent.resetCount == 1 && session.Generation() == before + 1);

	agent.state = LlmAgentState::Dead;
	std::string error;
	assert(session.Begin("invalid", 0.0, 1.0, &error) == 0);
	assert(!error.empty());
}

void TestDetachWhileActive(){
	FakeAgent agent;
	LlmRequestSession session(&agent);
	assert(session.Begin("detach", 0.0, 1.0) != 0);
	session.Detach();
	assert(agent.stopCount == 1 && !session.HasActiveRequest());
	assert(session.Poll(0.5).status == LlmRequestStatus::None);
}

} // namespace

int main(){
	TestObservedRunningCompletion();
	TestFastCompletionWithoutRunningPoll();
	TestTimeoutAndCancel();
	TestResetAndDeadAgent();
	TestDetachWhileActive();
	std::cout << "ElemenTactics LLM request session smoke tests passed\n";
	return 0;
}
