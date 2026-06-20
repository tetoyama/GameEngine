// =======================================================================
//
// EntityCommandCommitSystem.h
//
// =======================================================================
#pragma once

#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "Scene/Interface/ISystem.h"
#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Scene/Command/EntityCommandBuffer.h"

// Scene IDごとのCommand Bufferを所有し、各Scheduleの開始時にSceneContextへ接続、
// 終了時にまとめて構造変更を適用する。
class EntityCommandCommitSystem final : public ISystem {
public:
	explicit EntityCommandCommitSystem(SceneManagerContext* context)
		: m_context(context) {
	}

	const char* GetSystemName() const override {
		return "EntityCommandCommitSystem";
	}

	void RegisterTasks(SystemScheduleBuilder& builder) override {
		RegisterDomainTasks(builder, SystemTaskDomain::Fixed, "Fixed");
		RegisterDomainTasks(builder, SystemTaskDomain::Frame, "Frame");
		RegisterDomainTasks(builder, SystemTaskDomain::Editor, "Editor");
		RegisterDomainTasks(builder, SystemTaskDomain::Render, "Render");
	}

	EntityCommandBuffer* GetBuffer(SceneContext* sceneContext) {
		if(!sceneContext || sceneContext->contextID == 0) return nullptr;

		auto& buffer = m_buffers[sceneContext->contextID];
		if(!buffer) {
			buffer = std::make_unique<EntityCommandBuffer>();
		}

		sceneContext->commands = buffer.get();
		sceneContext->scriptCommands = {
			sceneContext,
			&QueueCreateEntityFromScript,
			&QueueDestroyEntityFromScript,
			&QueueAddDefaultComponentFromScript,
			&QueueRemoveComponentFromScript
		};
		return buffer.get();
	}

	void PrepareBuffers() {
		AttachAllScenes();
	}

	void CommitNow() {
		CommitAllScenes();
	}

private:
	static SceneContext* ResolveScriptContext(void* owner) {
		return static_cast<SceneContext*>(owner);
	}

	static EntityCommandBuffer* ResolveScriptBuffer(void* owner) {
		SceneContext* context = ResolveScriptContext(owner);
		return context ? context->commands : nullptr;
	}

	static CommandEntity QueueCreateEntityFromScript(void* owner) {
		EntityCommandBuffer* buffer = ResolveScriptBuffer(owner);
		return buffer ? buffer->CreateEntity() : CommandEntity{};
	}

	static bool QueueDestroyEntityFromScript(
		void* owner,
		CommandEntity target
	) {
		EntityCommandBuffer* buffer = ResolveScriptBuffer(owner);
		if(!buffer || (!target.IsExisting() && !target.IsPending())) return false;

		buffer->DestroyEntity(target);
		return true;
	}

	static bool QueueAddDefaultComponentFromScript(
		void* owner,
		CommandEntity target,
		const char* runtimeTypeName
	) {
		EntityCommandBuffer* buffer = ResolveScriptBuffer(owner);
		if(!buffer || !runtimeTypeName || runtimeTypeName[0] == '\0') return false;
		if(!target.IsExisting() && !target.IsPending()) return false;

		// DLL所有文字列をCommitまで保持せず、Engine側で即座にコピーする。
		std::string copiedTypeName(runtimeTypeName);
		buffer->Execute(
			target,
			[copiedTypeName = std::move(copiedTypeName)](
				Entity entity,
				SceneContext& context
			) {
				if(!context.component) return;
				context.component->AddDefaultComponentByRuntimeTypeName(
					entity,
					copiedTypeName
				);
			}
		);
		return true;
	}

	static bool QueueRemoveComponentFromScript(
		void* owner,
		CommandEntity target,
		const char* runtimeTypeName
	) {
		EntityCommandBuffer* buffer = ResolveScriptBuffer(owner);
		if(!buffer || !runtimeTypeName || runtimeTypeName[0] == '\0') return false;
		if(!target.IsExisting() && !target.IsPending()) return false;

		std::string copiedTypeName(runtimeTypeName);
		buffer->Execute(
			target,
			[copiedTypeName = std::move(copiedTypeName)](
				Entity entity,
				SceneContext& context
			) {
				if(!context.component) return;
				context.component->RemoveComponentByRuntimeTypeName(
					entity,
					copiedTypeName
				);
			}
		);
		return true;
	}

	void RegisterDomainTasks(
		SystemScheduleBuilder& builder,
		SystemTaskDomain domain,
		const char* domainName
	) {
		SystemAccess attachAccess;
		attachAccess.SetWorldAccess(WorldAccessMode::Exclusive);

		builder.AddTask(
			std::string("EntityCommandCommitSystem.") + domainName + "Attach",
			domain,
			SystemPhase::Earliest,
			(std::numeric_limits<int32_t>::min)(),
			std::move(attachAccess),
			ThreadAffinity::MainThread,
			[this](const SystemTaskContext&) {
				AttachAllScenes();
			}
		);

		SystemAccess commitAccess;
		commitAccess.SetWorldAccess(WorldAccessMode::Exclusive)
			.SetStructuralAccess(StructuralAccess::ExclusiveWorldWrite);

		builder.AddTask(
			std::string("EntityCommandCommitSystem.") + domainName + "Commit",
			domain,
			SystemPhase::Latest,
			(std::numeric_limits<int32_t>::max)(),
			std::move(commitAccess),
			ThreadAffinity::MainThread,
			[this](const SystemTaskContext&) {
				CommitAllScenes();
			}
		);
	}

	void AttachAllScenes() {
		if(!m_context || !m_context->sceneManager) return;

		std::unordered_set<uint32_t> activeContextIDs;

		for(auto& [sceneName, scene] : m_context->sceneManager->GetActiveScenes()) {
			if(!scene) continue;

			SceneContext* sceneContext = scene->GetSceneContext();
			if(!sceneContext || sceneContext->contextID == 0) continue;

			activeContextIDs.insert(sceneContext->contextID);
			GetBuffer(sceneContext);
		}

		// SceneContextが別のshared_ptrから参照されている可能性があるため、
		// 非ActiveになってもBuffer本体は保持し、未適用Commandだけを破棄する。
		for(auto& [contextID, buffer] : m_buffers) {
			if(buffer && !activeContextIDs.contains(contextID)) {
				buffer->Clear();
			}
		}
	}

	void CommitAllScenes() {
		AttachAllScenes();
		if(!m_context || !m_context->sceneManager) return;

		for(auto& [sceneName, scene] : m_context->sceneManager->GetActiveScenes()) {
			if(!scene) continue;

			SceneContext* sceneContext = scene->GetSceneContext();
			EntityCommandBuffer* buffer = GetBuffer(sceneContext);
			if(!sceneContext || !buffer) continue;

			buffer->Commit(*sceneContext);
		}
	}

	SceneManagerContext* m_context = nullptr;
	std::unordered_map<uint32_t, std::unique_ptr<EntityCommandBuffer>> m_buffers;
};
