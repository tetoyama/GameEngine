#pragma once

#include <algorithm>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

class JobScratchAllocator {
public:
	struct Marker { size_t blockIndex = 0; size_t offset = 0; };

	explicit JobScratchAllocator(size_t defaultBlockSize = 256 * 1024)
		: m_defaultBlockSize((std::max)(defaultBlockSize, size_t{1024})) {
		AddBlock(m_defaultBlockSize);
	}

	JobScratchAllocator(const JobScratchAllocator&) = delete;
	JobScratchAllocator& operator=(const JobScratchAllocator&) = delete;

	Marker Mark() const noexcept { return {m_currentBlock, m_offset}; }

	void Rewind(Marker marker) noexcept {
		assert(marker.blockIndex < m_blocks.size());
		m_currentBlock = marker.blockIndex;
		m_offset = marker.offset;
	}

	void Reset() noexcept { m_currentBlock = 0; m_offset = 0; }

	void* Allocate(size_t size, size_t alignment = alignof(std::max_align_t)) {
		if(size == 0) return nullptr;
		assert(alignment && (alignment & (alignment - 1)) == 0);
		for(;;) {
			Block& block = m_blocks[m_currentBlock];
			const size_t alignedOffset = AlignUp(m_offset, alignment);
			if(alignedOffset <= block.capacity && size <= block.capacity - alignedOffset) {
				void* result = block.memory.get() + alignedOffset;
				m_offset = alignedOffset + size;
				return result;
			}
			++m_currentBlock;
			m_offset = 0;
			if(m_currentBlock >= m_blocks.size()) {
				AddBlock((std::max)(m_defaultBlockSize, size + alignment));
			}
		}
	}

	template<typename T>
	T* AllocateArray(size_t count = 1) {
		return count ? static_cast<T*>(Allocate(sizeof(T) * count, alignof(T))) : nullptr;
	}

private:
	struct Block { std::unique_ptr<std::byte[]> memory; size_t capacity = 0; };
	static size_t AlignUp(size_t value, size_t alignment) noexcept {
		return (value + alignment - 1) & ~(alignment - 1);
	}
	void AddBlock(size_t capacity) {
		Block block;
		block.memory = std::make_unique<std::byte[]>(capacity);
		block.capacity = capacity;
		m_blocks.emplace_back(std::move(block));
	}

	std::vector<Block> m_blocks;
	size_t m_defaultBlockSize = 0;
	size_t m_currentBlock = 0;
	size_t m_offset = 0;
};

class JobCommandBuffer {
public:
	using Command = std::function<void()>;
	explicit JobCommandBuffer(size_t reserveCount = 64) { m_commands.reserve(reserveCount); }
	JobCommandBuffer(const JobCommandBuffer&) = delete;
	JobCommandBuffer& operator=(const JobCommandBuffer&) = delete;

	template<typename Function>
	void Enqueue(Function&& function) { m_commands.emplace_back(std::forward<Function>(function)); }
	bool Empty() const noexcept { return m_commands.empty(); }
	size_t Size() const noexcept { return m_commands.size(); }
	void Flush() {
		std::vector<Command> commands;
		commands.swap(m_commands);
		for(Command& command : commands) if(command) command();
	}
	void Clear() noexcept { m_commands.clear(); }

private:
	std::vector<Command> m_commands;
};

struct JobThreadContext {
	size_t workerIndex = static_cast<size_t>(-1);
	JobScratchAllocator scratch;
	JobCommandBuffer commands;
};

namespace JobSystemDetail {
struct FenceState {
	std::atomic<uint32_t> pending{0};
	mutable std::mutex mutex;
	std::condition_variable completed;
	std::exception_ptr exception;
};
}

