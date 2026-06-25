// =======================================================================
//
// ComponentQueryView.h
//
// ComponentMaskで生存Entityを遅延フィルタする無確保Query View。
// Query中にEntity / Component構造を変更してはならない。
//
// =======================================================================
#pragma once

#include "Entity/Entity.h"
#include "Interface/IComponentStorage.h"
#include "System/Scheduler/SystemAccess.h"

#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

namespace ECSQuery {

template<typename T>
struct Read {
	using Component = T;

	static void ApplyAccess(SystemAccess& access){
		access.ReadComponent<T>();
	}
};

template<typename T>
struct Write {
	using Component = T;

	static void ApplyAccess(SystemAccess& access){
		access.WriteComponent<T>();
	}
};

template<typename Access>
using ComponentType = typename Access::Component;

template<typename... Accesses>
class ComponentQueryView {
public:
	using AliveSet = std::unordered_set<Entity>;
	using MaskMap = std::unordered_map<Entity, ComponentMask>;

	class Iterator {
	public:
		using UnderlyingIterator = AliveSet::const_iterator;

		Iterator(
			UnderlyingIterator current,
			UnderlyingIterator end,
			const MaskMap* masks,
			const ComponentMask* required,
			const ComponentMask* excluded
		)
			: m_current(current),
			  m_end(end),
			  m_masks(masks),
			  m_required(required),
			  m_excluded(excluded){
			AdvanceToMatch();
		}

		Entity operator*() const {
			return *m_current;
		}

		Iterator& operator++(){
			if(m_current != m_end){
				++m_current;
				AdvanceToMatch();
			}
			return *this;
		}

		bool operator==(const Iterator& other) const {
			return m_current == other.m_current;
		}

		bool operator!=(const Iterator& other) const {
			return !(*this == other);
		}

	private:
		void AdvanceToMatch(){
			while(m_current != m_end){
				auto maskIterator = m_masks->find(*m_current);
				if(maskIterator != m_masks->end()){
					const ComponentMask& mask = maskIterator->second;
					const bool hasRequired =
						(mask & *m_required) == *m_required;
					const bool hasExcluded =
						(mask & *m_excluded).any();
					if(hasRequired && !hasExcluded){
						break;
					}
				}
				++m_current;
			}
		}

		UnderlyingIterator m_current;
		UnderlyingIterator m_end;
		const MaskMap* m_masks = nullptr;
		const ComponentMask* m_required = nullptr;
		const ComponentMask* m_excluded = nullptr;
	};

	ComponentQueryView(
		const AliveSet& alive,
		const MaskMap& masks,
		ComponentMask required,
		ComponentMask excluded = {}
	)
		: m_alive(&alive),
		  m_masks(&masks),
		  m_required(required),
		  m_excluded(excluded){}

	Iterator begin() const {
		return Iterator(
			m_alive->begin(),
			m_alive->end(),
			m_masks,
			&m_required,
			&m_excluded
		);
	}

	Iterator end() const {
		return Iterator(
			m_alive->end(),
			m_alive->end(),
			m_masks,
			&m_required,
			&m_excluded
		);
	}

	static SystemAccess BuildSystemAccess(){
		SystemAccess access;
		(Accesses::ApplyAccess(access), ...);
		return access;
	}

	const ComponentMask& GetRequiredMask() const noexcept {
		return m_required;
	}

	const ComponentMask& GetExcludedMask() const noexcept {
		return m_excluded;
	}

private:
	const AliveSet* m_alive = nullptr;
	const MaskMap* m_masks = nullptr;
	ComponentMask m_required;
	ComponentMask m_excluded;
};

} // namespace ECSQuery
