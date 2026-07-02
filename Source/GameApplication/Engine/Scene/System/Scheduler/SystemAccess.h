// =======================================================================
//
// SystemAccess.h
//
// =======================================================================
#pragma once

#include <typeindex>
#include <unordered_set>
#include <utility>

// Taskが実行可能なThreadを表す。
enum class ThreadAffinity {
	AnyWorker,
	MainThread,
	RenderThread
};

// World全体に対する包括的なAccess。
// Legacy SystemやScriptは、詳細Accessを宣言できるまでExclusiveとして扱う。
enum class WorldAccessMode {
	None,
	Exclusive
};

// Entity / Component構造変更の扱い。
enum class StructuralAccess {
	None,
	EmitCommands,
	ExclusiveWorldWrite
};

// Component型・Resource型ごとのRead / Write宣言。
// Schedulerはこの情報からRAW / WAW / WAR依存を生成する。
struct SystemAccess {
	std::unordered_set<std::type_index> componentReads;
	std::unordered_set<std::type_index> componentWrites;
	std::unordered_set<std::type_index> resourceReads;
	std::unordered_set<std::type_index> resourceWrites;

	WorldAccessMode worldAccess = WorldAccessMode::None;
	StructuralAccess structuralAccess = StructuralAccess::None;

	template<typename T>
	SystemAccess& ReadComponent() {
		componentReads.emplace(typeid(T));
		return *this;
	}

	template<typename T>
	SystemAccess& WriteComponent() {
		componentWrites.emplace(typeid(T));
		return *this;
	}

	template<typename T>
	SystemAccess& ReadResource() {
		resourceReads.emplace(typeid(T));
		return *this;
	}

	template<typename T>
	SystemAccess& WriteResource() {
		resourceWrites.emplace(typeid(T));
		return *this;
	}

	SystemAccess& SetWorldAccess(WorldAccessMode mode) {
		worldAccess = mode;
		return *this;
	}

	SystemAccess& SetStructuralAccess(StructuralAccess mode) {
		structuralAccess = mode;
		return *this;
	}

	// 既存System用の安全側設定。
	// 詳細Accessが移行されるまでは同一Worldの他Taskと並列実行しない。
	static SystemAccess LegacyExclusive() {
		SystemAccess access;
		access.worldAccess = WorldAccessMode::Exclusive;
		return access;
	}

	bool ConflictsWith(const SystemAccess& other) const {
		if(worldAccess == WorldAccessMode::Exclusive ||
			other.worldAccess == WorldAccessMode::Exclusive) {
			return true;
		}

		if(structuralAccess == StructuralAccess::ExclusiveWorldWrite ||
			other.structuralAccess == StructuralAccess::ExclusiveWorldWrite) {
			return true;
		}

		return HasIntersection(componentWrites, other.componentReads) ||
			HasIntersection(componentWrites, other.componentWrites) ||
			HasIntersection(componentReads, other.componentWrites) ||
			HasIntersection(resourceWrites, other.resourceReads) ||
			HasIntersection(resourceWrites, other.resourceWrites) ||
			HasIntersection(resourceReads, other.resourceWrites);
	}

private:
	static bool HasIntersection(
		const std::unordered_set<std::type_index>& lhs,
		const std::unordered_set<std::type_index>& rhs
	) {
		const auto* smaller = &lhs;
		const auto* larger = &rhs;
		if(smaller->size() > larger->size()) {
			std::swap(smaller, larger);
		}

		for(const std::type_index& type : *smaller) {
			if(larger->contains(type)) {
				return true;
			}
		}
		return false;
	}
};
