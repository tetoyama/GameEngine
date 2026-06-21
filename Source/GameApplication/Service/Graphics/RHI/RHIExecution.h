#pragma once

#include <cstdint>
#include <memory>
#include <span>

#include "RHIHandle.h"

namespace RHI {

class IRHICommandList;

enum class CommandQueueType : uint8_t {
	Graphics,
	Compute,
	Copy
};

struct CommandListCreateDesc {
	CommandQueueType queueType = CommandQueueType::Graphics;
	bool secondary = false;
};

struct QueueSubmitDesc {
	std::span<IRHICommandList* const> commandLists;
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
