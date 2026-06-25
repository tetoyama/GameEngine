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
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "Scene/Interface/IComponentStorage.h"

namespace ECSStorage {

template<typename T, std::uint32_t PageSize = 256>
class DirectPagedComponentStorage final: public IComponentStorage {
	static_assert(PageSize > 0, "DirectPaged PageSize must be greater than zero");

public:
	static constexpr std::uint32_t kPageSize = PageSize;

	void Add(Entity entity, T component){
		Page& page = EnsurePage(PageIndex(entity));
		Slot& slot = page.slots[SlotIndex(entity)];
		if(Matches(slot, entity)) return;

		if(slot.occupied){
			slot.value.reset();
			IncrementGeneration(slot.componentGeneration);
			--m_size;
		}

		slot.value.emplace(std::move(component));
		slot.entity = entity;
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
		Page* page = FindPage(PageIndex(entity));
		if(!page) return;
		Slot& slot = page->slots[SlotIndex(entity)];
		if(!Matches(slot, entity)) return;

		slot.value.reset();
		slot.occupied = false;
		page->occupied.reset(SlotIndex(entity));
		IncrementGeneration(slot.componentGeneration);
		--m_size;
		++m_structureVersion;
	}

	bool Contains(Entity entity) const override { return FindSlot(entity) != nullptr; }
	void* GetRaw(Entity entity) override { return Get(entity); }
	const void* GetRaw(Entity entity) const override { return Get(entity); }

	std::vector<Entity> GetEntityList() const override {
		std::vector<Entity> entities;
		entities.reserve(m_size);
		ForEachOccupied([&entities](Entity entity, const T&){
			entities.push_back(entity);
		});
		return entities;
	}

	size_t Size() const noexcept override { return m_size; }

	std::uint32_t GetComponentGeneration(Entity entity) const override {
		const Page* page = FindPage(PageIndex(entity));
		return page ? page->slots[SlotIndex(entity)].componentGeneration : 0;
	}

	std::uint64_t GetStructureVersion() const noexcept override { return m_structureVersion; }

	void ReservePageTable(std::uint32_t expectedEntityCount){
		m_pages.reserve(RequiredPageCount(expectedEntityCount));
	}

	void PreallocatePages(std::uint32_t pageCount){
		if(m_pages.size() < pageCount) m_pages.resize(pageCount);
		for(std::uint32_t index = 0; index < pageCount; ++index){
			if(!m_pages[index]) m_pages[index] = std::make_unique<Page>();
		}
	}

	// Page tableを順番に辿り、割当済みSlotだけを列挙する。
	// TransformSystemなどの全件更新はAlive Entity全走査ではなく、このAPIを使用する。
	template<typename Fn>
	void ForEachOccupied(Fn&& function){
		for(auto& pageOwner : m_pages){
			if(!pageOwner) continue;
			Page& page = *pageOwner;
			for(std::uint32_t index = 0; index < PageSize; ++index){
				if(!page.occupied.test(index)) continue;
				Slot& slot = page.slots[index];
				if(slot.occupied && slot.value){
					function(slot.entity, slot.value.value());
				}
			}
		}
	}

