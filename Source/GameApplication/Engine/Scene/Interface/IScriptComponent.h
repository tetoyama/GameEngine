#pragma once
#include <backends/yaml-cpp/yaml.h>
#include <backends/ImGui/imgui.h>
#include <backends/myVector3.h>
#include "BackEnds/ImGuiFunc.h"
#include "BackEnds/YAMLConverters.h"

#include <Scene/scene.h>

class IScriptComponent
{
public:
	virtual ~IScriptComponent(){}

	// Script 識別子（唯一の契約）
	virtual const char* GetScriptName() const = 0;

	void Start(){
		if(!isInitialized){
			OnStart();
			isInitialized = true;
		}
	}
	void Update(float dt){
		if(isInitialized){
			OnUpdate(dt);
		}
	}
	void FixedUpdate(float dt){
		if(isInitialized){
			OnFixedUpdate(dt);
		}
	}
	void Draw(){
		if(isInitialized){
			OnDraw();
		}
	}
	void EditorUpdate(float dt){
		if(isInitialized){
			OnEditorUpdate(dt);
		}
	}
	void Stop(){
		if(isInitialized){
			OnStop();
			isInitialized = false;
		}
	}

	virtual YAML::Node Encode(){
		return {};
	}
	virtual void Decode(const YAML::Node&){}
	virtual void Inspector(){}

	Entity entity{};
	SceneContext* context{};

protected:

	// ライフサイクル
	virtual void OnStart(){}
	virtual void OnStop(){}
	virtual void OnUpdate(float dt){}
	virtual void OnFixedUpdate(float dt){}
	virtual void OnEditorUpdate(float dt){}
	virtual void OnDraw(){}


	std::string scriptName = "ScriptComponent";
	bool isInitialized = false; // 初期化フラグ
};

enum ScriptReflectFlags {
	SCRIPT_REFLECT_NONE = 0,
	SCRIPT_REFLECT_INSPECTOR = 1 << 0,
	SCRIPT_REFLECT_ENCODE = 1 << 1,
	SCRIPT_REFLECT_DECODE = 1 << 2,
	SCRIPT_REFLECT_ALL = SCRIPT_REFLECT_INSPECTOR | SCRIPT_REFLECT_ENCODE | SCRIPT_REFLECT_DECODE
};

template<typename T>
struct ScriptFieldHandler;

// float
template<>
struct ScriptFieldHandler<float> {
	static void Decode(const YAML::Node& node, IScriptComponent*, float& v, const char* name){
		if(node[name]) v = node[name].as<float>();
	}
	static void Encode(YAML::Node& node, IScriptComponent*, float& v, const char* name){
		node[name] = v;
	}
	static void Inspector(IScriptComponent*, float& v, const char* name){
		//ImGui::DragFloat(name, &v, 0.1f);
	}
};

// int
template<>
struct ScriptFieldHandler<int> {
	static void Decode(const YAML::Node& node, IScriptComponent*, int& v, const char* name){
		if(node[name]) v = node[name].as<int>();
	}
	static void Encode(YAML::Node& node, IScriptComponent*, int& v, const char* name){
		node[name] = v;
	}
	static void Inspector(IScriptComponent*, int& v, const char* name){
		//ImGui::DragInt(name, &v);
	}
};

// bool
template<>
struct ScriptFieldHandler<bool> {
	static void Decode(const YAML::Node& node, IScriptComponent*, bool& v, const char* name){
		if(node[name]) v = node[name].as<bool>();
	}
	static void Encode(YAML::Node& node, IScriptComponent*, bool& v, const char* name){
		node[name] = v;
	}
	static void Inspector(IScriptComponent*, bool& v, const char* name){
		//ImGui::Checkbox(name, &v);
	}
};

// Vector3
template<>
struct ScriptFieldHandler<Vector3> {
	static void Decode(const YAML::Node& node, IScriptComponent*, Vector3& v, const char* name){
		if(node[name]) v = node[name].as<Vector3>();
	}
	static void Encode(YAML::Node& node, IScriptComponent*, Vector3& v, const char* name){
		node[name] = v;
	}
	static void Inspector(IScriptComponent*, Vector3& v, const char* name){
		//ImGui::DragVec3(name, v);
	}
};

// string
template<>
struct ScriptFieldHandler<std::string> {
	static void Decode(const YAML::Node& node, IScriptComponent*, std::string& v, const char* name){
		if(node[name]) v = node[name].as<std::string>();
	}
	static void Encode(YAML::Node& node, IScriptComponent*, std::string& v, const char* name){
		node[name] = v;
	}
	static void Inspector(IScriptComponent*, std::string& v, const char* name){
		//ImGui::InputText(name, v.data(), v.capacity() + 1);
	}
};

struct ScriptFieldInfo {
	std::string name;
	size_t offset;
	std::function<void(IScriptComponent*, void*)> inspectorFunc;
	std::function<void(YAML::Node&, IScriptComponent*, void*)> encodeFunc;
	std::function<void(const YAML::Node&, IScriptComponent*, void*)> decodeFunc;
	int flags;
};

#define BEGIN_SCRIPT_REFLECT(ClassName) \
public: \
	using ThisType = ClassName; \
	static std::vector<ScriptFieldInfo>& GetMetaStatic(){ \
		static std::vector<ScriptFieldInfo> fields; \
		return fields; \
	}

#define SCRIPT_REFLECT_FIELD(Type, Name, ...) \
	Type Name = __VA_ARGS__; \
	struct AutoRegister_##Name { \
		AutoRegister_##Name(){ \
			static bool registered = false; \
			if(!registered){ \
				ThisType::GetMetaStatic().push_back({ \
					#Name, offsetof(ThisType, Name), \
					[](IScriptComponent* c, void*){ \
						auto* f = reinterpret_cast<Type*>(reinterpret_cast<char*>(c) + offsetof(ThisType, Name)); \
						ScriptFieldHandler<Type>::Inspector(c, *f, #Name); \
					}, \
					[](YAML::Node& n, IScriptComponent* c, void*){ \
						auto* f = reinterpret_cast<Type*>(reinterpret_cast<char*>(c) + offsetof(ThisType, Name)); \
						ScriptFieldHandler<Type>::Encode(n, c, *f, #Name); \
					}, \
					[](const YAML::Node& n, IScriptComponent* c, void*){ \
						auto* f = reinterpret_cast<Type*>(reinterpret_cast<char*>(c) + offsetof(ThisType, Name)); \
						ScriptFieldHandler<Type>::Decode(n, c, *f, #Name); \
					}, \
					SCRIPT_REFLECT_ALL \
				}); \
				registered = true; \
			} \
		} \
	} _autoRegister_##Name;

#define SCRIPT_INSPECTOR_FIELDS() \
	for(auto& f : ThisType::GetMetaStatic()) \
		if(f.flags & SCRIPT_REFLECT_INSPECTOR && f.inspectorFunc) \
			f.inspectorFunc(this, nullptr)

#define SCRIPT_ENCODE_FIELDS(Node) \
	for(auto& f : ThisType::GetMetaStatic()) \
		if(f.flags & SCRIPT_REFLECT_ENCODE && f.encodeFunc) \
			f.encodeFunc(Node, this, nullptr)

#define SCRIPT_DECODE_FIELDS(Node) \
	for(auto& f : ThisType::GetMetaStatic()) \
		if(f.flags & SCRIPT_REFLECT_DECODE && f.decodeFunc) \
			f.decodeFunc(Node, this, nullptr)
