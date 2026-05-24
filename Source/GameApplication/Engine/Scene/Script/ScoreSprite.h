// =======================================================================
// 
// ScoreSprite.h
// 
// =======================================================================
#pragma once
#include "Component/CustomScriptComponent.h"
#include "Backends/checkFileExtention.h"

// スコア表示スプライトスクリプト
class ScoreSprite: public CustomScriptComponent {
public:
	BEGIN_REFLECT(ScoreSprite)
		REFLECT_FIELD(bool, isBlue, false)
		REFLECT_FIELD(int, Digit, 0)

	ComponentRef<ScoreManager> scoreManager;
	ComponentRef<TextureComponent> texture;
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

	void OnStart() override{
		auto entities = m_ref.GetScene()->pComponent->FindEntitiesWithComponent<ScoreManager>();
		if(!entities.empty()){
			m_scoreManager = GetComponentRefFor<ScoreManager>(entities[0]);
		}
		m_texture = GetComponentRef<TextureComponent>();
	}
	void OnUpdate(float dt) override{
		if(!m_scoreManager || Digit <= 0) return;
		int score= isBlue ? m_scoreManager->BlueScore : m_scoreManager->RedScore;
		Score = Score / (int)pow(10, (double)(Digit - 1));
		int setNum= Score % 10;
		if(m_texture){
			m_texture->AnimationNum = SetNum;
		}
	}
	void OnFixedUpdate(float dt)override{}
	void OnDraw() override{}
	void OnEditorUpdate(float dt)override{}
	void OnStop() override{}
private:
};
