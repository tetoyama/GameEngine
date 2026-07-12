#pragma once

#include "Game/MiniGameCollection/Core/MiniGameSceneTransition.h"
#include "Game/MiniGameCollection/Presentation/MiniGamePresentationService.h"

#include "Scene/scene.h"
#include "Scene/sceneManager.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace MiniGameCollection {

// SceneManagerへの直接アクセスはこのAdapterへ閉じ込める。
// Render / RHI / PhysX内部には依存しない。
class MiniGameSceneTransitionEngineBackend final
    : public IMiniGameSceneTransitionBackend {
public:
    using InputGate = std::function<void(bool)>;
    using RulesShutdown = std::function<void(SceneToken)>;

    MiniGameSceneTransitionEngineBackend(
        SceneManager& sceneManager,
        Presentation::MiniGamePresentationService& presentation,
        InputGate inputGate,
        RulesShutdown rulesShutdown
    )
        : m_sceneManager(sceneManager),
          m_presentation(presentation),
          m_inputGate(std::move(inputGate)),
          m_rulesShutdown(std::move(rulesShutdown)) {
    }

    void SetGameplayInputEnabled(bool enabled) override {
        if (m_inputGate) {
            m_inputGate(enabled);
        }
    }

    void CancelPresentation(SceneToken sceneToken) override {
        m_presentation.CancelAllForScene(sceneToken);
    }

    void ShutdownRules(SceneToken sceneToken) override {
        if (m_rulesShutdown) {
            m_rulesShutdown(sceneToken);
        }
    }

    bool RequestUnloadScene(const std::string& sceneName) override {
        const auto& scenes = m_sceneManager.GetActiveScenes();
        auto found = scenes.find(sceneName);
        if (found == scenes.end() || !found->second) {
            return true;
        }
        found->second->isDestroy = true;
        return true;
    }

    bool IsSceneUnloaded(const std::string& sceneName) const override {
        const auto& scenes = m_sceneManager.GetActiveScenes();
        auto found = scenes.find(sceneName);
        return found == scenes.end() || !found->second;
    }

    bool RequestLoadScene(const std::string& scenePath) override {
        if (scenePath.empty()) {
            return false;
        }
        std::shared_ptr<Scene> scene = m_sceneManager.LoadFromFilePath(scenePath);
        if (!scene) {
            return false;
        }
        m_lastRequestedPath = scenePath;
        m_lastLoadedSceneName = scene->SceneName;
        return true;
    }

    bool IsSceneLoaded(const std::string& scenePath) const override {
        for (const auto& [name, scene] : m_sceneManager.GetActiveScenes()) {
            (void)name;
            if (scene && scene->ScenePath == scenePath) {
                return true;
            }
        }
        return false;
    }

    const std::string& GetLastLoadedSceneName() const noexcept {
        return m_lastLoadedSceneName;
    }

private:
    SceneManager& m_sceneManager;
    Presentation::MiniGamePresentationService& m_presentation;
    InputGate m_inputGate;
    RulesShutdown m_rulesShutdown;
    std::string m_lastRequestedPath;
    std::string m_lastLoadedSceneName;
};

} // namespace MiniGameCollection
