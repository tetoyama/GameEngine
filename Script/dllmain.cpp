// dllmain.cpp : DLL アプリケーションのエントリ ポイントを定義します。

#include "ScriptRegistry.h"
#include "Scene/System/Script/ScriptModuleAPI.h"

#include "DebugTools/DebugSystem.cpp"

#include <cstring>
#include <new>
#include <string>
#include <variant>

namespace {
	template<typename T>
	bool SetTypedParameter(
		IScriptComponent* script,
		const char* name,
		T value
	){
		if(!script || !name){
			return false;
		}

		for(const ScriptParam& parameter : script->GetParams()){
			if(parameter.name != name){
				continue;
			}

			if(!std::holds_alternative<T>(parameter.value)){
				return false;
			}

			script->SetParam(parameter.name, ParamValue(value));
			return true;
		}

		return false;
	}
}

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
size_t GetScriptParameterCount(IScriptComponent* script){
	return script ? script->GetParams().size() : 0;
}

extern "C" __declspec(dllexport)
bool GetScriptParameter(
	IScriptComponent* script,
	size_t index,
	ScriptParameterData* outParameter
){
	if(!script || !outParameter){
		return false;
	}

	auto& parameters = script->GetParams();
	if(index >= parameters.size()){
		return false;
	}

	const ScriptParam& parameter = parameters[index];
	*outParameter = {};
	outParameter->name = parameter.name.c_str();

	if(const int* value = std::get_if<int>(&parameter.value)){
		outParameter->type = ScriptParameterType::Integer;
		outParameter->intValue = static_cast<int32_t>(*value);
		return true;
	}

	if(const float* value = std::get_if<float>(&parameter.value)){
		outParameter->type = ScriptParameterType::Float;
		outParameter->floatValue = *value;
		return true;
	}

	if(const bool* value = std::get_if<bool>(&parameter.value)){
		outParameter->type = ScriptParameterType::Boolean;
		outParameter->boolValue = *value ? 1 : 0;
		return true;
	}

	if(const std::string* value = std::get_if<std::string>(&parameter.value)){
		outParameter->type = ScriptParameterType::String;
		outParameter->stringValue = value->c_str();
		outParameter->stringSize = value->size();
		return true;
	}

	return false;
}

extern "C" __declspec(dllexport)
bool SetScriptIntegerParameter(
	IScriptComponent* script,
	const char* name,
	int32_t value
){
	return SetTypedParameter<int>(script, name, static_cast<int>(value));
}

extern "C" __declspec(dllexport)
bool SetScriptFloatParameter(
	IScriptComponent* script,
	const char* name,
	float value
){
	return SetTypedParameter<float>(script, name, value);
}

extern "C" __declspec(dllexport)
bool SetScriptBooleanParameter(
	IScriptComponent* script,
	const char* name,
	uint8_t value
){
	return SetTypedParameter<bool>(script, name, value != 0);
}

extern "C" __declspec(dllexport)
bool SetScriptStringParameter(
	IScriptComponent* script,
	const char* name,
	const char* value,
	size_t size
){
	if(!script || !name || (!value && size != 0)){
		return false;
	}

	for(const ScriptParam& parameter : script->GetParams()){
		if(parameter.name != name){
			continue;
		}

		if(!std::holds_alternative<std::string>(parameter.value)){
			return false;
		}

		const std::string text = value
			? std::string(value, size)
			: std::string{};
		script->SetParam(parameter.name, ParamValue(text));
		return true;
	}

	return false;
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
