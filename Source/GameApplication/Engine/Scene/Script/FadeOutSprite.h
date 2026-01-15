#pragma once
#include "Component/CustomScriptComponent.h"
#include "Backends/checkFileExtention.h"
#include <Component/materialComponent.h>
#include <Component/textureComponent.h>

class FadeOutSprite: public CustomScriptComponent {
public:
	BEGIN_REFLECT(FadeOutSprite)
		REFLECT_FIELD(float, FadeTime, 1.0f)
		REFLECT_FIELD_INIT(bool, Active, false,REFLECT_INSPECTOR)


		float Timer = 0.0f;
	FadeOutSprite(): CustomScriptComponent("FadeOutSprite"){}

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
		Active = false;

	}
	void OnUpdate(float dt) override{
		if(!Active){
			return;
		}
		Timer += dt;
		// ScoreSpriteのコンポーネントを取得
		auto Texture = GetComponent<TextureComponent>();
		auto Material = GetComponent<MaterialComponent>();
		if(Material){
			// スコアに応じてテクスチャを設定
			Material->Material.BaseColor.w = Timer / FadeTime;
		}
	}
	void OnFixedUpdate(float dt)override{}
	void OnDraw() override{}
	void OnEditorUpdate(float dt)override{}
	void OnStop() override{}
private:
};
