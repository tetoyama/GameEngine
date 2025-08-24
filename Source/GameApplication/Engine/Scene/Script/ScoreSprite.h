#pragma once
#include "Component/CustomScriptComponent.h"
#include "Backends/checkFileExtention.h"

class ScoreSprite: public CustomScriptComponent {
public:

	ScoreSprite(): CustomScriptComponent("ScoreSprite"){}

	YAML::Node encode() override{
		YAML::Node node;
		node["ScriptName"] = scriptName;
		node["Degit"] = Degit;
		return node;
	}
	bool decode(const YAML::Node& node) override{
		if(node["ScriptName"])
			scriptName = node["ScriptName"].as<std::string>();
		if(node["Degit"])
			Degit = node["Degit"].as<int>();
		return true;
	}

	void inspector(SceneContext* context) override{
		ImGui::Text(scriptName.c_str());
		ImGui::InputInt("Degit", &Degit,1,0);
	}

	void OnStart() override{}
	void OnUpdate(float dt) override{
		// ScoreManagerのコンポーネントを持つエンティティを取得
		auto entity = m_context->component->FindEntitiesWithComponent<ScoreManager>();
		if(entity.empty()){
			return;
		}
		// ScoreManagerのコンポーネントからスコアを取得
		int Score = m_context->component->GetComponent<ScoreManager>(entity[0])->Score;
		Score = Score / (int)pow(10, (double)(Degit - 1));
		int SetNum = Score % 10;

		// ScoreSpriteのコンポーネントを取得
		auto Texture = GetComponent<TextureComponent>();
		if(Texture){
			// スコアに応じてテクスチャを設定
			Texture->AnimationNum = SetNum;
		}
	}
	void OnFixedUpdate(float dt)override{}
	void OnDraw() override{}
	void OnEditorUpdate(float dt)override{}
	void OnStop() override{}

	int Degit = 1;
private:
};
