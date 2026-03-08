// =======================================================================
// 
// resourceService.cpp
// 
// =======================================================================
#include "resourceService.h"

#include <Windows.h>

#include "Data/textureData.h"
#include "Data/modelData.h"
#include "Data/shaderData.h"
#include "Data/audioData.h"
#include "Data/effectData.h"
#include "Data/llamaModelData.h"
#include "Data/prefabData.h"

#include "Loader/textureLoader.h"
#include "Loader/shaderLoader.h"
#include "Loader/modelLoader.h"
#include "Loader/audioLoader.h"
#include "Loader/effectLoader.h"
#include "Loader/llamaModelLoader.h"
#include "Loader/prefabLoader.h"

void ResourceService::Initialize(GraphicsContext* graphics, AudioContext* audio, DebugLogService* debugLog){
	m_Graphics = graphics;
	m_Audio = audio;
	m_DebugLog = debugLog;

	if(m_DebugLog){
		m_DebugLog->LOG_INFO("ResourceService の初期化を開始します");
	}

	RegisterLoader<TextureData>();
	RegisterLoader<ModelData>();
	RegisterLoader<VertexShaderData>();
	RegisterLoader<PixelShaderData>();
	RegisterLoader<AudioData>();
	RegisterLoader<EffectData>();
	RegisterLoader<LLAMAModelData>();
	RegisterLoader<PrefabData>();

	if(m_DebugLog){
		m_DebugLog->LOG_INFO("ResourceService の初期化が完了しました");
	}
}

void ResourceService::Shutdown(){
	if(m_DebugLog){
		m_DebugLog->LOG_INFO("ResourceService を終了します");
	}

	ClearAllUnused();

	for(auto& [type, loader] : m_Loaders){
		loader->DumpCacheState();
	}

	m_Loaders.clear();
	m_Graphics = nullptr;
	m_Audio = nullptr;
	m_DebugLog = nullptr;
}
