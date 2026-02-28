// dllmain.cpp : DLL アプリケーションのエントリ ポイントを定義します。

#include "ScriptRegistry.h"

#include "DebugTools/DebugSystem.cpp"

extern "C" __declspec(dllexport)
IScriptComponent* CreateScript(const char* name){
	if(!name)
		return nullptr;

	return ScriptRegistry::Instance().Create(name);
}

extern "C" __declspec(dllexport)
const char* GetRegisteredScriptName(int index){

	auto& factories = ScriptRegistry::Instance().m_factories;
	if(index < 0 || index >= (int)factories.size())
		return nullptr;
	auto it = factories.begin();
	std::advance(it, index);
	return it->first.c_str();
}

extern "C" __declspec(dllexport)
int GetRegisteredScriptCount(){
	return (int)ScriptRegistry::Instance().m_factories.size();
}