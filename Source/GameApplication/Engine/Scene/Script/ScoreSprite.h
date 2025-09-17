#pragma once
#include "Component/CustomScriptComponent.h"
#include "Backends/checkFileExtention.h"

class ScoreSprite: public CustomScriptComponent {
public:
	BEGIN_REFLECT(ScoreSprite)
		REFLECT_FIELD(bool, isBlue, false)
		REFLECT_FIELD(int, Degit, 0)

	ScoreSprite(): CustomScriptComponent("ScoreSprite"){}

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

	void OnStart() override{}
	void OnUpdate(float dt) override{
		// ScoreManagerのコンポーネントを持つエンティティを取得
		auto entity = m_context->component->FindEntitiesWithComponent<ScoreManager>();
		if(entity.empty()){
			return;
		}
		if(0 < Degit){
			// ScoreManagerのコンポーネントからスコアを取得
			int Score;
			if(isBlue){
				Score = m_context->component->GetComponent<ScoreManager>(entity[0])->BlueScore;
			} else{
				Score = m_context->component->GetComponent<ScoreManager>(entity[0])->RedScore;
			}
			Score = Score / (int)pow(10, (double)(Degit - 1));
			int SetNum = Score % 10;

			// ScoreSpriteのコンポーネントを取得
			auto Texture = GetComponent<TextureComponent>();
			if(Texture){
				// スコアに応じてテクスチャを設定
				Texture->AnimationNum = SetNum;
			}
		}
	}
	void OnFixedUpdate(float dt)override{}
	void OnDraw() override{}
	void OnEditorUpdate(float dt)override{}
	void OnStop() override{}
private:
};