class JobFence {
public:
	JobFence() : m_state(std::make_shared<JobSystemDetail::FenceState>()) {}
	bool IsComplete() const noexcept { return m_state->pending.load(std::memory_order_acquire) == 0; }
	uint32_t PendingCount() const noexcept { return m_state->pending.load(std::memory_order_acquire); }
private:
	friend class JobSystem;
	std::shared_ptr<JobSystemDetail::FenceState> m_state;
};

class JobSystem {
public:
	using JobFunction = std::function<void(JobThreadContext&)>;
	static constexpr size_t InvalidWorkerIndex = static_cast<size_t>(-1);

	JobSystem() = default;
	~JobSystem() { StopNoThrow(); }
	JobSystem(const JobSystem&) = delete;
	JobSystem& operator=(const JobSystem&) = delete;
	JobSystem(JobSystem&&) = delete;
	JobSystem& operator=(JobSystem&&) = delete;

	void Start(size_t workerCount = 0) {
		std::scoped_lock lifecycleLock(m_lifecycleMutex);
		if(m_running.load(std::memory_order_acquire)) return;
		if(workerCount == 0) {
			const unsigned hardwareThreads = std::thread::hardware_concurrency();
			workerCount = hardwareThreads > 1 ? static_cast<size_t>(hardwareThreads - 1) : 1;
		}

		assert(m_pendingJobs.load(std::memory_order_acquire) == 0);
		assert(m_availableJobs.load(std::memory_order_acquire) == 0);
		m_nextSubmission.store(0, std::memory_order_release);
		m_nextSteal.store(0, std::memory_order_release);
		m_mainContext.scratch.Reset();
		m_mainContext.commands.Clear();
		{
			std::scoped_lock lock(m_exceptionMutex);
			m_unhandledException = nullptr;
		}

		m_workers.clear();
		m_workers.reserve(workerCount);
		for(size_t index = 0; index < workerCount; ++index) {
			auto worker = std::make_unique<Worker>();
			worker->context.workerIndex = index;
			m_workers.emplace_back(std::move(worker));
		}

		m_ownerThread = std::this_thread::get_id();
		{
			std::scoped_lock wakeLock(m_wakeMutex);
			m_stopping.store(false, std::memory_order_release);
			m_accepting.store(true, std::memory_order_release);
		}
		m_running.store(true, std::memory_order_release);

		try {
			for(size_t index = 0; index < m_workers.size(); ++index) {
				m_workers[index]->thread = std::thread([this, index] { WorkerLoop(index); });
			}
		} catch(...) {
			{
				std::scoped_lock wakeLock(m_wakeMutex);
				m_accepting.store(false, std::memory_order_release);
				m_stopping.store(true, std::memory_order_release);
			}
			m_wakeCondition.notify_all();
			for(auto& worker : m_workers) if(worker->thread.joinable()) worker->thread.join();
			m_workers.clear();
			m_running.store(false, std::memory_order_release);
			m_stopping.store(false, std::memory_order_release);
			m_ownerThread = {};
			throw;
		}
	}

	void Stop() {
		if(IsExecutingJobOnThisThread()) {
			throw std::logic_error("JobSystem::Stop cannot be called from a currently executing job.");
		}

		std::unique_lock lifecycleLock(m_lifecycleMutex);
		if(!m_running.load(std::memory_order_acquire)) return;
		lifecycleLock.unlock();

		std::exception_ptr waitException;
		try { WaitIdle(); } catch(...) { waitException = std::current_exception(); }

		lifecycleLock.lock();
		{
			std::scoped_lock wakeLock(m_wakeMutex);
			m_accepting.store(false, std::memory_order_release);
			m_stopping.store(true, std::memory_order_release);
		}
		m_wakeCondition.notify_all();
		for(auto& worker : m_workers) if(worker->thread.joinable()) worker->thread.join();
		m_workers.clear();
		m_running.store(false, std::memory_order_release);
		m_stopping.store(false, std::memory_order_release);
		m_ownerThread = {};
		lifecycleLock.unlock();

		if(waitException) std::rethrow_exception(waitException);
	}

