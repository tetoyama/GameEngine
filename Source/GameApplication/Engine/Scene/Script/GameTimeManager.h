#pragma once
#include "Component/CustomScriptComponent.h"
#include "Backends/checkFileExtention.h"
#include "fadeOutSprite.h"
#include "scoreManager.h"

class GameTimeManager: public CustomScriptComponent {
public:
	BEGIN_REFLECT(GameTimeManager)
		REFLECT_FIELD_INIT(float, Timer, 0.0f, REFLECT_INSPECTOR)
		REFLECT_FIELD_INIT(float, CountDownTimer, 0.0f, REFLECT_INSPECTOR)
		REFLECT_FIELD_INIT(float, ShutDownTimer, 0.0f, REFLECT_INSPECTOR)
		REFLECT_FIELD(int, CountDownTime, 5)
		REFLECT_FIELD(int, InGameTime, 60)

	bool init = false;
	FadeOutSprite* fade = nullptr;
	ScoreManager* score = nullptr;
	GameTimeManager(): CustomScriptComponent("GameTimeManager"){}

	YAML::Node encode() override{
		YAML::Node node;
		ENCODE_FIELDS(node);
		return node;
	}
	bool decode(const YAML::Node& node) override{
		DECODE_FIELDS(node);
		return true;
	}

	void inspector(SceneContext* context) override{
		INSPECTOR_FIELDS();
	}

	void OnStart() override{
		Timer = (float)InGameTime;
		CountDownTimer = (float)CountDownTime;
		ShutDownTimer = 0.0f;

		auto fadeEntities = m_context->component->FindEntitiesWithComponent<FadeOutSprite>();
		if(!fadeEntities.empty()){
			fade = m_context->component->GetComponent<FadeOutSprite>(fadeEntities[0]);
		}

		auto scoreEntities = m_context->component->FindEntitiesWithComponent<ScoreManager>();
		if(!scoreEntities.empty()){
			score = m_context->component->GetComponent<ScoreManager>(scoreEntities[0]);
		}
	}
	void OnUpdate(float dt) override{
		if(CountDownTimer > 0.0f){
			CountDownTimer -= dt;
			if(0.0f >= CountDownTimer){
				CountDownTimer = 0.0f;
			}
		} else if(Timer > 0.0f){
			Timer -= dt;
			if(0 >= Timer){
				Timer = 0.0f;
			}
		} else{
			ShutDownTimer += dt;
			if(fade){
				fade->Active = true;

				if(fade->Active && fade->FadeTime <= fade->Timer){
					if(score){


						if(score->BlueScore >= score->RedScore){
							LoadScene("Asset/Scene/scene_win.yaml");
						} else{
							LoadScene("Asset/Scene/scene_lose.yaml");
						}
					} else{
						auto scoreEntities = m_context->component->FindEntitiesWithComponent<ScoreManager>();
						if(!scoreEntities.empty()){
							score = m_context->component->GetComponent<ScoreManager>(scoreEntities[0]);
						}
					}
				}
			} else{
				auto fadeEntities = m_context->component->FindEntitiesWithComponent<FadeOutSprite>();
				if(!fadeEntities.empty()){
					fade = m_context->component->GetComponent<FadeOutSprite>(fadeEntities[0]);
				}
			}
		}
	}
	void OnFixedUpdate(float dt)override{}
	void OnDraw() override{}
	void OnEditorUpdate(float dt)override{}
	void OnStop() override{}

private:
};
