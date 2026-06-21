#include "System/Job/JobSystem.h"

#include <atomic>
#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <thread>

namespace {

void TestFenceAndParallelFor(){
	JobSystem jobs;
	jobs.Start(3);

	std::atomic<int> completed{0};
	JobFence fence;
	for(int index = 0; index < 128; ++index){
		jobs.Submit(fence, [&completed](JobThreadContext& context){
			int* scratch = context.scratch.AllocateArray<int>(1);
			*scratch = 1;
			completed.fetch_add(*scratch, std::memory_order_relaxed);
		});
	}
	jobs.Wait(fence);
	assert(completed.load(std::memory_order_relaxed) == 128);

	std::atomic<size_t> sum{0};
	jobs.ParallelFor(0, 1000, 17, [&sum](size_t index){
		sum.fetch_add(index, std::memory_order_relaxed);
	});
	assert(sum.load(std::memory_order_relaxed) == 999 * 1000 / 2);

	jobs.Stop();
}

void TestNestedWaitAndThreadCommands(){
	JobSystem jobs;
	jobs.Start(2);

	std::atomic<int> nestedCount{0};
	std::atomic<int> commandCount{0};
	JobFence outerFence;

	jobs.Submit(outerFence, [&jobs, &nestedCount, &commandCount](JobThreadContext&){
		JobFence nestedFence;
		for(int index = 0; index < 64; ++index){
			jobs.Submit(nestedFence, [&nestedCount, &commandCount](JobThreadContext& context){
				nestedCount.fetch_add(1, std::memory_order_relaxed);
				context.commands.Enqueue([&commandCount]{
					commandCount.fetch_add(1, std::memory_order_relaxed);
				});
			});
		}
		jobs.Wait(nestedFence);
	});

	jobs.Wait(outerFence);
	assert(nestedCount.load(std::memory_order_relaxed) == 64);
	assert(commandCount.load(std::memory_order_relaxed) == 0);

	jobs.FlushThreadCommands();
	assert(commandCount.load(std::memory_order_relaxed) == 64);
	jobs.Stop();
}

void TestFenceExceptionAndReuse(){
	JobSystem jobs;
	jobs.Start(2);

	JobFence fence;
	jobs.Submit(fence, [](JobThreadContext&){
		throw std::runtime_error("expected fence failure");
	});

	bool caught = false;
	try{
		jobs.Wait(fence);
	} catch(const std::runtime_error&){
		caught = true;
	}
	assert(caught);

	std::atomic<bool> reused{false};
	jobs.Submit(fence, [&reused](JobThreadContext&){
		reused.store(true, std::memory_order_release);
	});
	jobs.Wait(fence);
	assert(reused.load(std::memory_order_acquire));

	jobs.Stop();
}

void TestUnhandledException(){
	JobSystem jobs;
	jobs.Start(2);
	jobs.Submit([](JobThreadContext&){
		throw std::runtime_error("expected unhandled failure");
	});

	bool caught = false;
	try{
		jobs.WaitIdle();
	} catch(const std::runtime_error&){
		caught = true;
	}
	assert(caught);
	jobs.Stop();
}

void TestDrainAllowsChildJobs(){
	JobSystem jobs;
	jobs.Start(2);

	std::atomic<bool> parentStarted{false};
	std::atomic<bool> allowChildSubmit{false};
	std::atomic<bool> childCompleted{false};

	jobs.Submit([&](JobThreadContext&){
		parentStarted.store(true, std::memory_order_release);
		while(!allowChildSubmit.load(std::memory_order_acquire)){
			std::this_thread::yield();
		}

		jobs.Submit([&childCompleted](JobThreadContext&){
			childCompleted.store(true, std::memory_order_release);
		});
	});

	while(!parentStarted.load(std::memory_order_acquire)){
		std::this_thread::yield();
	}

	allowChildSubmit.store(true, std::memory_order_release);
	jobs.Stop();
	assert(childCompleted.load(std::memory_order_acquire));
}

void TestRestartAndSerialFallback(){
	JobSystem jobs;

	std::atomic<int> serialCount{0};
	jobs.ParallelFor(0, 16, 4, [&serialCount](size_t){
		serialCount.fetch_add(1, std::memory_order_relaxed);
	});
	assert(serialCount.load(std::memory_order_relaxed) == 16);

	for(int cycle = 0; cycle < 2; ++cycle){
		jobs.Start(2);
		std::atomic<bool> completed{false};
		JobFence fence;
		jobs.Submit(fence, [&completed](JobThreadContext&){
			completed.store(true, std::memory_order_release);
		});
		jobs.Wait(fence);
		assert(completed.load(std::memory_order_acquire));
		jobs.Stop();
	}
}

// WaitIdleのpredicate確認直後と最後のJob完了通知が競合しても、
// notifyを取りこぼして永久待機しないことを反復検証する。
void TestWaitIdleLostWakeupStress(){
	JobSystem jobs;
	jobs.Start(4);

	std::atomic<size_t> completed{0};
	constexpr size_t Iterations = 5000;
	for(size_t iteration = 0; iteration < Iterations; ++iteration){
		jobs.Submit([&completed](JobThreadContext&){
			completed.fetch_add(1, std::memory_order_relaxed);
		});
		jobs.WaitIdle();
		assert(jobs.PendingJobCount() == 0);
		assert(jobs.AvailableJobCount() == 0);
	}

	assert(completed.load(std::memory_order_relaxed) == Iterations);
	jobs.Stop();
}

// Job自身はm_pendingJobsに含まれるため、そのJob内でWaitIdleすると
// 自分自身の完了を待つ再入デッドロックになる。明示的に拒否する。
void TestWaitIdleRejectedInsideJob(){
	JobSystem jobs;
	jobs.Start(2);

	std::atomic<bool> rejected{false};
	JobFence fence;
	jobs.Submit(fence, [&jobs, &rejected](JobThreadContext&){
		try{
			jobs.WaitIdle();
		} catch(const std::logic_error&){
			rejected.store(true, std::memory_order_release);
		}
	});
	jobs.Wait(fence);

	assert(rejected.load(std::memory_order_acquire));
	assert(jobs.PendingJobCount() == 0);
	jobs.Stop();
}

} // namespace

int main(){
	TestFenceAndParallelFor();
	TestNestedWaitAndThreadCommands();
	TestFenceExceptionAndReuse();
	TestUnhandledException();
	TestDrainAllowsChildJobs();
	TestRestartAndSerialFallback();
	TestWaitIdleLostWakeupStress();
	TestWaitIdleRejectedInsideJob();
	return 0;
}
