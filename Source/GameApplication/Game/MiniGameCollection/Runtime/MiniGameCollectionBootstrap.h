#pragma once

#include "Scene/sceneManager.h"

#include <memory>

namespace MiniGameCollection::Runtime {

class MiniGameCollectionBootstrap {
public:
    static constexpr const char* PersistentScenePath =
        "Asset/Game/MiniGameCollection/Scene/Persistent/MiniGamePersistent.scene";

    static bool Load(SceneManager& sceneManager) {
        bool loaded = false;
        for (const auto& [name, scene] : sceneManager.GetActiveScenes()) {
            (void)name;
            if (scene && scene->ScenePath == PersistentScenePath) {
                loaded = true;
                break;
            }
        }

        if (!loaded) {
            loaded = static_cast<bool>(
                sceneManager.LoadFromFilePath(PersistentScenePath)
            );
        }
        if (!loaded) {
            return false;
        }

        // Engineの既定状態はStopped。ゲーム専用Bootstrapでは、最初のUpdateで
        // StartAllとCustomScriptのOnStartが実行されるようPlayingへ遷移させる。
        sceneManager.State = SceneManagerState::Playing;
        return true;
    }
};

} // namespace MiniGameCollection::Runtime