	template<typename Fn>
	void ForEachOccupied(Fn&& function) const {
		for(const auto& pageOwner : m_pages){
			if(!pageOwner) continue;
			const Page& page = *pageOwner;
			for(std::uint32_t index = 0; index < PageSize; ++index){
				if(!page.occupied.test(index)) continue;
				const Slot& slot = page.slots[index];
				if(slot.occupied && slot.value){
					function(slot.entity, slot.value.value());
				}
			}
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

	static constexpr size_t RequiredPageCount(std::uint32_t entityCount) noexcept {
		return entityCount == 0 ? 0 : static_cast<size_t>((entityCount - 1) / PageSize) + 1;
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
		return Matches(slot, entity) ? &slot : nullptr;
	}

	const Slot* FindSlot(Entity entity) const {
		const Page* page = FindPage(PageIndex(entity));
		if(!page) return nullptr;
		const Slot& slot = page->slots[SlotIndex(entity)];
		return Matches(slot, entity) ? &slot : nullptr;
	}

	static bool Matches(const Slot& slot, Entity entity){
		return slot.occupied &&
			slot.entityGeneration == entity.GetGeneration() &&
			slot.entity == entity;
	}

	static void IncrementGeneration(std::uint32_t& generation){
		++generation;
		if(generation == 0) ++generation;
	}

	std::vector<std::unique_ptr<Page>> m_pages;
	size_t m_size = 0;
	std::uint64_t m_structureVersion = 0;
};

template<typename T, std::uint32_t PageSize = 256>
class DirectPagedTagStorage final: public IComponentStorage {
	static_assert(PageSize > 0, "DirectPaged PageSize must be greater than zero");
	static_assert(std::is_default_constructible_v<T>,
		"DirectPaged Tag type must be default constructible");

public:
	void Add(Entity entity){
		Page& page = EnsurePage(PageIndex(entity));
		const std::uint32_t index = SlotIndex(entity);
		if(Matches(page, index, entity)) return;

		if(page.occupied.test(index)){
			IncrementGeneration(page.componentGenerations[index]);
			--m_size;
		}

		page.occupied.set(index);
		page.entities[index] = entity;
		page.entityGenerations[index] = entity.GetGeneration();
		++m_size;
		++m_structureVersion;
	}

	void Remove(Entity entity) override {
		Page* page = FindPage(PageIndex(entity));
		if(!page) return;
		const std::uint32_t index = SlotIndex(entity);
		if(!Matches(*page, index, entity)) return;
		page->occupied.reset(index);
		IncrementGeneration(page->componentGenerations[index]);
		--m_size;
		++m_structureVersion;
	}

	bool Contains(Entity entity) const override {
		const Page* page = FindPage(PageIndex(entity));
		return page && Matches(*page, SlotIndex(entity), entity);
	}

	void* GetRaw(Entity entity) override { return Contains(entity) ? &m_dummy : nullptr; }
	const void* GetRaw(Entity entity) const override { return Contains(entity) ? &m_dummy : nullptr; }

	std::vector<Entity> GetEntityList() const override {
		std::vector<Entity> entities;
		entities.reserve(m_size);
		ForEachEntity([&entities](Entity entity){ entities.push_back(entity); });
		return entities;
	}

	size_t Size() const noexcept override { return m_size; }

	std::uint32_t GetComponentGeneration(Entity entity) const override {
		const Page* page = FindPage(PageIndex(entity));
		return page ? page->componentGenerations[SlotIndex(entity)] : 0;
	}

	std::uint64_t GetStructureVersion() const noexcept override { return m_structureVersion; }

	void ReservePageTable(std::uint32_t expectedEntityCount){
		m_pages.reserve(RequiredPageCount(expectedEntityCount));
	}

	void PreallocatePages(std::uint32_t pageCount){
		if(m_pages.size() < pageCount) m_pages.resize(pageCount);
		for(std::uint32_t index = 0; index < pageCount; ++index){
			if(!m_pages[index]) m_pages[index] = std::make_unique<Page>();
		}
	}

	template<typename Fn>
	void ForEachEntity(Fn&& function) const {
		for(const auto& pageOwner : m_pages){
			if(!pageOwner) continue;
			const Page& page = *pageOwner;
			for(std::uint32_t index = 0; index < PageSize; ++index){
				if(page.occupied.test(index)) function(page.entities[index]);
			}
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

	static constexpr size_t RequiredPageCount(std::uint32_t entityCount) noexcept {
		return entityCount == 0 ? 0 : static_cast<size_t>((entityCount - 1) / PageSize) + 1;
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

	static bool Matches(const Page& page, std::uint32_t index, Entity entity){
		return page.occupied.test(index) &&
			page.entityGenerations[index] == entity.GetGeneration() &&
			page.entities[index] == entity;
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
