// ConfigSystem.h
#pragma once
class ConfigSystem {
public:
    ConfigSystem();
    ~ConfigSystem();
    bool LoadConfig(const std::wstring& file);
};
