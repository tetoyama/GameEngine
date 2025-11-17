// Engine/Scene/Interface/IComponent.h
#pragma once
#include <backends/yaml-cpp/yaml.h>
#include <backends/ImGui/imgui.h>
#include <backends/myVector3.h>
#include "Service/ImGuiFunc.h"
#include "Service/YAMLConverters.h"
#include <functional>
struct SceneContext;

class IComponent {
public:
	IComponent() = default;
	virtual ~IComponent() = default;

	virtual YAML::Node encode() = 0;

	virtual bool decode(SceneContext* context, const YAML::Node& node) = 0;

	virtual void inspector(SceneContext* context) = 0;
};


// ===========================================
// フラグ
// ===========================================
enum ReflectFlags {
	REFLECT_NONE = 0,
	REFLECT_INSPECTOR = 1 << 0,
	REFLECT_ENCODE = 1 << 1,
	REFLECT_DECODE = 1 << 2,
	REFLECT_ALL = REFLECT_INSPECTOR | REFLECT_ENCODE | REFLECT_DECODE
};

// ===========================================
// 型ごとのハンドラ
// ===========================================
template<typename T>
struct FieldHandler;

// float
template<>
struct FieldHandler<float> {
	static void Decode(const YAML::Node& node, IComponent*, float& v, const char* name){
		if(node[name]) v = node[name].as<float>();
	}
	static void Encode(YAML::Node& node, IComponent*, float& v, const char* name){
		node[name] = v;
	}
	static void Inspector(IComponent*, float& v, const char* name){
		ImGui::DragFloat(name, &v, 0.1f);
	}
};

// int
template<>
struct FieldHandler<int> {
	static void Decode(const YAML::Node& node, IComponent*, int& v, const char* name){
		if(node[name]) v = node[name].as<int>();
	}
	static void Encode(YAML::Node& node, IComponent*, int& v, const char* name){
		node[name] = v;
	}
	static void Inspector(IComponent*, int& v, const char* name){
		ImGui::DragInt(name, &v);
	}
};

// bool
template<>
struct FieldHandler<bool> {
	static void Decode(const YAML::Node& node, IComponent*, bool& v, const char* name){
		if(node[name]) v = node[name].as<bool>();
	}
	static void Encode(YAML::Node& node, IComponent*, bool& v, const char* name){
		node[name] = v;
	}
	static void Inspector(IComponent*, bool& v, const char* name){
		ImGui::Checkbox(name, &v);
	}
};

// Vector3
template<>
struct FieldHandler<Vector3> {
	static void Decode(const YAML::Node& node, IComponent*, Vector3& v, const char* name){
		if(node[name]) v = node[name].as<Vector3>();
	}
	static void Encode(YAML::Node& node, IComponent*, Vector3& v, const char* name){
		node[name] = v;
	}
	static void Inspector(IComponent*, Vector3& v, const char* name){
		ImGui::DragVec3(name, v);
	}
};

// String
template<>
struct FieldHandler<std::string> {
    static void Decode(const YAML::Node& node, IComponent*, std::string& v, const char* name) {
        if (node[name]) v = node[name].as<std::string>();
    }
    static void Encode(YAML::Node& node, IComponent*, std::string& v, const char* name) {
        node[name] = v;
    }
    static void Inspector(IComponent*, std::string& v, const char* name) {
        ImGui::InputText(name,(char*)v.c_str(),v.size());
    }
};


// ===========================================
// フィールド情報
// ===========================================
struct FieldInfo {
	std::string name;
	size_t offset;
	std::function<void(IComponent*, void*)> inspectorFunc;
	std::function<void(YAML::Node&, IComponent*, void*)> encodeFunc;
	std::function<void(const YAML::Node&, IComponent*, void*)> decodeFunc;
	int flags;
};

// ===========================================
// マクロ
// ===========================================
#define BEGIN_REFLECT(ClassName) \
public: \
    using ThisType = ClassName; \
    static std::vector<FieldInfo>& GetMetaStatic() { \
        static std::vector<FieldInfo> fields; \
        return fields; \
    }

#define REFLECT_FIELD(Type, Name, ...) \
    Type Name = __VA_ARGS__; \
    struct AutoRegister_##Name { \
        AutoRegister_##Name() { \
            static bool registered = false; \
            if(!registered) { \
                ThisType::GetMetaStatic().push_back({ \
                    #Name, \
                    offsetof(ThisType, Name), \
                    /* inspector */ [](IComponent* c, void*) { \
                        Type* field = reinterpret_cast<Type*>(reinterpret_cast<char*>(c) + offsetof(ThisType, Name)); \
                        FieldHandler<Type>::Inspector(c, *field, #Name); \
                    }, \
                    /* encode */ [](YAML::Node& node, IComponent* c, void*) { \
                        Type* field = reinterpret_cast<Type*>(reinterpret_cast<char*>(c) + offsetof(ThisType, Name)); \
                        FieldHandler<Type>::Encode(node, c, *field, #Name); \
                    }, \
                    /* decode */ [](const YAML::Node& node, IComponent* c, void*) { \
                        Type* field = reinterpret_cast<Type*>(reinterpret_cast<char*>(c) + offsetof(ThisType, Name)); \
                        FieldHandler<Type>::Decode(node, c, *field, #Name); \
                    }, \
                    REFLECT_ALL \
                }); \
                registered = true; \
            } \
        } \
    } _autoRegister_##Name;

// ===========================================
// 初期値あり・フラグ指定あり
// ===========================================
#define REFLECT_FIELD_INIT(Type, Name, InitialValue, Flags) \
    Type Name = InitialValue; \
    struct AutoRegister_##Name { \
        AutoRegister_##Name() { \
            static bool registered = false; \
            if(!registered){ \
                ThisType::GetMetaStatic().push_back({ \
                    #Name, offsetof(ThisType, Name), \
                    [](IComponent* c, void*) { \
                        Type* field = reinterpret_cast<Type*>(reinterpret_cast<char*>(c) + offsetof(ThisType, Name)); \
                        FieldHandler<Type>::Inspector(c, *field, #Name); \
                    }, \
                    [](YAML::Node& node, IComponent* c, void*) { \
                        Type* field = reinterpret_cast<Type*>(reinterpret_cast<char*>(c) + offsetof(ThisType, Name)); \
                        FieldHandler<Type>::Encode(node, c, *field, #Name); \
                    }, \
                    [](const YAML::Node& node, IComponent* c, void*) { \
                        Type* field = reinterpret_cast<Type*>(reinterpret_cast<char*>(c) + offsetof(ThisType, Name)); \
                        FieldHandler<Type>::Decode(node, c, *field, #Name); \
                    }, \
                    Flags \
                }); \
                registered = true; \
            } \
        } \
    } _autoRegister_##Name;

// 呼び出し用マクロ
#define INSPECTOR_FIELDS() \
    for(auto& f : ThisType::GetMetaStatic()) { \
        if(f.flags & REFLECT_INSPECTOR && f.inspectorFunc) f.inspectorFunc(this, nullptr); \
    }

#define ENCODE_FIELDS(Node) \
    for(auto& f : ThisType::GetMetaStatic()) { \
        if(f.flags & REFLECT_ENCODE && f.encodeFunc) f.encodeFunc(Node, this, nullptr); \
    }

#define DECODE_FIELDS(Node) \
    for(auto& f : ThisType::GetMetaStatic()) { \
        if(f.flags & REFLECT_DECODE && f.decodeFunc) f.decodeFunc(Node, this, nullptr); \
    }