// dllmain.cpp : DLL アプリケーションのエントリ ポイントを定義します。

#include "ScriptRegistry.h"

#include "DebugTools/DebugSystem.cpp"

#include <cstring>
#include <new>
#include <string>

extern "C" __declspec(dllexport)
IScriptComponent* CreateScript(const char* name){
	if(!name)
		return nullptr;

	return ScriptRegistry::Instance().Create(name);
}

// CreateScriptで生成したインスタンスは、必ず同じDLL内のCRTで破棄する。
extern "C" __declspec(dllexport)
void DestroyScript(IScriptComponent* script){
	if(!script)
		return;

	ScriptRegistry::Instance().Destroy(script);
}

// ScriptのYAML状態をDLL内で文字列化する。
// outDataの所有権はDLL側にあり、FreeScriptStateBufferで解放する。
extern "C" __declspec(dllexport)
bool SerializeScriptState(
	IScriptComponent* script,
	char** outData,
	size_t* outSize
){
	if(!script || !outData || !outSize){
		return false;
	}

	*outData = nullptr;
	*outSize = 0;

	try {
		YAML::Emitter emitter;
		emitter << script->Encode();
		if(!emitter.good()){
			return false;
		}

		const char* source = emitter.c_str();
		const size_t size = std::strlen(source);
		char* buffer = new(std::nothrow) char[size + 1];
		if(!buffer){
			return false;
		}

		std::memcpy(buffer, source, size);
		buffer[size] = '\0';

		*outData = buffer;
		*outSize = size;
		return true;
	} catch(...){
		return false;
	}
}

extern "C" __declspec(dllexport)
void FreeScriptStateBuffer(char* data){
	delete[] data;
}

// Engine側から受け取ったUTF-8文字列をDLL内のyaml-cppで復元する。
extern "C" __declspec(dllexport)
bool DeserializeScriptState(
	IScriptComponent* script,
	const char* data,
	size_t size
){
	if(!script || (!data && size != 0)){
		return false;
	}

	try {
		const std::string text = data
			? std::string(data, size)
			: std::string{};
		const YAML::Node node = text.empty()
			? YAML::Node{}
			: YAML::Load(text);
		script->Decode(node);
		return true;
	} catch(...){
		return false;
	}
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
