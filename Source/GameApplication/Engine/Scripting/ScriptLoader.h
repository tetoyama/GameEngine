// ScriptLoader.h
#pragma once
#include <string>
class ScriptLoader {
public:
    ScriptLoader();
    ~ScriptLoader();
    void LoadScript(const std::string& path);
};
