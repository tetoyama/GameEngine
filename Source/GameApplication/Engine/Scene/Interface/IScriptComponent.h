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

#include <backends/yaml-cpp/yaml.h>
#include <Scene/scene.h>
#include <Scene/Reference/EntityRef.h>
#include <System/Script/ScriptExecution.h>

// ================================
// Param system
// ================================

using ParamValue = std::variant<int, float, bool, std::string>;

struct ScriptParam {
    std::string name;
    ParamValue  value;
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
    // Life cycle
    // ----------------------------
    void Start() {
        if (!isInitialized) {
            OnStart();
            isInitialized = true;
        }
    }

    void Update(float dt) {
        if (isInitialized) OnUpdate(dt);
    }

    void FixedUpdate(float dt) {
        if (isInitialized) OnFixedUpdate(dt);
    }

    void Draw() {
        if (isInitialized) OnDraw();
    }

    void EditorUpdate(float dt) {
        if (isInitialized) OnEditorUpdate(dt);
    }

    void Stop() {
        if (isInitialized) {
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
    std::vector<ScriptParam>& GetParams() { return params; }

    void SetParam(const std::string& name, const ParamValue& value) {
        for (auto& p : params) {
            if (p.name == name) {
                p.value = value;

                if (p.ptr) {
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
            if (!registered) { \
                ThisType::GetMetaStatic().push_back({ \
                    #Name, offsetof(ThisType, Name), \
                    [](IScriptComponent* c) { \
                        auto* self = static_cast<ThisType*>(c); \
                        auto* ptr = &self->Name; \
                        auto& params = c->GetParams(); \
                        bool found = false; \
                        for (auto& p : params) { \
                            if (p.name == #Name) { \
                                p.ptr = ptr; \
                                p.value = *ptr; \
                                found = true; \
                                break; \
                            } \
                        } \
                        if (!found) { \
                            params.push_back({ #Name, *ptr, ptr }); \
                        } \
                    }, \
                    [](YAML::Node& n, IScriptComponent* c) { \
                        auto* self = static_cast<ThisType*>(c); \
                        n[#Name] = self->Name; \
                    }, \
                    [](const YAML::Node& n, IScriptComponent* c) { \
                        if (!n[#Name]) return; \
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
    for (auto& f : ThisType::GetMetaStatic()) \
        if (f.flags & SCRIPT_REFLECT_INSPECTOR) \
            f.inspector(this)

#define SCRIPT_ENCODE_FIELDS(Node) \
    for (auto& f : ThisType::GetMetaStatic()) \
        if (f.flags & SCRIPT_REFLECT_ENCODE) \
            f.encode(Node, this)

#define SCRIPT_DECODE_FIELDS(Node) \
    for (auto& f : ThisType::GetMetaStatic()) \
        if (f.flags & SCRIPT_REFLECT_DECODE) \
            f.decode(Node, this)
