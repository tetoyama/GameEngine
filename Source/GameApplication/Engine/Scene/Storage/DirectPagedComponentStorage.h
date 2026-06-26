// =======================================================================
//
// DirectPagedComponentStorage.h
//
// =======================================================================
#pragma once

#include "Scene/Interface/IComponentStorage.h"

#include <algorithm>
#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace ECSStorage {

inline constexpr std::uint32_t DirectPagedDefaultPageSize = 256;

template<typename T, std::uint32_t PageSizeV = DirectPagedDefaultPageSize>
class DirectPagedComponentStorage final: public IComponentStorage {
public:
	static_assert(PageSizeV > 0);
	static constexpr std::uint32_t PageSize = PageSizeV;

	void Add(Entity entity, T component){
		const std::uint32_t pageIndex = PageIndex(entity);
		const std::uint32_t slotIndex = SlotIndex(entity);
		const bool requiresPage =
			pageIndex >= m_pages.size() || !m_pages[pageIndex];
		Page& page = EnsurePage(pageIndex);
		if(Matches(page, slotIndex, entity)) return;

		if(page.occupied.test(slotIndex)){
			page.components[slotIndex].reset();
			IncrementGeneration(page.componentGenerations[slotIndex]);
		}else{
			++m_size;
		}

		page.components[slotIndex].emplace(std::move(component));
		page.entities[slotIndex] = entity;
		page.entityGenerations[slotIndex] = entity.GetGeneration();
		page.occupied.set(slotIndex);
		if(requiresPage) ++m_growthEventCount;
		m_peakSize = (std::max)(m_peakSize, m_size);
		++m_structureVersion;
	}

	T* Get(Entity entity){
		Page* page = FindPage(PageIndex(entity));
		const std::uint32_t index = SlotIndex(entity);
		return page && Matches(*page, index, entity)
			? &page->components[index].value()
			: nullptr;
	}

	const T* Get(Entity entity) const {
		const Page* page = FindPage(PageIndex(entity));
		const std::uint32_t index = SlotIndex(entity);
		return page && Matches(*page, index, entity)
			? &page->components[index].value()
			: nullptr;
	}

	bool Contains(Entity entity) const override {
		return Get(entity) != nullptr;
	}

	void Remove(Entity entity) override {
		Page* page = FindPage(PageIndex(entity));
		const std::uint32_t index = SlotIndex(entity);
		if(!page || !Matches(*page, index, entity)) return;

		page->components[index].reset();
		page->occupied.reset(index);
		IncrementGeneration(page->componentGenerations[index]);
		--m_size;
		++m_structureVersion;
	}

	void* GetRaw(Entity entity) override { return Get(entity); }
	const void* GetRaw(Entity entity) const override { return Get(entity); }

	std::vector<Entity> GetEntityList() const override {
		std::vector<Entity> entities;
		entities.reserve(m_size);
		ForEachOccupied(
			[&entities](Entity entity, const T& component){
				(void)component;
				entities.push_back(entity);
			}
		);
		return entities;
	}

	size_t Size() const noexcept override { return m_size; }

	void Reserve(size_t expectedCount) override {
		ReservePageTable(expectedCount);
	}

	void PreallocatePages(size_t pageCount) override {
		const size_t clamped = (std::min)(
			pageCount,
			static_cast<size_t>((std::numeric_limits<std::uint32_t>::max)())
		);
		for(size_t pageIndex = 0; pageIndex < clamped; ++pageIndex){
			EnsurePage(static_cast<std::uint32_t>(pageIndex));
		}
	}

	size_t Capacity() const noexcept override {
		return AllocatedPageCount() * static_cast<size_t>(PageSize);
	}

	size_t PeakSize() const noexcept override { return m_peakSize; }
	size_t GrowthEventCount() const noexcept override { return m_growthEventCount; }
	void ResetPeakMetrics() noexcept override {
		m_peakSize = m_size;
		m_growthEventCount = 0;
	}

	uint32_t GetComponentGeneration(Entity entity) const override {
		const Page* page = FindPage(PageIndex(entity));
		return page ? page->componentGenerations[SlotIndex(entity)] : 0;
	}

	std::uint64_t GetStructureVersion() const noexcept override {
		return m_structureVersion;
	}

	void ReservePageTable(size_t expectedEntityCount){
		m_pages.reserve(RequiredPageCount(expectedEntityCount));
	}

	size_t GetAllocatedPageCount() const noexcept {
		return AllocatedPageCount();
	}

