// effectLoader.h
#pragma once
#include "ResourceLoader.h"

#include <memory>
#include <string>
#include "Engine/Resources/Data/effectData.h"
#include "Engine/Graphics/graphicsContext.h"

#include <uchar.h>

inline std::shared_ptr<EffectData> LoadEffectFromFile(const std::string& filePath, GraphicsContext* context) {

	std::shared_ptr<EffectData> efc = std::make_shared<EffectData>();
	efc->FilePath = filePath;
	efc->effect = Effekseer::Effect::Create(context->GetEffectManager(), (const char16_t*)filePath.c_str());
	if(efc->effect == nullptr){
		OutputDebugStringA(("Failed to load effect: " + filePath + "\n").c_str());
		return nullptr;
	}
	return efc;
}

template<>
inline void ResourceLoader<EffectData>::SetupLoadFunc(void* contextPtr) {
	OutputDebugStringA("SetupLoadFunc TextureData called\n");

	auto context = static_cast<GraphicsContext*>(contextPtr);
	SetLoadFunction([=](const std::string& path, void*) {
		return LoadEffectFromFile(path, context);
	});
}
