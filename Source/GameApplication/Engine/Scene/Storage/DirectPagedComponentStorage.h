// =======================================================================
//
// DirectPagedComponentStorage.h
//
// =======================================================================
#pragma once

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "Scene/Interface/IComponentStorage.h"

namespace ECSStorage {

// Entity indexからPage / Slotへ直接到達する実データComponent用Storage。
// Page本体はunique_ptrで保持するため、Page tableの再配置でComponentアドレスは移動しない。
template<typename T, std::uint32_t PageSize = 256>
class DirectPagedComponentStorage final: public IComponentStorage {
	static_assert(PageSize > 0, "DirectPaged PageSize must be greater than zero");

public:
	static constexpr std::uint32_t kPageSize = PageSize;

	void Add(Entity entity, T component){
		Page& page = EnsurePage(PageIndex(entity));
		Slot& slot = page.slots[SlotIndex(entity)];

		if(slot.occupied && slot.entityGeneration == entity.GetGeneration()){
			return;
		}

		// 同じindexへ古いEntity generationのComponentが残っている場合は破棄する。
		if(slot.occupied){
			slot.value.reset();
			IncrementComponentGeneration(slot);
			--m_size;
		}

		slot.value.emplace(std::move(component));
		slot.entityGeneration = entity.GetGeneration();
		slot.occupied = true;
		page.occupied.set(SlotIndex(entity));
		++m_size;
		++m_structureVersion;
	}

	T* Get(Entity entity){
		Slot* slot = FindSlot(entity);
		return slot && slot->value ? &slot->value.value() : nullptr;
	}

	const T* Get(Entity entity) const {
		const Slot* slot = FindSlot(entity);
		return slot && slot->value ? &slot->value.value() : nullptr;
	}

	void Remove(Entity entity) override {
		Slot* slot = FindSlot(entity);
		if(!slot) return;

		Page* page = FindPage(PageIndex(entity));
		if(!page) return;

		slot->value.reset();
		slot->occupied = false;
		page->occupied.reset(SlotIndex(entity));
		IncrementComponentGeneration(*slot);
		--m_size;
		++m_structureVersion;
	}

	bool Contains(Entity entity) const override {
		return FindSlot(entity) != nullptr;
	}

	void* GetRaw(Entity entity) override {
		return Get(entity);
	}

	const void* GetRaw(Entity entity) const override {
		return Get(entity);
	}

	std::vector<Entity> GetEntityList() const override {
		std::vector<Entity> entities;
		entities.reserve(m_size);

		for(const auto& pageOwner : m_pages){
			if(!pageOwner) continue;
			const Page& page = *pageOwner;
			for(std::uint32_t slotIndex = 0; slotIndex < PageSize; ++slotIndex){
				if(!page.occupied.test(slotIndex)) continue;
				const Slot& slot = page.slots[slotIndex];
				if(slot.occupied && slot.value){
					entities.push_back(slot.entity);
				}
			}
		}
		return entities;
	}

	size_t Size() const noexcept override {
		return m_size;
	}

	std::uint32_t GetComponentGeneration(Entity entity) const override {
		const Page* page = FindPage(PageIndex(entity));
		if(!page) return 0;
		return page->slots[SlotIndex(entity)].componentGeneration;
	}

	std::uint64_t GetStructureVersion() const noexcept override {
		return m_structureVersion;
	}

	void ReservePageTable(std::uint32_t expectedEntityCount){
		const size_t requiredPages = expectedEntityCount == 0
			? 0
			: static_cast<size_t>((expectedEntityCount - 1) / PageSize) + 1;
		m_pages.reserve(requiredPages);
	}

	void PreallocatePages(std::uint32_t pageCount){
		if(m_pages.size() < pageCount){
			m_pages.resize(pageCount);
		}
		for(std::uint32_t index = 0; index < pageCount; ++index){
			if(!m_pages[index]) m_pages[index] = std::make_unique<Page>();
		}
	}

private:
	struct Slot {
		std::optional<T> value;
		Entity entity{};
		std::uint32_t entityGeneration = 0;
		std::uint32_t componentGeneration = 0;
		bool occupied = false;
	};

	struct Page {
		std::array<Slot, PageSize> slots{};
		std::bitset<PageSize> occupied{};
	};

	static constexpr std::uint32_t PageIndex(Entity entity) noexcept {
		return entity.GetIndex() / PageSize;
	}

	static constexpr std::uint32_t SlotIndex(Entity entity) noexcept {
		return entity.GetIndex() % PageSize;
	}

	Page& EnsurePage(std::uint32_t pageIndex){
		if(m_pages.size() <= pageIndex) m_pages.resize(static_cast<size_t>(pageIndex) + 1);
		if(!m_pages[pageIndex]) m_pages[pageIndex] = std::make_unique<Page>();
		return *m_pages[pageIndex];
	}

	Page* FindPage(std::uint32_t pageIndex){
		return pageIndex < m_pages.size() ? m_pages[pageIndex].get() : nullptr;
	}

	const Page* FindPage(std::uint32_t pageIndex) const {
		return pageIndex < m_pages.size() ? m_pages[pageIndex].get() : nullptr;
	}

	Slot* FindSlot(Entity entity){
		Page* page = FindPage(PageIndex(entity));
		if(!page) return nullptr;
		Slot& slot = page->slots[SlotIndex(entity)];
		return slot.occupied &&
			slot.entityGeneration == entity.GetGeneration() &&
			slot.entity == entity
			? &slot
			: nullptr;
	}