	bool IsRunning() const noexcept { return m_running.load(std::memory_order_acquire); }
	size_t WorkerCount() const noexcept { return m_workers.size(); }
	size_t PendingJobCount() const noexcept { return m_pendingJobs.load(std::memory_order_acquire); }
	size_t AvailableJobCount() const noexcept { return m_availableJobs.load(std::memory_order_acquire); }

	void Submit(JobFunction function) { SubmitInternal(std::move(function), nullptr); }
	void Submit(JobFence& fence, JobFunction function) { SubmitInternal(std::move(function), fence.m_state); }
	template<typename Function>
	void Submit(JobFence& fence, Function&& function) {
		Submit(fence, JobFunction(std::forward<Function>(function)));
	}

	void Wait(JobFence& fence) {
		const auto state = fence.m_state;
		for(;;) {
			if(state->pending.load(std::memory_order_acquire) == 0) break;
			if(TryExecuteOne(CurrentWorkerIndex())) continue;
			std::unique_lock lock(state->mutex);
			state->completed.wait(lock, [&state] {
				return state->pending.load(std::memory_order_acquire) == 0;
			});
		}

		std::exception_ptr exception;
		{
			std::scoped_lock lock(state->mutex);
			exception = state->exception;
		}
		if(exception) std::rethrow_exception(exception);
	}

	void WaitIdle() {
		if(IsExecutingJobOnThisThread()) {
			throw std::logic_error(
				"JobSystem::WaitIdle cannot be called from a currently executing job. Use JobFence::Wait for nested work."
			);
		}

		for(;;) {
			while(TryExecuteOne(InvalidWorkerIndex)) {}
			std::unique_lock lock(m_idleMutex);
			if(m_pendingJobs.load(std::memory_order_acquire) == 0) break;
			m_idleCondition.wait(lock, [this] {
				return m_pendingJobs.load(std::memory_order_acquire) == 0;
			});
		}

		std::exception_ptr exception;
		{
			std::scoped_lock lock(m_exceptionMutex);
			exception = std::exchange(m_unhandledException, nullptr);
		}
		if(exception) std::rethrow_exception(exception);
	}

	template<typename Function>
	void ParallelFor(size_t begin, size_t end, size_t minimumBatchSize, Function&& function) {
		if(begin >= end) return;
		if(!IsRunning()) {
			for(size_t index = begin; index < end; ++index) function(index);
			return;
		}

		minimumBatchSize = (std::max)(minimumBatchSize, size_t{1});
		const size_t itemCount = end - begin;
		const size_t preferredJobs = (std::max)(size_t{1}, WorkerCount() * size_t{4});
		const size_t jobCount = (std::min)(
			preferredJobs,
			(itemCount + minimumBatchSize - 1) / minimumBatchSize
		);
		const size_t batchSize = (itemCount + jobCount - 1) / jobCount;
		auto sharedFunction = std::make_shared<std::decay_t<Function>>(std::forward<Function>(function));
		JobFence fence;
		for(size_t batchBegin = begin; batchBegin < end; batchBegin += batchSize) {
			const size_t batchEnd = (std::min)(end, batchBegin + batchSize);
			Submit(fence, [sharedFunction, batchBegin, batchEnd](JobThreadContext&) {
				for(size_t index = batchBegin; index < batchEnd; ++index) (*sharedFunction)(index);
			});
		}
		Wait(fence);
	}

	JobThreadContext& CurrentContext() {
		if(s_currentSystem == this && s_currentWorker != InvalidWorkerIndex) {
			return m_workers[s_currentWorker]->context;
		}
		if(std::this_thread::get_id() != m_ownerThread) {
			throw std::logic_error("External threads must submit jobs instead of accessing Job context.");
		}
		return m_mainContext;
	}

