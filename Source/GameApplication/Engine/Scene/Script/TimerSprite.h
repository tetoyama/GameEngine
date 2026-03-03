// =======================================================================
// 
// TimerSprite.h
// 
// =======================================================================
#pragma once
#include "Component/CustomScriptComponent.h"
#include "Backends/checkFileExtention.h"

// タイマー表示スプライトスクリプト
class TimerSprite: public CustomScriptComponent {
public:
	BEGIN_REFLECT(TimerSprite)
		REFLECT_FIELD(bool, isBlue, false)
		REFLECT_FIELD(int, Degit, 0)

	ComponentRef<GameTimeManager> m_gameTime;
	ComponentRef<TextureComponent> m_texture;
	TimerSprite(): CustomScriptComponent("TimerSprite"){}

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
		auto entities = m_ref.GetScene()->component->FindEntitiesWithComponent<GameTimeManager>();
		if(!entities.empty()){
			m_gameTime = GetComponentRefFor<GameTimeManager>(entities[0]);
		}
		m_texture = GetComponentRef<TextureComponent>();
	}
	void OnUpdate(float dt) override{
		if(!m_gameTime || Degit <= 0) return;
		int Score = (int)m_gameTime->Timer;
		Score = Score / (int)pow(10, (double)(Degit - 1));
		int SetNum = Score % 10;
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
