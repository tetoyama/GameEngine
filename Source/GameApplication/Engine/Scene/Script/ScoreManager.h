#pragma once
#include "Component/CustomScriptComponent.h"

class ScoreManager: public CustomScriptComponent {
public:
	BEGIN_REFLECT(ScoreManager)
		REFLECT_FIELD(int, RedScore, 0)
		REFLECT_FIELD(int, BlueScore, 0)

	ScoreManager(): CustomScriptComponent("ScoreManager"){}

	YAML::Node encode() override{
		YAML::Node node;
		ENCODE_FIELDS(node);

		return node;
	}
	bool decode(SceneContext* context, const YAML::Node& node) override{
		DECODE_FIELDS(node);

		return true;
	}

	void inspector(SceneContext* context) override{
		INSPECTOR_FIELDS();
	}

	void OnStart() override{
		RedScore = 0;
		BlueScore = 0;
	}
	void OnUpdate(float dt) override{}
	void OnFixedUpdate(float dt)override{}
	void OnDraw() override{}
	void OnEditorUpdate(float dt)override{}
	void OnStop() override{}

private:
};