	void FlushThreadCommands() {
		if(std::this_thread::get_id() != m_ownerThread) {
			throw std::logic_error("JobSystem::FlushThreadCommands must run on the owner thread.");
		}
		WaitIdle();
		m_mainContext.commands.Flush();
		for(auto& worker : m_workers) worker->context.commands.Flush();
	}

private:
	struct Job {
		JobFunction function;
		std::shared_ptr<JobSystemDetail::FenceState> fence;
	};
	struct Worker {
		std::mutex queueMutex;
		std::deque<Job> queue;
		std::thread thread;
		JobThreadContext context;
	};

	void SubmitInternal(JobFunction function, std::shared_ptr<JobSystemDetail::FenceState> fence) {
		if(!function) return;
		if(!m_accepting.load(std::memory_order_acquire) || m_workers.empty()) {
			throw std::runtime_error("JobSystem is not accepting jobs.");
		}

		const size_t workerIndex = SelectSubmissionWorker();
		Worker& worker = *m_workers[workerIndex];
		{
			std::unique_lock wakeLock(m_wakeMutex);
			if(!m_accepting.load(std::memory_order_acquire)) {
				throw std::runtime_error("JobSystem is stopping.");
			}
			std::unique_lock queueLock(worker.queueMutex);
			worker.queue.push_front(Job{std::move(function), fence});

			if(fence) {
				std::scoped_lock fenceLock(fence->mutex);
				if(fence->pending.load(std::memory_order_acquire) == 0) fence->exception = nullptr;
				fence->pending.fetch_add(1, std::memory_order_release);
			}
			{
				std::scoped_lock idleLock(m_idleMutex);
				m_pendingJobs.fetch_add(1, std::memory_order_release);
			}
			m_availableJobs.fetch_add(1, std::memory_order_release);
		}
		m_wakeCondition.notify_one();
	}

	size_t SelectSubmissionWorker() noexcept {
		if(s_currentSystem == this && s_currentWorker != InvalidWorkerIndex) return s_currentWorker;
		return m_nextSubmission.fetch_add(1, std::memory_order_relaxed) % m_workers.size();
	}

	void WorkerLoop(size_t workerIndex) {
		s_currentSystem = this;
		s_currentWorker = workerIndex;
		for(;;) {
			if(TryExecuteOne(workerIndex)) continue;
			std::unique_lock lock(m_wakeMutex);
			m_wakeCondition.wait(lock, [this] {
				return m_stopping.load(std::memory_order_acquire) ||
					m_availableJobs.load(std::memory_order_acquire) != 0;
			});
			if(m_stopping.load(std::memory_order_acquire) &&
				m_pendingJobs.load(std::memory_order_acquire) == 0) break;
		}
		s_currentWorker = InvalidWorkerIndex;
		s_currentSystem = nullptr;
	}

	bool TryExecuteOne(size_t preferredWorker) {
		Job job;
		if(!TryAcquireJob(preferredWorker, job)) return false;
		JobThreadContext& context =
			preferredWorker != InvalidWorkerIndex && preferredWorker < m_workers.size()
				? m_workers[preferredWorker]->context
				: m_mainContext;
		ExecuteJob(job, context);
		return true;
	}

	bool TryAcquireJob(size_t preferredWorker, Job& output) {
		if(preferredWorker != InvalidWorkerIndex && preferredWorker < m_workers.size() &&
			TryPopOwn(preferredWorker, output)) return true;
		const size_t workerCount = m_workers.size();
		if(workerCount == 0) return false;
		const size_t start = m_nextSteal.fetch_add(1, std::memory_order_relaxed) % workerCount;
		for(size_t offset = 0; offset < workerCount; ++offset) {
			const size_t victim = (start + offset) % workerCount;
			if(victim != preferredWorker && TrySteal(victim, output)) return true;
		}
		return false;
	}

