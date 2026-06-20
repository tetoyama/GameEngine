// =======================================================================
//
// EntityCommandBuffer.h
//
// =======================================================================
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Scene/scene.h"
#include "Scene/Registry/entityRegistry.h"
#include "Scene/Registry/componentRegistry.h"

// Commit済みEntityと、同じCommand Buffer内で生成予定のEntityを共通に表す。
class CommandEntity {
public:
	CommandEntity() = default;

	static CommandEntity Existing(Entity entity) {
		CommandEntity result;
		result.m_entity = entity;
		return result;
	}

	static CommandEntity Pending(uint32_t pendingID) {
		CommandEntity result;
		result.m_pendingID = pendingID;
		return result;
	}

	bool IsPending() const noexcept { return m_pendingID != 0; }
	bool IsExisting() const noexcept { return !IsPending() && static_cast<bool>(m_entity); }
	Entity GetExisting() const noexcept { return m_entity; }
	uint32_t GetPendingID() const noexcept { return m_pendingID; }

private:
	Entity m_entity{};
	uint32_t m_pendingID = 0;
};

// Scene単位の構造変更キュー。
// Task実行中はコマンドだけを記録し、Latest PhaseのCommit Taskでまとめて適用する。
class EntityCommandBuffer {
public:
	using ResolveMap = std::unordered_map<uint32_t, Entity>;
	using Command = std::function<void(SceneContext&, ResolveMap&)>;

	EntityCommandBuffer() = default;
	~EntityCommandBuffer() = default;

	EntityCommandBuffer(const EntityCommandBuffer&) = delete;
	EntityCommandBuffer& operator=(const EntityCommandBuffer&) = delete;

	CommandEntity CreateEntity() {
		std::scoped_lock lock(m_mutex);
		const uint32_t pendingID = m_nextPendingID++;

		m_commands.emplace_back(
			[pendingID](SceneContext& context, ResolveMap& resolved) {
				if(!context.entity) return;
				resolved[pendingID] = context.entity->Create();
			}
		);

		return CommandEntity::Pending(pendingID);
	}

	void DestroyEntity(Entity entity) {
		DestroyEntity(CommandEntity::Existing(entity));
	}

	void DestroyEntity(CommandEntity target) {
		Enqueue(
			[target](SceneContext& context, ResolveMap& resolved) {
				const Entity entity = Resolve(target, resolved);
				if(!entity || !context.entity || !context.component) return;
				if(!context.entity->IsAlive(entity)) return;

				context.component->OnEntityDestroyed(entity);
				context.entity->Destroy(entity);
			}
		);
	}

	template<typename T, typename... Args>
	void AddComponent(Entity entity, Args&&... args) {
		AddComponent<T>(
			CommandEntity::Existing(entity),
			std::forward<Args>(args)...
		);
	}

	template<typename T, typename... Args>
	void AddComponent(CommandEntity target, Args&&... args) {
		using ArgumentTuple = std::tuple<std::decay_t<Args>...>;
		auto arguments = std::make_shared<ArgumentTuple>(
			std::forward<Args>(args)...
		);

		Enqueue(
			[target, arguments](SceneContext& context, ResolveMap& resolved) mutable {
				const Entity entity = Resolve(target, resolved);
				if(!entity || !context.entity || !context.component) return;
				if(!context.entity->IsAlive(entity)) return;

				std::apply(
					[&](auto&... values) {
						context.component->AddComponent<T>(
							entity,
							std::move(values)...
						);
					},
					*arguments
				);
			}
		);
	}

	template<typename T>
	void RemoveComponent(Entity entity) {
		RemoveComponent<T>(CommandEntity::Existing(entity));
	}

	template<typename T>
	void RemoveComponent(CommandEntity target) {
		Enqueue(
			[target](SceneContext& context, ResolveMap& resolved) {
				const Entity entity = Resolve(target, resolved);
				if(!entity || !context.entity || !context.component) return;
				if(!context.entity->IsAlive(entity)) return;

				context.component->RemoveComponent<T>(entity);
			}
		);
	}

	// 生成予定Entityを含む対象へ、Commit時の初期化処理を登録する。
	void Execute(
		CommandEntity target,
		std::function<void(Entity, SceneContext&)> callback
	) {
		Enqueue(
			[target, callback = std::move(callback)](
				SceneContext& context,
				ResolveMap& resolved
			) {
				if(!callback) return;
				const Entity entity = Resolve(target, resolved);
				if(!entity || !context.entity || !context.entity->IsAlive(entity)) return;
				callback(entity, context);
			}
		);
	}

	// 現在キューにあるコマンドだけを取り出して適用する。
	// Commit中に追加されたコマンドは次回Commitへ残す。
	void Commit(SceneContext& context) {
		std::vector<Command> commands;
		{
			std::scoped_lock lock(m_mutex);
			commands.swap(m_commands);
		}

		ResolveMap resolved;
		for(Command& command : commands) {
			if(command) {
				command(context, resolved);
			}
		}
	}

	void Clear() {
		std::scoped_lock lock(m_mutex);
		m_commands.clear();
	}

	size_t PendingCommandCount() const {
		std::scoped_lock lock(m_mutex);
		return m_commands.size();
	}

	bool Empty() const {
		return PendingCommandCount() == 0;
	}

private:
	void Enqueue(Command command) {
		std::scoped_lock lock(m_mutex);
		m_commands.emplace_back(std::move(command));
	}

	static Entity Resolve(
		CommandEntity target,
		const ResolveMap& resolved
	) {
		if(target.IsExisting()) {
			return target.GetExisting();
		}

		if(!target.IsPending()) {
			return {};
		}

		auto it = resolved.find(target.GetPendingID());
		return it != resolved.end() ? it->second : Entity{};
	}

	mutable std::mutex m_mutex;
	std::vector<Command> m_commands;
	uint32_t m_nextPendingID = 1;
};
