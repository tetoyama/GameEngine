#pragma once
#include "Component/CustomScriptComponent.h"
#include "Backends/checkFileExtention.h"

class TimerSprite: public CustomScriptComponent {
public:
	BEGIN_REFLECT(TimerSprite)
		REFLECT_FIELD(bool, isBlue, false)
		REFLECT_FIELD(int, Degit, 0)

		TimerSprite(): CustomScriptComponent("TimerSprite"){}

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
	void OnUpdate(float dt) override{
		// ScoreManagerのコンポーネントを持つエンティティを取得
		auto entity = m_context->component->FindEntitiesWithComponent<GameTimeManager>();
		if(entity.empty()){
			return;
		}
		if(0 < Degit){
			// ScoreManagerのコンポーネントからスコアを取得
			int Score;
			Score = m_context->component->GetComponent<GameTimeManager>(entity[0])->Timer;

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
