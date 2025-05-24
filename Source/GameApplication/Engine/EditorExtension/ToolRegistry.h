// ToolRegistry.h
#pragma once
class ToolRegistry {
public:
    ToolRegistry();
    ~ToolRegistry();
    void RegisterTool(const std::string& name);
};
