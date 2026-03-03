// =======================================================================
// 
// effectLoader.h
// 
// =======================================================================
#pragma once
#include "ResourceLoader.h"

#include <memory>
#include <string>
#include "Resources/Data/effectData.h"
#include "Graphics/graphicsContext.h"

#include <uchar.h>

inline std::shared_ptr<EffectData> LoadEffectFromFile(const std::string& filePath, GraphicsContext* context) {

	std::shared_ptr<EffectData> efc = std::make_shared<EffectData>();
	efc->FilePath = filePath;
	efc->effect = Effekseer::Effect::Create(context->GetEffectManager(), std::u16string(filePath.begin(), filePath.end()).c_str());
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
	SetLoadFunction([=](const std::string& path, std::shared_ptr<void> /*args*/) {
		return LoadEffectFromFile(path, context);
	});
}
