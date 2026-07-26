// =======================================================================
//
// FlowContext.h
//
// 再帰Flow間で不変のRoot Goalと親子関係を共有する。
// Tool実行中の現在Taskも同じthread上で公開し、Command監視と
// CreateChildFlowが「何のための子Flowか」を見失わないようにする。
//
// =======================================================================
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "../AgentOsTypes.h"

namespace agentos {

class BudgetTracker;

struct FlowContext {
	bool active = false;
	SessionId sessionId = kInvalidId;
	SessionId rootSessionId = kInvalidId;
	SessionId parentSessionId = kInvalidId;
	int depth = 0;
	int maxDepth = 3;

	// rootGoal / rootResolvedRequest は子Flowから変更しない。
	std::string rootGoal;
	std::string rootResolvedRequest;

	// parentTask はこのFlowを生成した親Task、currentTaskは現在実行中のTask。
	std::string parentTask;
	std::string currentTask;
	std::vector<std::string> ancestorTasks;

	// 子Flowも親と同じ総Budgetを消費する。再帰でBudgetをリセットしない。
	BudgetTracker* sharedBudget = nullptr;
};

inline FlowContext& MutableCurrentFlowContext() {
	// 関数内static thread_localにして、スレッド起動時の動的初期化を避ける。
	static thread_local FlowContext value;
	return value;
}

inline const FlowContext& CurrentFlowContext() {
	return MutableCurrentFlowContext();
}

inline bool HasCurrentFlowContext() {
	return CurrentFlowContext().active;
}

class ScopedFlowContext {
public:
	explicit ScopedFlowContext(FlowContext next)
		: previous_(CurrentFlowContext()) {
		MutableCurrentFlowContext() = std::move(next);
	}

	~ScopedFlowContext() {
		MutableCurrentFlowContext() = std::move(previous_);
	}

	ScopedFlowContext(const ScopedFlowContext&) = delete;
	ScopedFlowContext& operator=(const ScopedFlowContext&) = delete;

private:
	FlowContext previous_;
};

class ScopedCurrentFlowTask {
public:
	explicit ScopedCurrentFlowTask(std::string task)
		: previous_(CurrentFlowContext().currentTask) {
		MutableCurrentFlowContext().currentTask = std::move(task);
	}

	~ScopedCurrentFlowTask() {
		MutableCurrentFlowContext().currentTask = std::move(previous_);
	}

	ScopedCurrentFlowTask(const ScopedCurrentFlowTask&) = delete;
	ScopedCurrentFlowTask& operator=(const ScopedCurrentFlowTask&) = delete;

private:
	std::string previous_;
};

} // namespace agentos