	const Slot* FindSlot(Entity entity) const {
		const Page* page = FindPage(PageIndex(entity));
		if(!page) return nullptr;
		const Slot& slot = page->slots[SlotIndex(entity)];
		return slot.occupied &&
			slot.entityGeneration == entity.GetGeneration() &&
			slot.entity == entity
			? &slot
			: nullptr;
	}

	static void IncrementComponentGeneration(Slot& slot){
		++slot.componentGeneration;
		if(slot.componentGeneration == 0) ++slot.componentGeneration;
	}

	std::vector<std::unique_ptr<Page>> m_pages;
	size_t m_size = 0;
	std::uint64_t m_structureVersion = 0;
};

// 値を持たず、存在だけで状態を表すTag Component用Storage。
// GetRaw互換のため型共有Dummyを返すが、Dummyへの書き込みは禁止する。
template<typename T, std::uint32_t PageSize = 256>
class DirectPagedTagStorage final: public IComponentStorage {
	static_assert(PageSize > 0, "DirectPaged PageSize must be greater than zero");
	static_assert(std::is_default_constructible_v<T>,
		"DirectPaged Tag type must be default constructible");

public:
	void Add(Entity entity){
		Page& page = EnsurePage(PageIndex(entity));
		const std::uint32_t slotIndex = SlotIndex(entity);

		if(page.occupied.test(slotIndex) &&
			page.entityGenerations[slotIndex] == entity.GetGeneration()){
			return;
		}

		if(page.occupied.test(slotIndex)){
			IncrementGeneration(page.componentGenerations[slotIndex]);
			--m_size;
		}

		page.occupied.set(slotIndex);
		page.entities[slotIndex] = entity;
		page.entityGenerations[slotIndex] = entity.GetGeneration();
		++m_size;
		++m_structureVersion;
	}

	void Remove(Entity entity) override {
		Page* page = FindPage(PageIndex(entity));
		if(!page) return;
		const std::uint32_t slotIndex = SlotIndex(entity);
		if(!Matches(*page, slotIndex, entity)) return;

		page->occupied.reset(slotIndex);
		IncrementGeneration(page->componentGenerations[slotIndex]);
		--m_size;
		++m_structureVersion;
	}

	bool Contains(Entity entity) const override {
		const Page* page = FindPage(PageIndex(entity));
		return page && Matches(*page, SlotIndex(entity), entity);
	}

	void* GetRaw(Entity entity) override {
		return Contains(entity) ? &m_dummy : nullptr;
	}

	const void* GetRaw(Entity entity) const override {
		return Contains(entity) ? &m_dummy : nullptr;
	}

	std::vector<Entity> GetEntityList() const override {
		std::vector<Entity> entities;
		entities.reserve(m_size);
		for(const auto& pageOwner : m_pages){
			if(!pageOwner) continue;
			const Page& page = *pageOwner;
			for(std::uint32_t slotIndex = 0; slotIndex < PageSize; ++slotIndex){
				if(page.occupied.test(slotIndex)) entities.push_back(page.entities[slotIndex]);
			}
		}
		return entities;
	}

	size_t Size() const noexcept override {
		return m_size;
	}

	std::uint32_t GetComponentGeneration(Entity entity) const override {
		const Page* page = FindPage(PageIndex(entity));
		return page ? page->componentGenerations[SlotIndex(entity)] : 0;
	}

	std::uint64_t GetStructureVersion() const noexcept override {
		return m_structureVersion;
	}

	void ReservePageTable(std::uint32_t expectedEntityCount){
		const size_t requiredPages = expectedEntityCount == 0
			? 0
			: static_cast<size_t>((expectedEntityCount - 1) / PageSize) + 1;
		m_pages.reserve(requiredPages);
	}

	void PreallocatePages(std::uint32_t pageCount){
		if(m_pages.size() < pageCount) m_pages.resize(pageCount);
		for(std::uint32_t index = 0; index < pageCount; ++index){
			if(!m_pages[index]) m_pages[index] = std::make_unique<Page>();
		}
	}

private:
	struct Page {
		std::bitset<PageSize> occupied{};
		std::array<Entity, PageSize> entities{};
		std::array<std::uint32_t, PageSize> entityGenerations{};
		std::array<std::uint32_t, PageSize> componentGenerations{};
	};

	static constexpr std::uint32_t PageIndex(Entity entity) noexcept {
		return entity.GetIndex() / PageSize;
	}

	static constexpr std::uint32_t SlotIndex(Entity entity) noexcept {
		return entity.GetIndex() % PageSize;
	}

	Page& EnsurePage(std::uint32_t pageIndex){
		if(m_pages.size() <= pageIndex) m_pages.resize(static_cast<size_t>(pageIndex) + 1);
		if(!m_pages[pageIndex]) m_pages[pageIndex] = std::make_unique<Page>();
		return *m_pages[pageIndex];
	}

	Page* FindPage(std::uint32_t pageIndex){
		return pageIndex < m_pages.size() ? m_pages[pageIndex].get() : nullptr;
	}

	const Page* FindPage(std::uint32_t pageIndex) const {
		return pageIndex < m_pages.size() ? m_pages[pageIndex].get() : nullptr;
	}

	static bool Matches(const Page& page, std::uint32_t slotIndex, Entity entity){
		return page.occupied.test(slotIndex) &&
			page.entityGenerations[slotIndex] == entity.GetGeneration() &&
			page.entities[slotIndex] == entity;
	}

	static void IncrementGeneration(std::uint32_t& generation){
		++generation;
		if(generation == 0) ++generation;
	}

	std::vector<std::unique_ptr<Page>> m_pages;
	T m_dummy{};
	size_t m_size = 0;
	std::uint64_t m_structureVersion = 0;
};

} // namespace ECSStorage
