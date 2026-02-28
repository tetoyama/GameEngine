#pragma once
#include <unordered_map>
#include <functional>
#include <string>

#include "Scene/Interface/IScriptComponent.h"
#include "Backends/yaml-cpp/yaml.h"

#define REGISTER_SCRIPT(NAME, TYPE) \
namespace { \
    inline const bool TYPE##_auto_registered = []() { \
        ScriptRegistry::Instance().Register( \
            NAME, []() -> IScriptComponent* { return new TYPE(); }); \
        return true; \
    }(); \
}

class ScriptRegistry
{
public:
	using Factory = std::function<IScriptComponent* ()>;

	static ScriptRegistry& Instance(){
		static ScriptRegistry instance;
		return instance;
	}

	void Register(const char* name, Factory factory){
		m_factories[name] = factory;
	}

	IScriptComponent* Create(const char* name){

		OutputDebugStringA(("ScriptRegistry: Create Script [" + std::string(name) + "]\n").c_str());

		auto it = m_factories.find(name);
		if(it == m_factories.end()){
			OutputDebugStringA(("ScriptRegistry: Script [" + std::string(name) + "] not found!\n").c_str());
			return nullptr;
		}
		return it->second();
	}

	void Destroy(IScriptComponent* script) {
		delete script;
	}
	std::unordered_map<std::string, Factory> m_factories;

private:
};

