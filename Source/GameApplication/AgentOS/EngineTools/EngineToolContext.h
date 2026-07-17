// =======================================================================
//
// EngineToolContext.h
//
// EngineTools（Scene Introspection / WriteTrace 等）がエンジンへアクセスする
// ための最小限の依存をまとめたコンテキスト。
// AgentOSService が所有し、EngineToolRegistry::RegisterEngineTools() へ渡す。
//
// =======================================================================
#pragma once

#include <string>

#include "Scene/sceneManager.h"
#include "Scene/scene.h"

class DebugLogService;

namespace agentos {

// ---------------------------------
// EngineToolContext
// EngineTools全体で共有するエンジン依存への参照。
// 生存期間はAgentOSServiceに従う（AgentOSServiceが所有する限り有効）。
// ---------------------------------
struct EngineToolContext {
	SceneManager* sceneManager = nullptr;
	DebugLogService* debugLog = nullptr;

	// アクティブSceneのSceneContextを解決する。
	// sceneNameが空なら最初に見つかったActive Sceneへフォールバックする
	// （GetActiveScenes()はstd::unordered_mapのため、複数Sceneがある場合の
	//  「最初」は非決定的である点に注意。複数Scene環境ではsceneNameを明示すること）。
	// 該当Sceneが無ければnullptrを返す。
	SceneContext* ResolveSceneContext(const std::string& sceneName = std::string()) const {
		if(!sceneManager) return nullptr;

		const auto& scenes = sceneManager->GetActiveScenes();
		if(scenes.empty()) return nullptr;

		if(!sceneName.empty()){
			auto iterator = scenes.find(sceneName);
			if(iterator == scenes.end() || !iterator->second) return nullptr;
			return iterator->second->GetSceneContext();
		}

		auto iterator = scenes.begin();
		return iterator->second ? iterator->second->GetSceneContext() : nullptr;
	}
};

} // namespace agentos
