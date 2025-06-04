// ShaderLoader.h
#pragma once
class ShaderLoader {
public:
    ShaderLoader();
    ~ShaderLoader();
    bool LoadShader(const std::string& shaderPath);
};
