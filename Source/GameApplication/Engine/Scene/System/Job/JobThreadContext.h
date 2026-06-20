// =======================================================================
//
// JobThreadContext.h
//
// WorkerごとのScratch Allocatorと遅延Command Buffer。
//
// =======================================================================
#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

class JobScratchAllocator {
public:
	struct Marker {
		size_t blockIndex = 0;
		size_t offset = 0;
	};

	explicit JobScratchAllocator(size_t defaultBlockSize = 256 * 1024)
		: m_defaultBlockSize((std::max)(defaultBlockSize, size_t{1024})) {
		AddBlock(m_defaultBlockSize);
	}

	JobScratchAllocator(const JobScratchAllocator&) = delete;
	JobScratchAllocator& operator=(const JobScratchAllocator&) = delete;

	Marker Mark() const noexcept {
		return {m_currentBlock, m_offset};
	}

	void Rewind(Marker marker) noexcept {
		assert(marker.blockIndex < m_blocks.size());
		m_currentBlock = marker.blockIndex;
		m_offset = marker.offset;
	}

	void Reset() noexcept {
		m_currentBlock = 0;
		m_offset = 0;
	}

	void* Allocate(size_t size, size_t alignment = alignof(std::max_align_t)) {
		if(size == 0) return nullptr;
		assert(alignment != 0 && (alignment & (alignment - 1)) == 0);

		for(;;) {
			Block& block = m_blocks[m_currentBlock];
			const size_t alignedOffset = AlignUp(m_offset, alignment);
			if(alignedOffset <= block.capacity &&
				size <= block.capacity - alignedOffset) {
				void* result = block.memory.get() + alignedOffset;
				m_offset = alignedOffset + size;
				return result;
			}

			++m_currentBlock;
			m_offset = 0;
			if(m_currentBlock >= m_blocks.size()) {
				const size_t required = size + alignment;
				AddBlock((std::max)(m_defaultBlockSize, required));
			}
		}
	}

	template<typename T>
	T* AllocateArray(size_t count = 1) {
		if(count == 0) return nullptr;
		return static_cast<T*>(Allocate(sizeof(T) * count, alignof(T)));
	}

private:
	struct Block {
		std::unique_ptr<std::byte[]> memory;
		size_t capacity = 0;
	};

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

	explicit JobCommandBuffer(size_t reserveCount = 64) {
		m_commands.reserve(reserveCount);
	}

	JobCommandBuffer(const JobCommandBuffer&) = delete;
	JobCommandBuffer& operator=(const JobCommandBuffer&) = delete;

	template<typename Function>
	void Enqueue(Function&& function) {
		m_commands.emplace_back(std::forward<Function>(function));
	}

	bool Empty() const noexcept {
		return m_commands.empty();
	}

	size_t Size() const noexcept {
		return m_commands.size();
	}

	void Flush() {
		std::vector<Command> commands;
		commands.swap(m_commands);
		for(Command& command : commands) {
			if(command) command();
		}
	}

	void Clear() noexcept {
		m_commands.clear();
	}

private:
	std::vector<Command> m_commands;
};

struct JobThreadContext {
	static constexpr size_t InvalidWorkerIndex = static_cast<size_t>(-1);

	size_t workerIndex = InvalidWorkerIndex;
	JobScratchAllocator scratch;
	JobCommandBuffer commands;
};
