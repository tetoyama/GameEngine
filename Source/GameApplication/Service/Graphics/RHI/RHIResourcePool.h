// =======================================================================
// RHIResourcePool.h
// =======================================================================
#pragma once

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "RHIHandle.h"

namespace RHI {

template<typename HandleType, typename ResourceType>
class ResourcePool {
private:
	struct Slot {
		std::optional<ResourceType> resource;
		uint32_t generation = 1;
	};

public:
	template<typename... Args>
	HandleType Create(Args&&... args){
		for(uint32_t index = 0; index < m_slots.size(); ++index){
			Slot& slot = m_slots[index];
			if(slot.resource) continue;

			slot.resource.emplace(std::forward<Args>(args)...);
			++m_aliveCount;
			return HandleType{index, slot.generation};
		}

		Slot slot;
		slot.resource.emplace(std::forward<Args>(args)...);
		m_slots.emplace_back(std::move(slot));
		++m_aliveCount;
		return HandleType{
			static_cast<uint32_t>(m_slots.size() - 1),
			m_slots.back().generation
		};
	}

	bool Destroy(HandleType handle){
		Slot* slot = FindSlot(handle);
		if(!slot) return false;

		slot->resource.reset();
		++slot->generation;
		if(slot->generation == 0) slot->generation = 1;
		--m_aliveCount;
		return true;
	}

	ResourceType* TryGet(HandleType handle){
		Slot* slot = FindSlot(handle);
		return slot ? &slot->resource.value() : nullptr;
	}

	const ResourceType* TryGet(HandleType handle) const {
		const Slot* slot = FindSlot(handle);
		return slot ? &slot->resource.value() : nullptr;
	}

	bool IsAlive(HandleType handle) const {
		return FindSlot(handle) != nullptr;
	}

	size_t AliveCount() const noexcept {
		return m_aliveCount;
	}

	void Clear(){
		for(Slot& slot : m_slots){
			slot.resource.reset();
			++slot.generation;
			if(slot.generation == 0) slot.generation = 1;
		}
		m_aliveCount = 0;
	}

private:
	Slot* FindSlot(HandleType handle){
		if(!handle || handle.index >= m_slots.size()) return nullptr;
		Slot& slot = m_slots[handle.index];
		if(slot.generation != handle.generation || !slot.resource) return nullptr;
		return &slot;
	}

	const Slot* FindSlot(HandleType handle) const {
		if(!handle || handle.index >= m_slots.size()) return nullptr;
		const Slot& slot = m_slots[handle.index];
		if(slot.generation != handle.generation || !slot.resource) return nullptr;
		return &slot;
	}

	std::vector<Slot> m_slots;
	size_t m_aliveCount = 0;
};

} // namespace RHI
