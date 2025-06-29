#pragma once
#include <string>
#include "ScriptWrapperAPI.h" // 上で定義したマクロを include

class SCRIPTWRAPPER_API NativeScriptHandle {
public:
	NativeScriptHandle(const std::string& scriptName);
	~NativeScriptHandle();

	void OnStart();
	void OnUpdate(float dt);
	void OnFixedUpdate(float dt);
	void OnDraw();
	void OnStop();

private:
	class Impl;
	Impl* impl;
};
