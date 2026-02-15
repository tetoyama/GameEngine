// dllmain.cpp : DLL アプリケーションのエントリ ポイントを定義します。

#include "ScriptRegistry.h"

#include "DebugTools/DebugSystem.cpp"

extern "C" __declspec(dllexport)
IScriptComponent* CreateScript(const char* name){
	if(!name)
		return nullptr;

	return ScriptRegistry::Instance().Create(name);
}
