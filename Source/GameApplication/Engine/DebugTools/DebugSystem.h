// DebugSystem.h
#pragma once
#include <string>

class DebugSystem {
public:
    DebugSystem();
    ~DebugSystem();
    void Log(const std::string& message);
};
