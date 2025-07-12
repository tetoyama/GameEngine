#pragma once
#include <memory>
#include <string>
#include "Engine/Resources/Data/vertexShaderData.h"
#include "Engine/Resources/Data/pixelShaderData.h"
#include "Engine/Graphics/graphicsContext.h"
#include "Backends/checkFileExtention.h"

#include "ResourceLoader.h"

inline std::shared_ptr<VertexShaderData> LoadVertexShaderFromFile(const std::string& path, GraphicsContext* context) {
	if (!HasExtension(path, "cso")) return nullptr;

	auto shader = std::make_shared<VertexShaderData>();
	context->CreateVertexShader(path.c_str(), &shader->m_VertexShader, &shader->m_VertexLayout);
	shader->FilePath = path;
	return shader;
}


inline std::shared_ptr<PixelShaderData> LoadPixelShaderFromFile(const std::string& path, GraphicsContext* context) {
	if (!HasExtension(path, "cso")) return nullptr;

	auto shader = std::make_shared<PixelShaderData>();
	context->CreatePixelShader(path.c_str(), &shader->m_PixelShader);
	shader->FilePath = path;
	return shader;
}

template<>
inline void ResourceLoader<VertexShaderData>::SetupLoadFunc(void* contextPtr) {
	auto context = static_cast<GraphicsContext*>(contextPtr);
	SetLoadFunction([=](const std::string& path, void*) {
		return LoadVertexShaderFromFile(path, context);
	});
}

template<>
inline void ResourceLoader<PixelShaderData>::SetupLoadFunc(void* contextPtr) {
	auto context = static_cast<GraphicsContext*>(contextPtr);
	SetLoadFunction([=](const std::string& path, void*) {
		return LoadPixelShaderFromFile(path, context);
	});
}