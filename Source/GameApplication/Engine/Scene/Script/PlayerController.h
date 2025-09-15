#pragma once
#include "Component/CustomScriptComponent.h"
#include "Backends/checkFileExtention.h"



#include "Component/TransformComponent.h"


class PlayerController: public CustomScriptComponent {
	BEGIN_REFLECT(PlayerController)

		REFLECT_FIELD_INIT(float, moveSpeed,0.0f, REFLECT_INSPECTOR)
		REFLECT_FIELD(float, health, 0.0f)
		REFLECT_FIELD(bool, isInvincible, false)

public:

	PlayerController(): CustomScriptComponent("PlayerController"){}

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
		ImGui::Text(scriptName.c_str());

		INSPECTOR_FIELDS();
	}

	void OnStart() override{
		m_transform = GetComponent<TransformComponent>();
	}
	void OnUpdate(float dt) override{
		if(GetKey('W')){
			m_transform->position.z += 1.0f * dt;
		}
	}
	void OnFixedUpdate(float dt)override{}
	void OnDraw() override{}
	void OnEditorUpdate(float dt)override{}
	void OnStop() override{}

private:

	TransformComponent* m_transform = nullptr;

};
