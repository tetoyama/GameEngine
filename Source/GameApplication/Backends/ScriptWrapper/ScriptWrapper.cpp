#include "ScriptWrapper.h"

ScriptWrapper::ScriptWrapper(String^ className){
	try{
		// DLL読み込み（ClassLibrary.dll は既にリンクされている）
		Type^ type = Type::GetType(className);
		if(type == nullptr)
			throw gcnew Exception("Script class not found: " + className);

		Object^ obj = Activator::CreateInstance(type);
		instance = safe_cast<ScriptBase^>(obj);
	} catch(Exception^ ex){
		Console::WriteLine("ScriptWrapper Error: {0}", ex->Message);
		instance = nullptr;
	}
}

void ScriptWrapper::OnStart(){
	if(instance != nullptr)
		instance->Start();
}

void ScriptWrapper::OnUpdate(float dt){
	if(instance != nullptr)
		instance->Update(dt);
}

void ScriptWrapper::OnFixedUpdate(float dt){
	if(instance != nullptr)
		instance->FixedUpdate(dt);
}

void ScriptWrapper::OnDraw(){
	if(instance != nullptr)
		instance->Draw();
}

void ScriptWrapper::OnStop(){
	if(instance != nullptr)
		instance->Stop();
}
