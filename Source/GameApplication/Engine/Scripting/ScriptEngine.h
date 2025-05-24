// ScriptEngine.h
#pragma once
class ScriptEngine {
public:
    ScriptEngine();
    ~ScriptEngine();
    void LoadAssembly(const std::string& path);
};
