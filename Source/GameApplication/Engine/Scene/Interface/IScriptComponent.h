// =======================================================================
//
// IScriptComponent.h
//
// =======================================================================
#pragma once

#include <vector>
#include <string>
#include <variant>
#include <functional>
#include <type_traits>
#include <typeinfo>
#include <utility>

#include <backends/yaml-cpp/yaml.h>
#include <Scene/scene.h>
#include <Scene/Reference/EntityRef.h>
#include <System/Script/ScriptExecution.h>
#include <System/Script/ScriptModuleAPI.h>

// ================================
// Param system
// ================================

using ParamValue = std::variant<int, float, bool, std::string>;

struct ScriptParam {
	std::string name;
	ParamValue value;
	void* ptr;
};

// ================================
// IScriptComponent
// ================================

class IScriptComponent {
public:
	virtual ~IScriptComponent() = default;

	virtual const char* GetScriptName() const = 0;

	void SetExecutionOrder(
		SystemTaskDomain domain,
		SystemPhase phase,
		int32_t priority
	) {
		executionSettings.SetOrder(domain, phase, priority);
	}

	SystemTaskOrder GetExecutionOrder(SystemTaskDomain domain) const {
		return executionSettings.GetOrder(domain);
	}

	void SetRegistrationOrder(uint64_t order) {
		executionSettings.registrationOrder = order;
	}

	bool HasRegistrationOrder() const {
		return executionSettings.registrationOrder != 0;
	}

	// ----------------------------
	// Deferred structural changes
	// ----------------------------
	// Script DLLはEntityCommandBufferへ直接触れない。
	// すべてEngineが設定したPOD関数ポインタ経由で即時にキュー登録する。

	CommandEntity QueueCreateEntity() {
		const ScriptCommandAPI* commands = GetCommandAPI();
		return commands && commands->createEntity
			? commands->createEntity(commands->owner)
			: CommandEntity{};
	}

	bool QueueDestroyEntity(Entity entity) {
		return QueueDestroyEntity(CommandEntity::Existing(entity));
	}

	bool QueueDestroyEntity(CommandEntity entity) {
		const ScriptCommandAPI* commands = GetCommandAPI();
		if(!commands || !commands->destroyEntity) return false;
		return commands->destroyEntity(commands->owner, entity);
	}

	bool QueueDestroySelf() {
		return QueueDestroyEntity(ref.GetEntityID());
	}

	// DLL境界では任意のC++引数を安全に保持できないため、
	// Script標準APIのComponent追加はデフォルト構築だけを許可する。
	template<typename T, typename... Args>
	bool QueueAddComponent(Entity entity, Args&&...) {
		static_assert(
			sizeof...(Args) == 0,
			"Script QueueAddComponent supports default construction only. "
			"Use an explicit POD Engine bridge for initialization data."
		);
		return QueueAddComponent<T>(CommandEntity::Existing(entity));
	}

	template<typename T, typename... Args>
	bool QueueAddComponent(CommandEntity entity, Args&&...) {
		static_assert(
			sizeof...(Args) == 0,
			"Script QueueAddComponent supports default construction only. "
			"Use an explicit POD Engine bridge for initialization data."
		);

		const ScriptCommandAPI* commands = GetCommandAPI();
		if(!commands || !commands->addDefaultComponent) return false;
		return commands->addDefaultComponent(
			commands->owner,
			entity,
			typeid(T).name()
		);
	}

	template<typename T>
	bool QueueRemoveComponent(Entity entity) {
		return QueueRemoveComponent<T>(CommandEntity::Existing(entity));
	}

	template<typename T>
	bool QueueRemoveComponent(CommandEntity entity) {
		const ScriptCommandAPI* commands = GetCommandAPI();
		if(!commands || !commands->removeComponent) return false;
		return commands->removeComponent(
			commands->owner,
			entity,
			typeid(T).name()
		);
	}

	// ----------------------------
	// Life cycle
	// ----------------------------
	void Start() {
		if(!isInitialized) {
			OnStart();
			isInitialized = true;
		}
	}

	void Update(float dt) {
		if(isInitialized) OnUpdate(dt);
	}

	void FixedUpdate(float dt) {
		if(isInitialized) OnFixedUpdate(dt);
	}

	void Draw() {
		if(isInitialized) OnDraw();
	}

	void EditorUpdate(float dt) {
		if(isInitialized) OnEditorUpdate(dt);
	}

	void Stop() {
		if(isInitialized) {
			OnStop();
			isInitialized = false;
		}
	}

