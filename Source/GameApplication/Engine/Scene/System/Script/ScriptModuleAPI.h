// =======================================================================
//
// ScriptModuleAPI.h
//
// =======================================================================
#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "Scene/Entity/Entity.h"

class IScriptComponent;

// =======================================================================
// Script module ownership / state / parameter API
// =======================================================================

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

// =======================================================================
// Script -> Engine deferred structural command API
// =======================================================================

// Commit済みEntityと、Command Buffer内で生成予定のEntityを共通に表す。
// STL所有権を持たないため、Engine / Script DLL間を値で渡せる。
struct CommandEntity {
	Entity entity{};
	uint32_t pendingID = 0;

	static constexpr CommandEntity Existing(Entity value) noexcept {
		return {value, 0};
	}

	static constexpr CommandEntity Pending(uint32_t value) noexcept {
		return {{}, value};
	}

	constexpr bool IsPending() const noexcept { return pendingID != 0; }
	constexpr bool IsExisting() const noexcept {
		return !IsPending() && static_cast<bool>(entity);
	}
	constexpr Entity GetExisting() const noexcept { return entity; }
	constexpr uint32_t GetPendingID() const noexcept { return pendingID; }
};

using ScriptQueueCreateEntityFunction = CommandEntity(*)(void*);
using ScriptQueueDestroyEntityFunction = bool(*)(void*, CommandEntity);
using ScriptQueueAddDefaultComponentFunction = bool(*)(
	void*,
	CommandEntity,
	const char*
);
using ScriptQueueRemoveComponentFunction = bool(*)(
	void*,
	CommandEntity,
	const char*
);

// ownerと関数ポインタはEngineが設定する。
// Script DLLは関数を呼ぶだけで、ECBやstd::functionを直接操作しない。
struct ScriptCommandAPI {
	void* owner = nullptr;
	ScriptQueueCreateEntityFunction createEntity = nullptr;
	ScriptQueueDestroyEntityFunction destroyEntity = nullptr;
	ScriptQueueAddDefaultComponentFunction addDefaultComponent = nullptr;
	ScriptQueueRemoveComponentFunction removeComponent = nullptr;

	bool IsValid() const {
		return owner &&
			createEntity &&
			destroyEntity &&
			addDefaultComponent &&
			removeComponent;
	}
};

static_assert(std::is_standard_layout_v<ScriptParameterData>);
static_assert(std::is_trivially_copyable_v<ScriptParameterData>);
static_assert(std::is_trivially_copyable_v<ScriptModuleAPI>);
static_assert(std::is_standard_layout_v<CommandEntity>);
static_assert(std::is_trivially_copyable_v<CommandEntity>);
static_assert(std::is_trivially_copyable_v<ScriptCommandAPI>);
