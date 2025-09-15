#pragma once
#include "Component/CustomScriptComponent.h"
#include "Backends/checkFileExtention.h"

class BallController: public CustomScriptComponent {
public:
	BEGIN_REFLECT(BallController)
	TransformComponent* transform = nullptr;
	ScoreManager* scoreManager = nullptr;
	

	BallController(): CustomScriptComponent("BallController"){}

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
		transform = GetComponent<TransformComponent>();
		auto entity = m_context->component->FindEntitiesWithComponent<ScoreManager>();
		if(entity.empty()){
			return;
		} else{
			scoreManager = m_context->component->GetComponent<ScoreManager>(entity[0]);
		}
	}
	void OnUpdate(float dt) override{
		if(transform->position.x > 5.5f && abs(transform->position.z) < 0.7f){
			transform->position = Vector3(0.0f, 1.0f, 0.0f);
			scoreManager->RedScore++;
		}


		if(transform->position.x < -5.4f && abs(transform->position.z) < 0.7f){
			transform->position = Vector3(0.0f, 1.0f, 0.0f);
			scoreManager->BlueScore++;

		}
	}
	void OnFixedUpdate(float dt)override{}
	void OnDraw() override{}
	void OnEditorUpdate(float dt)override{}
	void OnStop() override{}
private:
};
