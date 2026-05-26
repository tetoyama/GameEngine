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
		audioContext= m_context->audio;
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

	void Update(float) override{
		for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
			auto context = scene->GetSceneContext();
			auto entities = context->component->FindEntitiesWithComponent<AudioComponent>();
			for (auto entity : entities) {
				auto* comp = context->component->GetComponent<AudioComponent>(entity);
				if (!comp) continue;
				if (!comp->isInitialized) {
					comp->isInitialized = true;
					// AudioDataロードはSystem側でやる
					if (!comp->audioData && !comp->FilePath.empty()) {
						comp->audioData = m_context->resource->Load<AudioData>(comp->FilePath);
					}

					if (comp->PlayOnStart && !comp->Playing) {
						comp->Play(audioContext);
					}
				}
				if (comp->Playing) {
					if (!comp->sourceVoice) {
						comp->Playing = false;
					}
				}
			}
		}
	}

private:
	SceneManagerContext* m_context = nullptr;
	AudioContext* audioContext= nullptr;
};
