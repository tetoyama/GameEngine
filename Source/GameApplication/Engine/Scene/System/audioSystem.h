// AudioSystem.h
#pragma once
#include "Interface/ISystem.h"
#include "Engine/Scene/scene.h"
#include "Component/audioComponent.h"
#include "Engine/Audio/audioContext.h"

class AudioSystem: public ISystem {
public:
	AudioSystem(SceneContext* context)
		: m_context(context){}

	~AudioSystem(){
	}

	void Initialize() override{
		m_audioContext = m_context->manager->audio;
	}

	void Finalize() override{
		auto entities = m_context->component->FindEntitiesWithComponent<AudioComponent>();
		for(auto entity : entities){
			auto* comp = m_context->component->GetComponent<AudioComponent>(entity);
			if(comp){
				comp->Stop();
			}
		}
	}

	void Start() override{
		auto entities = m_context->component->FindEntitiesWithComponent<AudioComponent>();
		for(auto entity : entities){
			auto* comp = m_context->component->GetComponent<AudioComponent>(entity);
			if(!comp) continue;

			// AudioDataロードはSystem側でやる
			if(!comp->m_AudioData && !comp->FilePath.empty()){
				comp->m_AudioData = m_context->manager->resource->Load<AudioData>(comp->FilePath);
			}

			if(comp->PlayOnStart && !comp->Playing){
				comp->Play(m_audioContext);
			}
		}
	}

	void Update(float) override{
		auto entities = m_context->component->FindEntitiesWithComponent<AudioComponent>();
		for(auto entity : entities){
			auto* comp = m_context->component->GetComponent<AudioComponent>(entity);
			if(!comp) continue;

			if(comp->Playing){
				if(!comp->m_SourceVoice){
					comp->Playing = false;
				}
			}
		}
	}

	void FixedUpdate(float) override{}
	void Draw() override{}
	void EditorUpdate(float) override{}

private:
	SceneContext* m_context = nullptr;
	AudioContext* m_audioContext = nullptr;
};
