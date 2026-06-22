// =======================================================================
// 
// audioSystem.h
// 
// =======================================================================
#pragma once
#include "Interface/ISystem.h"
#include "Scene/scene.h"
#include "Registry/componentRegistry.h"
#include "Component/audioComponent.h"
#include "Audio/audioContext.h"

// オーディオの再生・停止を管理するシステム
class AudioSystem: public ISystem {
public:

	const char* GetSystemName() const override{
		return "AudioSystem";
	}
	AudioSystem(SceneManagerContext* context)
		: m_context(context){}

	~AudioSystem(){
	}

	void Initialize() override{
		m_audioContext = m_context->audio;
	}

	void Finalize() override{
		for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
			auto context = scene->GetSceneContext();
			auto entities = context->component->FindEntitiesWithComponent<AudioComponent>();
			for (auto entity : entities) {
				auto* comp = context->component->GetComponent<AudioComponent>(entity);
				if (comp) {
					comp->Stop();
				}
			}
		}
	}

	void Start() override{

	}

	void RegisterTasks(SystemScheduleBuilder& builder) override{

		using AudioUpdateQuery = ECSQuery::ComponentQueryView<
			ECSQuery::Read<TransformComponent>,
			ECSQuery::Write<AudioComponent>
		>;

		builder.AddQueryTask<AudioUpdateQuery>(
			"AudioSystem.Playback.Commit",
			SystemTaskDomain::Frame,
			SystemPhase::Late,
			0,
			StructuralAccess::None,
			ThreadAffinity::AnyWorker,
			[this](const SystemTaskContext& context){
				Update(context.deltaTime);
			}
		);
	}

	void Update(float){
		for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
			auto context = scene->GetSceneContext();
			auto entities = context->component->FindEntitiesWithComponent<AudioComponent>();
			for (auto entity : entities) {
				auto* comp = context->component->GetComponent<AudioComponent>(entity);
				if (!comp) continue;
				if (!comp->isInitialized) {
					comp->isInitialized = true;
					// AudioDataロードはSystem側でやる
					if (!comp->m_AudioData && !comp->FilePath.empty()) {
						comp->m_AudioData = m_context->resource->Load<AudioData>(comp->FilePath);
					}

					if (comp->PlayOnStart && !comp->Playing) {
						comp->Play(m_audioContext);
					}
				}
				if (comp->Playing) {
					if (!comp->m_SourceVoice) {
						comp->Playing = false;
					}
				}
			}
		}
	}

private:
	SceneManagerContext* m_context = nullptr;
	AudioContext* m_audioContext = nullptr;
};
