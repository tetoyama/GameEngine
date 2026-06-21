#include "System/Scheduler/ExecuteDependencySchedule.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace {

struct ResourceA {};
struct ResourceB {};
struct FailureResource {};

SystemTask MakeTask(
	const char* name,
	uint64_t registrationOrder,
	SystemAccess access,
	ThreadAffinity affinity,
	std::function<void(const SystemTaskContext&)> execute
) {
	SystemTask task;
	task.name = name;
	task.domain = SystemTaskDomain::Frame;
	task.order.registrationOrder = registrationOrder;
	task.access = std::move(access);
	task.threadAffinity = affinity;
	task.execute = std::move(execute);
	return task;
}

void WaitForParallelStart(const std::atomic<int>& started) {
	const auto deadline =
		std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while(started.load(std::memory_order_acquire) < 2 &&
		std::chrono::steady_clock::now() < deadline) {
		std::this_thread::yield();
	}
	assert(started.load(std::memory_order_acquire) == 2);
}

void TestParallelReadyNodesAndMainJoin() {
	JobSystem jobs;
	jobs.Start(2);

	const std::thread::id mainThread = std::this_thread::get_id();
	std::atomic<int> started{0};
	std::atomic<int> completed{0};
	std::atomic<int> flushedCommands{0};
	bool joinedOnMainThread = false;

	auto workerTask = [&](const SystemTaskContext& context) {
		assert(context.jobContext != nullptr);
		assert(context.jobContext->workerIndex != JobSystem::InvalidWorkerIndex);

		started.fetch_add(1, std::memory_order_acq_rel);
		WaitForParallelStart(started);
		completed.fetch_add(1, std::memory_order_acq_rel);

		context.jobContext->commands.Enqueue([&flushedCommands] {
			flushedCommands.fetch_add(1, std::memory_order_relaxed);
		});
	};

	SystemAccess accessA;
	accessA.WriteResource<ResourceA>();
	SystemAccess accessB;
	accessB.WriteResource<ResourceB>();
	SystemAccess joinAccess;
	joinAccess
		.ReadResource<ResourceA>()
		.ReadResource<ResourceB>()
		.SetStructuralAccess(StructuralAccess::ExclusiveWorldWrite);

	std::vector<SystemTask> tasks;
	tasks.emplace_back(MakeTask(
		"WorkerA",
		0,
		std::move(accessA),
		ThreadAffinity::AnyWorker,
		workerTask
	));
	tasks.emplace_back(MakeTask(
		"WorkerB",
		1,
		std::move(accessB),
		ThreadAffinity::AnyWorker,
		workerTask
	));
	tasks.emplace_back(MakeTask(
		"MainJoin",
		2,
		std::move(joinAccess),
		ThreadAffinity::MainThread,
		[&](const SystemTaskContext& context) {
			assert(std::this_thread::get_id() == mainThread);
			assert(context.jobContext != nullptr);
			assert(context.jobContext->workerIndex == JobSystem::InvalidWorkerIndex);
			assert(completed.load(std::memory_order_acquire) == 2);
			assert(flushedCommands.load(std::memory_order_relaxed) == 2);
			joinedOnMainThread = true;
		}
	));

	const CompiledSystemSchedule schedule =
		SystemScheduleCompiler::Compile(tasks, SystemTaskDomain::Frame);
	assert(schedule.valid);
	assert(schedule.nodes.size() == 3);

	ExecuteDependencySchedule(
		schedule,
		tasks,
		jobs,
		SystemTaskContext{1.0f / 60.0f}
	);

	assert(joinedOnMainThread);
	assert(flushedCommands.load(std::memory_order_relaxed) == 2);
	jobs.Stop();
}

void TestExceptionDrainsGraphAndSkipsDependents() {
	JobSystem jobs;
	jobs.Start(2);

	bool dependentExecuted = false;
	std::atomic<int> failedTaskCommands{0};

	SystemAccess failureAccess;
	failureAccess.WriteResource<FailureResource>();
	SystemAccess dependentAccess;
	dependentAccess
		.ReadResource<FailureResource>()
		.SetStructuralAccess(StructuralAccess::ExclusiveWorldWrite);

	std::vector<SystemTask> tasks;
	tasks.emplace_back(MakeTask(
		"Failure",
		0,
		std::move(failureAccess),
		ThreadAffinity::AnyWorker,
		[&](const SystemTaskContext& context) {
			assert(context.jobContext != nullptr);
			context.jobContext->commands.Enqueue([&failedTaskCommands] {
				failedTaskCommands.fetch_add(1, std::memory_order_relaxed);
			});
			throw std::runtime_error("expected schedule failure");
		}
	));
	tasks.emplace_back(MakeTask(
		"Dependent",
		1,
		std::move(dependentAccess),
		ThreadAffinity::MainThread,
		[&](const SystemTaskContext&) {
			dependentExecuted = true;
		}
	));

	const CompiledSystemSchedule schedule =
		SystemScheduleCompiler::Compile(tasks, SystemTaskDomain::Frame);
	assert(schedule.valid);

	bool caught = false;
	try {
		ExecuteDependencySchedule(schedule, tasks, jobs, SystemTaskContext{});
	} catch(const std::runtime_error&) {
		caught = true;
	}

	assert(caught);
	assert(!dependentExecuted);
	assert(failedTaskCommands.load(std::memory_order_relaxed) == 0);
	jobs.Stop();
}

void TestSerialFallback() {
	JobSystem jobs;
	const std::thread::id mainThread = std::this_thread::get_id();
	bool executed = false;

	std::vector<SystemTask> tasks;
	tasks.emplace_back(MakeTask(
		"SerialFallback",
		0,
		SystemAccess{},
		ThreadAffinity::AnyWorker,
		[&](const SystemTaskContext& context) {
			assert(std::this_thread::get_id() == mainThread);
			assert(context.jobContext == nullptr);
			executed = true;
		}
	));

	const CompiledSystemSchedule schedule =
		SystemScheduleCompiler::Compile(tasks, SystemTaskDomain::Frame);
	assert(schedule.valid);

	ExecuteDependencySchedule(schedule, tasks, jobs, SystemTaskContext{});
	assert(executed);
}

} // namespace

int main() {
	TestParallelReadyNodesAndMainJoin();
	TestExceptionDrainsGraphAndSkipsDependents();
	TestSerialFallback();
	return 0;
}
