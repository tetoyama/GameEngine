#include "shaderLoader.h"
#include "Backends/checkFileExtention.h"
#include "Engine/Graphics/graphicsContext.h"
#include "Engine/Resources/Data/vertexShaderData.h"
#include "Engine/Resources/Data/pixelShaderData.h"

VertexShaderData* ShaderLoader::LoadVertexShader(const std::string& shaderPath){

	if(!HasExtension(shaderPath, "cso")){
		return nullptr;
	}
	if(m_VertexShaders.count(shaderPath)){
		return m_VertexShaders[shaderPath].get();
	}
	std::shared_ptr<VertexShaderData> vertexShaderData = std::make_shared<VertexShaderData>();

	m_GraphicContext->CreateVertexShader(shaderPath.c_str(), &vertexShaderData->m_VertexShader, &vertexShaderData->m_VertexLayout);
	m_VertexShaders[shaderPath] = vertexShaderData;
	m_VertexShaders[shaderPath]->FilePath = shaderPath;

	return m_VertexShaders[shaderPath].get();
}

PixelShaderData* ShaderLoader::LoadPixelShader(const std::string& shaderPath){
	if(!HasExtension(shaderPath, "cso")){
		return nullptr;
	}
	if(m_PixelShaders.count(shaderPath)){
		return m_PixelShaders[shaderPath].get();
	}
	std::shared_ptr<PixelShaderData> pixelShaderData = std::make_shared<PixelShaderData>();

	m_GraphicContext->CreatePixelShader(shaderPath.c_str(), &pixelShaderData->m_PixelShader);
	m_PixelShaders[shaderPath] = pixelShaderData;
	m_PixelShaders[shaderPath]->FilePath = shaderPath;

	return m_PixelShaders[shaderPath].get();
}

VertexShaderData* ShaderLoader::GetVertexShader(const std::string& shaderPath){
	auto it = m_VertexShaders.find(shaderPath);
	if(it != m_VertexShaders.end()){
		return it->second.get();
	}
	return nullptr;
}

PixelShaderData* ShaderLoader::GetPixelShader(const std::string& shaderPath){
	auto it = m_PixelShaders.find(shaderPath);
	if(it != m_PixelShaders.end()){
		return it->second.get();
	}
	return nullptr;
}

void ShaderLoader::SetVertexShader(const std::string& shaderPath){
	if(!m_VertexShaders.count(shaderPath)){
		return;
	}
	VertexShaderData* shader = m_VertexShaders[shaderPath].get();

	m_GraphicContext->GetDeviceContext()->IASetInputLayout(shader->m_VertexLayout.Get());
	m_GraphicContext->GetDeviceContext()->VSSetShader(shader->m_VertexShader.Get(), NULL, 0);
}

void ShaderLoader::SetPixelShader(const std::string& shaderPath){
	if(!m_VertexShaders.count(shaderPath)){
		return;
	}
	PixelShaderData* shader = m_PixelShaders[shaderPath].get();
	m_GraphicContext->GetDeviceContext()->PSSetShader(shader->m_PixelShader.Get(), NULL, 0);
}
