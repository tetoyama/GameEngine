
#pragma once

#include <string>

#using <System.dll>
#using <ClassLibrary.dll> // ビルドしたC# DLL

using namespace System;
using namespace ClassLibrary; // ← C#のnamespace

public ref class ScriptWrapper {
private:
	ScriptBase^ instance;

public:
	ScriptWrapper(String^ className); // ctor
	void OnStart();
	void OnUpdate(float dt);
	void OnStop();
};