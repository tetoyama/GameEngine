#pragma once
#include "Component/CustomScriptComponent.h"
#include "Backends/checkFileExtention.h"

class GameTimeManager: public CustomScriptComponent {
public:
	BEGIN_REFLECT(GameTimeManager)
		REFLECT_FIELD_INIT(float, Timer, 0.0f, REFLECT_INSPECTOR)
		REFLECT_FIELD_INIT(float, CountDownTimer, 0.0f, REFLECT_INSPECTOR)
		REFLECT_FIELD_INIT(float, ShutDownTimer, 0.0f, REFLECT_INSPECTOR)
		REFLECT_FIELD(int, CountDownTime, 5)
		REFLECT_FIELD(int, InGameTime, 60)

	bool init = false;

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
		}
	}
	void OnFixedUpdate(float dt)override{}
	void OnDraw() override{}
	void OnEditorUpdate(float dt)override{}
	void OnStop() override{}

private:
};