	template<typename Fn>
	void ForEachOccupied(Fn&& function){
		for(auto& pageOwner : m_pages){
			if(!pageOwner) continue;
			Page& page = *pageOwner;
			for(std::uint32_t index = 0; index < PageSize; ++index){
				if(page.occupied.test(index)){
					function(page.entities[index], page.components[index].value());
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
				if(page.occupied.test(index)){
					function(page.entities[index], page.components[index].value());
				}
			}
		}
	}

private:
	struct Page {
		std::bitset<PageSize> occupied{};
		std::array<Entity, PageSize> entities{};
		std::array<std::uint32_t, PageSize> entityGenerations{};
		std::array<std::uint32_t, PageSize> componentGenerations{};
		std::array<std::optional<T>, PageSize> components{};
	};

	static constexpr std::uint32_t PageIndex(Entity entity) noexcept {
		return entity.GetIndex() / PageSize;
	}
	static constexpr std::uint32_t SlotIndex(Entity entity) noexcept {
		return entity.GetIndex() % PageSize;
	}
	static constexpr size_t RequiredPageCount(size_t entityCount) noexcept {
		return entityCount == 0 ? 0 : ((entityCount - 1) / PageSize) + 1;
	}

	Page& EnsurePage(std::uint32_t pageIndex){
		if(m_pages.size() <= pageIndex){
			m_pages.resize(static_cast<size_t>(pageIndex) + 1);
		}
		if(!m_pages[pageIndex]){
			m_pages[pageIndex] = std::make_unique<Page>();
		}
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

	size_t AllocatedPageCount() const noexcept {
		size_t count = 0;
		for(const auto& page : m_pages){
			if(page) ++count;
		}
		return count;
	}

	std::vector<std::unique_ptr<Page>> m_pages;
	size_t m_size = 0;
	size_t m_peakSize = 0;
	size_t m_growthEventCount = 0;
	std::uint64_t m_structureVersion = 0;
};

template<typename T, std::uint32_t PageSizeV = DirectPagedDefaultPageSize>
class DirectPagedTagStorage final: public IComponentStorage {
public:
	static_assert(PageSizeV > 0);
	static constexpr std::uint32_t PageSize = PageSizeV;

	void Add(Entity entity, T component = {}){
		(void)component;
		const std::uint32_t pageIndex = PageIndex(entity);
		const std::uint32_t slotIndex = SlotIndex(entity);
		const bool requiresPage =
			pageIndex >= m_pages.size() || !m_pages[pageIndex];
		Page& page = EnsurePage(pageIndex);
		if(Matches(page, slotIndex, entity)) return;

		if(page.occupied.test(slotIndex)){
			IncrementGeneration(page.componentGenerations[slotIndex]);
		}else{
			++m_size;
		}
		page.entities[slotIndex] = entity;
		page.entityGenerations[slotIndex] = entity.GetGeneration();
		page.occupied.set(slotIndex);
		if(requiresPage) ++m_growthEventCount;
		m_peakSize = (std::max)(m_peakSize, m_size);
		++m_structureVersion;
	}

	T* Get(Entity entity){
		return Contains(entity) ? &m_dummy : nullptr;
	}
	const T* Get(Entity entity) const {
		return Contains(entity) ? &m_dummy : nullptr;
	}
	bool Contains(Entity entity) const override {
		const Page* page = FindPage(PageIndex(entity));
		return page && Matches(*page, SlotIndex(entity), entity);
	}

	void Remove(Entity entity) override {
		Page* page = FindPage(PageIndex(entity));
		const std::uint32_t index = SlotIndex(entity);
		if(!page || !Matches(*page, index, entity)) return;
		page->occupied.reset(index);
		IncrementGeneration(page->componentGenerations[index]);
		--m_size;
		++m_structureVersion;
	}

	void* GetRaw(Entity entity) override { return Get(entity); }
	const void* GetRaw(Entity entity) const override { return Get(entity); }

	std::vector<Entity> GetEntityList() const override {
		std::vector<Entity> entities;
		entities.reserve(m_size);
		ForEachEntity([&entities](Entity entity){ entities.push_back(entity); });
		return entities;
	}

	size_t Size() const noexcept override { return m_size; }
	void Reserve(size_t expectedCount) override { ReservePageTable(expectedCount); }
	void PreallocatePages(size_t pageCount) override {
		const size_t clamped = (std::min)(
			pageCount,
			static_cast<size_t>((std::numeric_limits<std::uint32_t>::max)())
		);
		for(size_t pageIndex = 0; pageIndex < clamped; ++pageIndex){
			EnsurePage(static_cast<std::uint32_t>(pageIndex));
		}
	}
	size_t Capacity() const noexcept override {
		return AllocatedPageCount() * static_cast<size_t>(PageSize);
	}
	size_t PeakSize() const noexcept override { return m_peakSize; }
	size_t GrowthEventCount() const noexcept override { return m_growthEventCount; }
	void ResetPeakMetrics() noexcept override {
		m_peakSize = m_size;
		m_growthEventCount = 0;
	}

	uint32_t GetComponentGeneration(Entity entity) const override {
		const Page* page = FindPage(PageIndex(entity));
		return page ? page->componentGenerations[SlotIndex(entity)] : 0;
	}
	std::uint64_t GetStructureVersion() const noexcept override {
		return m_structureVersion;
	}

	void ReservePageTable(size_t expectedEntityCount){
		m_pages.reserve(RequiredPageCount(expectedEntityCount));
	}
	size_t GetAllocatedPageCount() const noexcept { return AllocatedPageCount(); }

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
	static constexpr size_t RequiredPageCount(size_t entityCount) noexcept {
		return entityCount == 0 ? 0 : ((entityCount - 1) / PageSize) + 1;
	}

	Page& EnsurePage(std::uint32_t pageIndex){
		if(m_pages.size() <= pageIndex){
			m_pages.resize(static_cast<size_t>(pageIndex) + 1);
		}
		if(!m_pages[pageIndex]){
			m_pages[pageIndex] = std::make_unique<Page>();
		}
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
	size_t AllocatedPageCount() const noexcept {
		size_t count = 0;
		for(const auto& page : m_pages){
			if(page) ++count;
		}
		return count;
	}

	std::vector<std::unique_ptr<Page>> m_pages;
	T m_dummy{};
	size_t m_size = 0;
	size_t m_peakSize = 0;
	size_t m_growthEventCount = 0;
	std::uint64_t m_structureVersion = 0;
};

} // namespace ECSStorage
