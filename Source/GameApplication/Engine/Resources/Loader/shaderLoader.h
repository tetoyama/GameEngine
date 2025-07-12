// ShaderLoader.h
#pragma once
#include <string>
#include <unordered_map>
#include <memory>

class GraphicsContext;

struct VertexShaderData;
struct PixelShaderData;

class ShaderLoader {
public:
    ShaderLoader(GraphicsContext* set):m_GraphicContext(set){}
	~ShaderLoader(){}

	std::shared_ptr<VertexShaderData> LoadVertexShader(const std::string& shaderPath);
	std::shared_ptr<PixelShaderData> LoadPixelShader(const std::string& shaderPath);

	std::shared_ptr<VertexShaderData> GetVertexShader(const std::string& shaderPath);
	std::shared_ptr<PixelShaderData> GetPixelShader(const std::string& shaderPath);

	void SetVertexShader(const std::string& shaderPath);
	void SetPixelShader(const std::string& shaderPath);
private:
	std::unordered_map<std::string, std::shared_ptr<VertexShaderData>> m_VertexShaders;
	std::unordered_map<std::string, std::shared_ptr<PixelShaderData> > m_PixelShaders;
	GraphicsContext* m_GraphicContext;
};