	// ----------------------------
	// YAML
	// ----------------------------
	virtual YAML::Node Encode() { return {}; }
	virtual void Decode(const YAML::Node&) {}

	// ----------------------------
	// Param access
	// ----------------------------
	// DLL内部のBridge実装だけが使用する。
	std::vector<ScriptParam>& GetParams() { return params; }

	void SetParam(const std::string& name, const ParamValue& value) {
		for(auto& p : params) {
			if(p.name == name) {
				p.value = value;

				if(p.ptr) {
					std::visit([&](auto&& v) {
						using T = std::decay_t<decltype(v)>;
						*reinterpret_cast<T*>(p.ptr) = v;
					}, value);
				}
				return;
			}
		}
	}

	EntityRef ref{};

protected:
	virtual void OnStart() {}
	virtual void OnStop() {}
	virtual void OnUpdate(float) {}
	virtual void OnFixedUpdate(float) {}
	virtual void OnEditorUpdate(float) {}
	virtual void OnDraw() {}

	const ScriptCommandAPI* GetCommandAPI() const {
		SceneContext* context = ref.GetScene();
		return context && context->scriptCommands.IsValid()
			? &context->scriptCommands
			: nullptr;
	}

	ScriptExecutionSettings executionSettings;
	bool isInitialized = false;
	std::vector<ScriptParam> params;
};

// ================================
// Reflection system
// ================================

enum ScriptReflectFlags {
	SCRIPT_REFLECT_NONE = 0,
	SCRIPT_REFLECT_INSPECTOR = 1 << 0,
	SCRIPT_REFLECT_ENCODE = 1 << 1,
	SCRIPT_REFLECT_DECODE = 1 << 2,
	SCRIPT_REFLECT_ALL =
	SCRIPT_REFLECT_INSPECTOR |
	SCRIPT_REFLECT_ENCODE |
	SCRIPT_REFLECT_DECODE
};

struct ScriptFieldInfo {
	const char* name;
	size_t offset;
	std::function<void(IScriptComponent*)> inspector;
	std::function<void(YAML::Node&, IScriptComponent*)> encode;
	std::function<void(const YAML::Node&, IScriptComponent*)> decode;
	int flags;
};

// ================================
// Macros
// ================================

#define BEGIN_SCRIPT_REFLECT(ClassName) \
public: \
	using ThisType = ClassName; \
	static std::vector<ScriptFieldInfo>& GetMetaStatic() { \
		static std::vector<ScriptFieldInfo> fields; \
		return fields; \
	}

#define SCRIPT_REFLECT_FIELD(Type, Name, DefaultValue) \
	Type Name = DefaultValue; \
	struct AutoRegister_##Name { \
		AutoRegister_##Name() { \
			static bool registered = false; \
			if(!registered) { \
				ThisType::GetMetaStatic().push_back({ \
					#Name, offsetof(ThisType, Name), \
					[](IScriptComponent* c) { \
						auto* self = static_cast<ThisType*>(c); \
						auto* ptr = &self->Name; \
						auto& params = c->GetParams(); \
						bool found = false; \
						for(auto& p : params) { \
							if(p.name == #Name) { \
								p.ptr = ptr; \
								p.value = *ptr; \
								found = true; \
								break; \
							} \
						} \
						if(!found) { \
							params.push_back({#Name, *ptr, ptr}); \
						} \
					}, \
					[](YAML::Node& n, IScriptComponent* c) { \
						auto* self = static_cast<ThisType*>(c); \
						n[#Name] = self->Name; \
					}, \
					[](const YAML::Node& n, IScriptComponent* c) { \
						if(!n[#Name]) return; \
						auto* self = static_cast<ThisType*>(c); \
						self->Name = n[#Name].as<Type>(); \
						c->SetParam(#Name, self->Name); \
					}, \
					SCRIPT_REFLECT_ALL \
				}); \
				registered = true; \
			} \
		} \
	} _autoRegister_##Name;

#define SCRIPT_INSPECTOR_FIELDS() \
	for(auto& f : ThisType::GetMetaStatic()) \
		if(f.flags & SCRIPT_REFLECT_INSPECTOR) \
			f.inspector(this)

#define SCRIPT_ENCODE_FIELDS(Node) \
	for(auto& f : ThisType::GetMetaStatic()) \
		if(f.flags & SCRIPT_REFLECT_ENCODE) \
			f.encode(Node, this)

#define SCRIPT_DECODE_FIELDS(Node) \
	for(auto& f : ThisType::GetMetaStatic()) \
		if(f.flags & SCRIPT_REFLECT_DECODE) \
			f.decode(Node, this)
