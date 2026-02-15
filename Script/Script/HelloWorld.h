#pragma once

#include "Scene/Interface/IScriptComponent.h"
#include "ScriptRegistry.h"
#include <string>

class HelloWorld : public IScriptComponent {

	const char* GetScriptName() const override {
		return "HelloWorld";
	}

	BEGIN_SCRIPT_REFLECT(HelloWorld)
		SCRIPT_REFLECT_FIELD(std::string, text, "Hello World!")

public:
	HelloWorld() {
		SCRIPT_INSPECTOR_FIELDS();
	}

	YAML::Node Encode() override {
		YAML::Node n;
		SCRIPT_ENCODE_FIELDS(n);
		return n;
	}

	void Decode(const YAML::Node& n) override {
		SCRIPT_DECODE_FIELDS(n);
	}

	void OnStart() override;
};


