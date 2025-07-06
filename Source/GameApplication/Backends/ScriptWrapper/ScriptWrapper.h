#pragma once  

#include <string>  
#include "ScriptWrapperAPI.h"

// Ensure the /clr compiler option is enabled for C++/CLI mode  
#ifdef _DEBUG  
#using <../ClassLibrary/bin/Debug/net4.8/ClassLibrary.dll> // ビルドしたC# DLL  
#else  
#using <../ClassLibrary/bin/Release/net4.8/ClassLibrary.dll> // ビルドしたC# DLL  
#endif  

using namespace System;  

public ref class ScriptWrapper {  
private:  
	ScriptBase^ instance;  
	static LogCallbackFn logCallback = nullptr;

public:  
	ScriptWrapper(String^ className); // ctor  
	void OnStart();  
	void OnUpdate(float dt);  
	void OnFixedUpdate(float dt);  
	void OnDraw();
	void OnStop();

	static void SetLogCallback(LogCallbackFn callback);

	void Log(System::String^ message); // C#から呼ばれる
};