	bool TryPopOwn(size_t workerIndex, Job& output) {
		Worker& worker = *m_workers[workerIndex];
		std::scoped_lock lock(worker.queueMutex);
		if(worker.queue.empty()) return false;
		output = std::move(worker.queue.front());
		worker.queue.pop_front();
		const size_t previous = m_availableJobs.fetch_sub(1, std::memory_order_acq_rel);
		assert(previous != 0 && "available job counter underflow");
		return true;
	}

	bool TrySteal(size_t workerIndex, Job& output) {
		Worker& worker = *m_workers[workerIndex];
		std::unique_lock lock(worker.queueMutex, std::try_to_lock);
		if(!lock.owns_lock() || worker.queue.empty()) return false;
		output = std::move(worker.queue.back());
		worker.queue.pop_back();
		const size_t previous = m_availableJobs.fetch_sub(1, std::memory_order_acq_rel);
		assert(previous != 0 && "available job counter underflow");
		return true;
	}

	void ExecuteJob(Job& job, JobThreadContext& context) noexcept {
		const JobScratchAllocator::Marker scratchMarker = context.scratch.Mark();
		JobSystem* previousExecutingSystem = s_executingSystem;
		const size_t previousExecutionDepth = s_executionDepth;
		s_executingSystem = this;
		s_executionDepth = previousExecutionDepth + 1;

		std::exception_ptr exception;
		try { job.function(context); } catch(...) { exception = std::current_exception(); }

		s_executionDepth = previousExecutionDepth;
		s_executingSystem = previousExecutingSystem;
		context.scratch.Rewind(scratchMarker);

		if(job.fence) {
			bool fenceCompleted = false;
			{
				std::scoped_lock lock(job.fence->mutex);
				if(exception && !job.fence->exception) job.fence->exception = exception;
				const uint32_t previous = job.fence->pending.fetch_sub(1, std::memory_order_acq_rel);
				assert(previous != 0 && "fence pending counter underflow");
				fenceCompleted = previous == 1;
			}
			if(fenceCompleted) job.fence->completed.notify_all();
		} else if(exception) {
			std::scoped_lock lock(m_exceptionMutex);
			if(!m_unhandledException) m_unhandledException = exception;
		}

		bool becameIdle = false;
		{
			std::scoped_lock lock(m_idleMutex);
			const size_t previous = m_pendingJobs.fetch_sub(1, std::memory_order_acq_rel);
			assert(previous != 0 && "pending job counter underflow");
			becameIdle = previous == 1;
		}
		if(becameIdle) m_idleCondition.notify_all();
	}

	size_t CurrentWorkerIndex() const noexcept {
		return s_currentSystem == this ? s_currentWorker : InvalidWorkerIndex;
	}
	bool IsExecutingJobOnThisThread() const noexcept {
		return s_executingSystem == this && s_executionDepth != 0;
	}
	void StopNoThrow() noexcept {
		try { Stop(); } catch(...) {}
	}

	std::vector<std::unique_ptr<Worker>> m_workers;
	JobThreadContext m_mainContext;
	std::atomic<bool> m_running{false};
	std::atomic<bool> m_accepting{false};
	std::atomic<bool> m_stopping{false};
	std::atomic<size_t> m_nextSubmission{0};
	std::atomic<size_t> m_nextSteal{0};
	std::atomic<size_t> m_pendingJobs{0};
	std::atomic<size_t> m_availableJobs{0};
	std::mutex m_lifecycleMutex;
	std::mutex m_wakeMutex;
	std::condition_variable m_wakeCondition;
	std::mutex m_idleMutex;
	std::condition_variable m_idleCondition;
	std::mutex m_exceptionMutex;
	std::exception_ptr m_unhandledException;
	std::thread::id m_ownerThread;

	inline static thread_local JobSystem* s_currentSystem = nullptr;
	inline static thread_local size_t s_currentWorker = InvalidWorkerIndex;
	inline static thread_local JobSystem* s_executingSystem = nullptr;
	inline static thread_local size_t s_executionDepth = 0;
};
