// =======================================================================
// 
// FadeInSprite.h
// 
// =======================================================================
#pragma once
#include "Component/CustomScriptComponent.h"
#include "Backends/checkFileExtention.h"

// フェードインスプライトスクリプト
class FadeInSprite: public CustomScriptComponent {
public:
	BEGIN_REFLECT(FadeInSprite)
		REFLECT_FIELD(float, FadeTime, 1.0f)

		float Timer = 0.0f;
	FadeInSprite(): CustomScriptComponent("FadeInSprite"){}

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
		Timer = 0.0f;
		material = GetComponentRef<MaterialComponent>();
	}
	void OnUpdate(float dt) override{
		Timer += dt;
		if(material){
			material->Material.BaseColor.w = 1.0f - Timer / FadeTime;
		}
	}
	void OnFixedUpdate(float dt)override{}
	void OnDraw() override{}
	void OnEditorUpdate(float dt)override{}
	void OnStop() override{}
private:
	ComponentRef<MaterialComponent> material;
};
