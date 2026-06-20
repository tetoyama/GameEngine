// =======================================================================
//
// SystemScheduleCompiler.h
//
// =======================================================================
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "System/Scheduler/SystemTask.h"

struct SystemScheduleNode {
	size_t taskIndex = 0;
	std::vector<size_t> dependencies;
	std::vector<size_t> dependents;
};

struct CompiledSystemSchedule {
	SystemTaskDomain domain = SystemTaskDomain::Frame;
	std::vector<SystemScheduleNode> nodes;
	std::vector<size_t> executionOrder;
	bool valid = true;
	std::string error;
};

// Phase / Priority / RegistrationOrderで論理順を確定し、
// Access競合があるTask間だけ依存辺を生成する。
// 明示的Before / Afterは扱わない。
class SystemScheduleCompiler {
public:
	static CompiledSystemSchedule Compile(
		const std::vector<SystemTask>& tasks,
		SystemTaskDomain domain
	) {
		CompiledSystemSchedule schedule;
		schedule.domain = domain;

		std::vector<size_t> orderedTaskIndices;
		for(size_t index = 0; index < tasks.size(); ++index) {
			if(tasks[index].domain == domain) {
				orderedTaskIndices.push_back(index);
			}
		}

		std::sort(
			orderedTaskIndices.begin(),
			orderedTaskIndices.end(),
			[&tasks](size_t lhsIndex, size_t rhsIndex) {
				return IsEarlier(tasks[lhsIndex], tasks[rhsIndex]);
			}
		);

		schedule.nodes.reserve(orderedTaskIndices.size());
		for(size_t taskIndex : orderedTaskIndices) {
			SystemScheduleNode node;
			node.taskIndex = taskIndex;
			schedule.nodes.emplace_back(std::move(node));
		}

		// 論理順で前にあるTaskから後ろのTaskへだけ辺を張る。
		// そのためAccess由来のGraphは構造上DAGになる。
		for(size_t earlier = 0; earlier < schedule.nodes.size(); ++earlier) {
			const SystemTask& earlierTask =
				tasks[schedule.nodes[earlier].taskIndex];

			for(size_t later = earlier + 1; later < schedule.nodes.size(); ++later) {
				const SystemTask& laterTask =
					tasks[schedule.nodes[later].taskIndex];

				if(!earlierTask.access.ConflictsWith(laterTask.access)) {
					continue;
				}

				schedule.nodes[earlier].dependents.push_back(later);
				schedule.nodes[later].dependencies.push_back(earlier);
			}
		}

		BuildTopologicalOrder(tasks, schedule);
		return schedule;
	}

private:
	static bool IsEarlier(const SystemTask& lhs, const SystemTask& rhs) {
		if(lhs.order.phase != rhs.order.phase) {
			return static_cast<int32_t>(lhs.order.phase) <
				static_cast<int32_t>(rhs.order.phase);
		}

		if(lhs.order.priority != rhs.order.priority) {
			return lhs.order.priority < rhs.order.priority;
		}

		return lhs.order.registrationOrder < rhs.order.registrationOrder;
	}

	static void BuildTopologicalOrder(
		const std::vector<SystemTask>& tasks,
		CompiledSystemSchedule& schedule
	) {
		std::vector<size_t> unresolvedDependencies;
		unresolvedDependencies.reserve(schedule.nodes.size());

		std::vector<size_t> readyNodes;
		for(size_t index = 0; index < schedule.nodes.size(); ++index) {
			const size_t dependencyCount =
				schedule.nodes[index].dependencies.size();
			unresolvedDependencies.push_back(dependencyCount);
			if(dependencyCount == 0) {
				readyNodes.push_back(index);
			}
		}

		schedule.executionOrder.clear();
		schedule.executionOrder.reserve(schedule.nodes.size());

		while(!readyNodes.empty()) {
			// Ready Taskが複数ある場合も論理順で決定する。
			auto earliestIt = std::min_element(
				readyNodes.begin(),
				readyNodes.end(),
				[&](size_t lhsNode, size_t rhsNode) {
					return IsEarlier(
						tasks[schedule.nodes[lhsNode].taskIndex],
						tasks[schedule.nodes[rhsNode].taskIndex]
					);
				}
			);

			const size_t nodeIndex = *earliestIt;
			readyNodes.erase(earliestIt);
			schedule.executionOrder.push_back(
				schedule.nodes[nodeIndex].taskIndex
			);

			for(size_t dependent : schedule.nodes[nodeIndex].dependents) {
				if(unresolvedDependencies[dependent] == 0) {
					continue;
				}

				--unresolvedDependencies[dependent];
				if(unresolvedDependencies[dependent] == 0) {
					readyNodes.push_back(dependent);
				}
			}
		}

		if(schedule.executionOrder.size() != schedule.nodes.size()) {
			schedule.valid = false;
			schedule.error = "System schedule contains a dependency cycle.";
			schedule.executionOrder.clear();
		}
	}
};
