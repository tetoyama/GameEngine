// =======================================================================
//
// ScriptModuleAPI.h
//
// =======================================================================
#pragma once

#include <cstddef>
#include <cstdint>

class IScriptComponent;

enum class ScriptParameterType : uint8_t {
	Integer,
	Float,
	Boolean,
	String
};

// DLLが所有するParameterを一時的に参照するPOD View。
// name / stringValueは次のDLL API呼び出しまでにEngine側へコピーして使用する。
struct ScriptParameterData {
	const char* name = nullptr;
	ScriptParameterType type = ScriptParameterType::Integer;

	int32_t intValue = 0;
	float floatValue = 0.0f;
	uint8_t boolValue = 0;

	const char* stringValue = nullptr;
	size_t stringSize = 0;
};

using ScriptDestroyFunction = void(*)(IScriptComponent*);
using ScriptSerializeFunction = bool(*)(IScriptComponent*, char**, size_t*);
using ScriptFreeBufferFunction = void(*)(char*);
using ScriptDeserializeFunction = bool(*)(IScriptComponent*, const char*, size_t);

using ScriptGetParameterCountFunction = size_t(*)(IScriptComponent*);
using ScriptGetParameterFunction = bool(*)(
	IScriptComponent*,
	size_t,
	ScriptParameterData*
);
using ScriptSetIntegerParameterFunction = bool(*)(
	IScriptComponent*,
	const char*,
	int32_t
);
using ScriptSetFloatParameterFunction = bool(*)(
	IScriptComponent*,
	const char*,
	float
);
using ScriptSetBooleanParameterFunction = bool(*)(
	IScriptComponent*,
	const char*,
	uint8_t
);
using ScriptSetStringParameterFunction = bool(*)(
	IScriptComponent*,
	const char*,
	const char*,
	size_t
);

struct ScriptModuleAPI {
	ScriptDestroyFunction destroy = nullptr;
	ScriptSerializeFunction serialize = nullptr;
	ScriptFreeBufferFunction freeBuffer = nullptr;
	ScriptDeserializeFunction deserialize = nullptr;

	ScriptGetParameterCountFunction getParameterCount = nullptr;
	ScriptGetParameterFunction getParameter = nullptr;
	ScriptSetIntegerParameterFunction setIntegerParameter = nullptr;
	ScriptSetFloatParameterFunction setFloatParameter = nullptr;
	ScriptSetBooleanParameterFunction setBooleanParameter = nullptr;
	ScriptSetStringParameterFunction setStringParameter = nullptr;

	bool IsValid() const {
		return destroy &&
			serialize &&
			freeBuffer &&
			deserialize &&
			getParameterCount &&
			getParameter &&
			setIntegerParameter &&
			setFloatParameter &&
			setBooleanParameter &&
			setStringParameter;
	}
};
