#pragma once

#include "Game/MiniGameCollection/Core/MiniGameCore.h"
#include "Game/MiniGameCollection/Core/MiniGameMath.h"

#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace MiniGameCollection::Runtime {

struct RuntimeTransitionCommand {
    SceneToken sceneToken = 0;
    std::string sourceSceneName;
    std::string targetScenePath;
    TransitionRequest reason = TransitionRequest::None;
    float presentationWaitSeconds = 0.35f;
};

enum class RuntimePresentationCommandType : std::uint8_t {
    BeginScene,
    Countdown,
    Score,
    Hit,
    Success,
    NearMiss,
    Failure,
    Result,
    Cancel
};

struct RuntimePresentationCommand {
    RuntimePresentationCommandType type = RuntimePresentationCommandType::BeginScene;
    SceneToken sceneToken = 0;
    Vec2 position{};
    float intensity = 1.0f;
};

class MiniGameRuntimeMailbox {
public:
    using ShutdownCallback = std::function<void()>;

    static bool SubmitTransition(RuntimeTransitionCommand command) {
        std::scoped_lock lock(Mutex());
        if (command.sceneToken == 0 || command.sourceSceneName.empty() ||
            PendingTransition().has_value()) {
            return false;
        }
        PendingTransition() = std::move(command);
        return true;
    }

    static std::optional<RuntimeTransitionCommand> ConsumeTransition() {
        std::scoped_lock lock(Mutex());
        std::optional<RuntimeTransitionCommand> result;
        result.swap(PendingTransition());
        return result;
    }

    static void SubmitPresentation(RuntimePresentationCommand command) {
        if (command.sceneToken == 0) {
            return;
        }
        std::scoped_lock lock(Mutex());
        PresentationCommands().push_back(command);
    }

    static std::vector<RuntimePresentationCommand> ConsumePresentation() {
        std::scoped_lock lock(Mutex());
        std::vector<RuntimePresentationCommand> result;
        result.swap(PresentationCommands());
        return result;
    }

    static bool RegisterRulesShutdown(
        SceneToken sceneToken,
        ShutdownCallback callback
    ) {
        if (sceneToken == 0 || !callback) {
            return false;
        }
        std::scoped_lock lock(Mutex());
        ShutdownCallbacks()[sceneToken] = std::move(callback);
        return true;
    }

    static void UnregisterRulesShutdown(SceneToken sceneToken) {
        std::scoped_lock lock(Mutex());
        ShutdownCallbacks().erase(sceneToken);
    }

    static void InvokeRulesShutdown(SceneToken sceneToken) {
        ShutdownCallback callback;
        {
            std::scoped_lock lock(Mutex());
            auto found = ShutdownCallbacks().find(sceneToken);
            if (found != ShutdownCallbacks().end()) {
                callback = found->second;
            }
        }
        if (callback) {
            callback();
        }
    }

    static void ClearForScene(SceneToken sceneToken) {
        std::scoped_lock lock(Mutex());
        ShutdownCallbacks().erase(sceneToken);
        std::erase_if(
            PresentationCommands(),
            [sceneToken](const RuntimePresentationCommand& command) {
                return command.sceneToken == sceneToken;
            }
        );
        if (PendingTransition() &&
            PendingTransition()->sceneToken == sceneToken) {
            PendingTransition().reset();
        }
    }

private:
    static std::mutex& Mutex() {
        static std::mutex mutex;
        return mutex;
    }

    static std::optional<RuntimeTransitionCommand>& PendingTransition() {
        static std::optional<RuntimeTransitionCommand> command;
        return command;
    }

    static std::vector<RuntimePresentationCommand>& PresentationCommands() {
        static std::vector<RuntimePresentationCommand> commands;
        return commands;
    }

    static std::unordered_map<SceneToken, ShutdownCallback>& ShutdownCallbacks() {
        static std::unordered_map<SceneToken, ShutdownCallback> callbacks;
        return callbacks;
    }
};

} // namespace MiniGameCollection::Runtime
