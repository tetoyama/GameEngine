#pragma once
#include "Component/CustomScriptComponent.h"
#include "Backends/checkFileExtention.h"

class ScoreManager: public CustomScriptComponent {
public:
	BEGIN_REFLECT(ScoreManager)
		REFLECT_FIELD_INIT(int, RedScore, 0,REFLECT_INSPECTOR)
		REFLECT_FIELD_INIT(int, BlueScore, 0, REFLECT_INSPECTOR)

	ScoreManager(): CustomScriptComponent("ScoreManager"){}

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

	void OnStart() override{}
	void OnUpdate(float dt) override{}
	void OnFixedUpdate(float dt)override{}
	void OnDraw() override{}
	void OnEditorUpdate(float dt)override{}
	void OnStop() override{}

private:
};
