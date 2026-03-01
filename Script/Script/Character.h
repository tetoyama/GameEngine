#pragma once

#include "Scene/Interface/IScriptComponent.h"
#include "ScriptRegistry.h"

class Character: public IScriptComponent {

	const char* GetScriptName() const override {
		return "Character";
	}

	BEGIN_SCRIPT_REFLECT(Character)

		SCRIPT_REFLECT_FIELD(float, speed, 5.0f)
		SCRIPT_REFLECT_FIELD(int, hp, 100)
		SCRIPT_REFLECT_FIELD(bool, isAlive, true)

public:
		Character() {
		SCRIPT_INSPECTOR_FIELDS();
	}

	YAML::Node Encode() override{
		YAML::Node n;
		SCRIPT_ENCODE_FIELDS(n);
		return n;
	}

	void Decode(const YAML::Node& n) override{
		SCRIPT_DECODE_FIELDS(n);
	}

	void OnStart() override;

	void OnUpdate(float dt) override;
};
REGISTER_SCRIPT("Character", Character)
