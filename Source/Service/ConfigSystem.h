// ConfigSystem.h
#pragma once
#include <string>
#include <Backends/yaml-cpp/yaml.h>
class ConfigSystem {
public:
    ConfigSystem();
    ~ConfigSystem();
    bool LoadConfig(const std::wstring& file);
};
