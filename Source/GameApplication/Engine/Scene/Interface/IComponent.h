// =======================================================================
// 
// IComponent.h
// 
// =======================================================================
#pragma once
#include <backends/yaml-cpp/yaml.h>
#include <backends/ImGui/imgui.h>
#include <backends/myVector3.h>
#include "Backends/ImGuiFunc.h"
#include "Backends/YAMLConverters.h"
#include <functional>
struct SceneContext;

// 移行期間中のComponent基底。
//
// encode / decode / inspectorはComponent本体の必須責務ではない。
// 新しいComponentはComponentRegistryの外部Operationsへ処理を登録し、
// 既存Componentだけが互換用virtual関数をoverrideする。
class IComponent {
public:
	IComponent() = default;
	virtual ~IComponent() = default;

	virtual YAML::Node encode() {
		return {};
	}

	virtual bool decode(SceneContext*, const YAML::Node&) {
		return false;
	}

	virtual void inspector(SceneContext*) {
	}

	// RegistryがComponentをStorageから除去する直前に呼ぶ。
	// Runtime資源の所有SystemをSceneContextから解決し、明示的に解放する。
	virtual void OnBeforeRemove(SceneContext*) {
	}
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

template<>
struct FieldHandler<float> {
	static void Decode(const YAML::Node& node, IComponent*, float& v, const char* name){
		if(node[name]) v = node[name].as<float>();
	}
	static void Encode(YAML::Node& node, IComponent*, float& v, const char* name){
		node[name] = v;
	}
	static void Inspector(IComponent*, float& v, const char* name){
		ImGui::UndoDragFloat(name, &v, 0.1f);
	}
};

template<>
struct FieldHandler<int> {
	static void Decode(const YAML::Node& node, IComponent*, int& v, const char* name){
		if(node[name]) v = node[name].as<int>();
	}
	static void Encode(YAML::Node& node, IComponent*, int& v, const char* name){
		node[name] = v;
	}
	static void Inspector(IComponent*, int& v, const char* name){
		ImGui::UndoDragInt(name, &v);
	}
};

template<>
struct FieldHandler<bool> {
	static void Decode(const YAML::Node& node, IComponent*, bool& v, const char* name){
		if(node[name]) v = node[name].as<bool>();
	}
	static void Encode(YAML::Node& node, IComponent*, bool& v, const char* name){
		node[name] = v;
	}
	static void Inspector(IComponent*, bool& v, const char* name){
		ImGui::UndoCheckbox(name, &v);
	}
};

template<>
struct FieldHandler<Vector3> {
	static void Decode(const YAML::Node& node, IComponent*, Vector3& v, const char* name){
		if(node[name]) v = node[name].as<Vector3>();
	}
	static void Encode(YAML::Node& node, IComponent*, Vector3& v, const char* name){
		node[name] = v;
	}
	static void Inspector(IComponent*, Vector3& v, const char* name){
		ImGui::UndoDragVec3(name, v);
	}
};

template<>
struct FieldHandler<std::string> {
    static void Decode(const YAML::Node& node, IComponent*, std::string& v, const char* name) {
        if (node[name]) v = node[name].as<std::string>();
    }
    static void Encode(YAML::Node& node, IComponent*, std::string& v, const char* name) {
        node[name] = v;
    }
    static void Inspector(IComponent*, std::string& v, const char* name) {
        ImGui::UndoInputText(name, &v);
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
                    REFLECT_ALL \
                }); \
                registered = true; \
            } \
        } \
    } _autoRegister_##Name;

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
