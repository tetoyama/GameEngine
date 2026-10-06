#include "resourceService.h"

void ResourceService::InitializeNative(DebugLogService* debugLog) {
    Shutdown();
    m_DebugLog=debugLog;
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
	if(m_DebugLog){
		m_DebugLog->LOG_INFO("ResourceService の終了処理が完了しました");
	}
	m_DebugLog = nullptr;
}
