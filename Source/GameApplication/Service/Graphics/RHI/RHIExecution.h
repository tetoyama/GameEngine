#pragma once

#include <cstdint>
#include <memory>
#include <span>

#include "RHIHandle.h"

namespace RHI {

class IRHICommandList;
class IRHISwapChain;

struct CommandListCreateDesc {
	CommandQueueType queueType = CommandQueueType::Graphics;
	bool secondary = false;
};

struct QueueFenceWait {
	FenceHandle fence;
	uint64_t value = 0;
};

struct QueueFenceSignal {
	FenceHandle fence;
	uint64_t value = 0;
};

// After Submit returns, callers may release command-list wrappers.
// Backends retain native submission state until GPU completion.
// Fence states are Device-owned until DestroyFence or Device destruction.
struct QueueSubmitDesc {
	std::span<IRHICommandList* const> commandLists;
	std::span<const QueueFenceWait> waits;
	std::span<const QueueFenceSignal> signals;

	// Compatibility path for callers that submit one wait and one signal.
	FenceHandle waitFence;
	uint64_t waitValue = 0;
	FenceHandle signalFence;
	uint64_t signalValue = 0;
};

class IRHICommandQueue {
public:
	virtual ~IRHICommandQueue() = default;
	virtual CommandQueueType GetType() const noexcept = 0;
	virtual bool Submit(const QueueSubmitDesc& desc) = 0;
	virtual bool Present(IRHISwapChain& swapChain, bool verticalSync) = 0;
	virtual void WaitIdle() = 0;
};

class IRHIFence {
public:
	virtual ~IRHIFence() = default;
	virtual FenceHandle GetHandle() const noexcept = 0;
	virtual uint64_t GetCompletedValue() const = 0;
	virtual bool Wait(uint64_t value, uint64_t timeoutNanoseconds) = 0;
};

} // namespace RHI
