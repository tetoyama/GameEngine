#pragma once

#include "Engine/Scene/Component/CustomScriptComponent.h"
#include "Engine/Scene/Component/audioComponent.h"
#include "Game/Platformer/PlatformerFeedback.h"
#include "Game/Platformer/PlatformerGameManager.h"
#include "Game/Platformer/PlatformerSceneAccess.h"
#include "Game/Platformer/PlatformerSoundLibrary.h"

class PlatformerClearFeedback : public CustomScriptComponent {
	BEGIN_REFLECT(PlatformerClearFeedback)
		REFLECT_FIELD(float, clearVolume, 0.32f)

public:
	YAML::Node encode() override {
		YAML::Node node;
		ENCODE_FIELDS(node);
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override {
		DECODE_FIELDS(node);
		return true;
	}

	void inspector(SceneContext* context) override {
		ImGui::Text("Platformer Clear Feedback");
		INSPECTOR_FIELDS();
		ImGui::Text("Played: %s", played ? "true" : "false");
	}

	void OnStart() override {
		manager = PlatformerSceneAccess::FindFirst<PlatformerGameManager>(m_ref.GetScene());
		audio = GetComponentRef<AudioComponent>();
		played = false;
	}

	void OnUpdate(float dt) override {
		if(played) return;
		if(!manager.IsValid()) manager = PlatformerSceneAccess::FindFirst<PlatformerGameManager>(m_ref.GetScene());
		auto* game = manager.TryGet();
		auto* audioComponent = audio.TryGet();
		if(!game || !audioComponent || !game->IsCleared()) return;

		audioComponent->Volume = clearVolume;
		played = PlatformerFeedback::Play(
			audioComponent,
			m_ref.GetScene(),
			PlatformerSoundLibrary::ClearPath);
	}

private:
	ComponentRef<PlatformerGameManager> manager;
	ComponentRef<AudioComponent> audio;
	bool played = false;
};
