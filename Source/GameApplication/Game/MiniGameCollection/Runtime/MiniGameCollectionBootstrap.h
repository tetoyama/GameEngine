#pragma once

#include "Scene/sceneManager.h"

#include <memory>

namespace MiniGameCollection::Runtime {

class MiniGameCollectionBootstrap {
public:
    static constexpr const char* PersistentScenePath =
        "Asset/Game/MiniGameCollection/Scene/Persistent/MiniGamePersistent.scene";

    static bool Load(SceneManager& sceneManager) {
        for (const auto& [name, scene] : sceneManager.GetActiveScenes()) {
            (void)name;
            if (scene && scene->ScenePath == PersistentScenePath) {
                return true;
            }
        }
        return static_cast<bool>(
            sceneManager.LoadFromFilePath(PersistentScenePath)
        );
    }
};

} // namespace MiniGameCollection::Runtime
