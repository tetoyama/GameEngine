#include "ScriptWrapper.h"
#ifdef _DEBUG  
#using <../ClassLibrary/bin/Debug/net4.8/ClassLibrary.dll> // ビルドしたC# DLL  
#else  
#using <../ClassLibrary/bin/Release/net4.8/ClassLibrary.dll> // ビルドしたC# DLL  
#endif  


using namespace System::Diagnostics;
//#include "../../../../Source/GameApplication/Engine/DebugTools/DebugSystem.h"

ScriptWrapper::ScriptWrapper(String^ className){
	try{
		// DLL読み込み（ClassLibrary.dll は既にリンクされている）
		Type^ type = Type::GetType(className);
		if(type == nullptr)
			throw gcnew Exception("Script class not found: " + className);

		Object^ obj = Activator::CreateInstance(type);
		instance = safe_cast<ScriptBase^>(obj);
	} catch(Exception^ ex){
		System::Diagnostics::Debugger::Log(0,"","ScriptWrapper Exception: " + ex->ToString());
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

void ScriptWrapper::SetLogCallback(LogCallbackFn callback){
	logCallback = callback;
}

void ScriptWrapper::Log(System::String^ message){

	if(message == nullptr) return;

	// System::String^ → std::string に変換
	using namespace System::Runtime::InteropServices;
	IntPtr ptr = Marshal::StringToHGlobalAnsi(message);
	const char* cstr = static_cast<const char*>(ptr.ToPointer());
	System::Diagnostics::Debugger::Log(0, message, message);

	Marshal::FreeHGlobal(ptr); // メモリ解放}
}