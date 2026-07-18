#pragma once

#include "Game/MiniGameCollection/Core/MiniGameBriefingModel.h"
#include "Game/MiniGameCollection/Core/MiniGameCore.h"
#include "Game/MiniGameCollection/Core/MiniGameMath.h"

#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
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
            PendingTransition().has_value() ||
            GuidanceScenes().contains(command.sceneToken) ||
            PracticeScenes().contains(command.sceneToken)) {
            return false;
        }
        PendingTransition() = std::move(command);
        return true;
    }

    // Practiceのラウンド再生成だけが使用する内部遷移。
    // 通常RuntimeのResult入力はPracticeScenesで遮断したままにする。
    static bool SubmitPracticeTransition(RuntimeTransitionCommand command) {
        std::scoped_lock lock(Mutex());
        if (command.sceneToken == 0 || command.sourceSceneName.empty() ||
            PendingTransition().has_value() ||
            !PracticeScenes().contains(command.sceneToken)) {
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

    static void BeginGuidance(SceneToken sceneToken) {
        if (sceneToken == 0) {
            return;
        }
        std::scoped_lock lock(Mutex());
        GuidanceScenes().insert(sceneToken);
        LastLowScorePresentationTimes().erase(sceneToken);
        std::erase_if(
            PresentationCommands(),
            [sceneToken](const RuntimePresentationCommand& command) {
                return command.sceneToken == sceneToken &&
                    command.type != RuntimePresentationCommandType::BeginScene &&
                    command.type != RuntimePresentationCommandType::Countdown;
            }
        );
    }

    static void EndGuidance(SceneToken sceneToken) {
        std::scoped_lock lock(Mutex());
        GuidanceScenes().erase(sceneToken);
    }

    static bool IsGuidanceActive(SceneToken sceneToken) {
        std::scoped_lock lock(Mutex());
        return GuidanceScenes().contains(sceneToken);
    }

    static void BeginPractice(SceneToken sceneToken) {
        if (sceneToken == 0) {
            return;
        }

        std::scoped_lock lock(Mutex());
        PracticeScenes().insert(sceneToken);
        PracticeRoundFinishedScenes().erase(sceneToken);
        LastLowScorePresentationTimes().erase(sceneToken);

        // Practice開始前に残っていた公式Result系だけを破棄する。
        // Score/Hit/NearMissは練習の手触りとして通常どおり再生する。
        std::erase_if(
            PresentationCommands(),
            [sceneToken](const RuntimePresentationCommand& command) {
                if (command.sceneToken != sceneToken) {
                    return false;
                }
                return command.type == RuntimePresentationCommandType::Result ||
                    command.type == RuntimePresentationCommandType::Success ||
                    command.type == RuntimePresentationCommandType::Failure;
            }
        );
    }

    static void EndPractice(SceneToken sceneToken) {
        std::scoped_lock lock(Mutex());
        PracticeScenes().erase(sceneToken);
        PracticeRoundFinishedScenes().erase(sceneToken);
    }

    static bool IsPracticeActive(SceneToken sceneToken) {
        std::scoped_lock lock(Mutex());
        return PracticeScenes().contains(sceneToken);
    }

    static bool ConsumePracticeRoundFinished(SceneToken sceneToken) {
        std::scoped_lock lock(Mutex());
        return PracticeRoundFinishedScenes().erase(sceneToken) > 0;
    }

    static void ArmBriefingBypass(MiniGameId gameId) {
        std::scoped_lock lock(Mutex());
        BriefingBypassGames().insert(static_cast<std::uint8_t>(gameId));
    }

    static bool ConsumeBriefingBypass(MiniGameId gameId) {
        std::scoped_lock lock(Mutex());
        return BriefingBypassGames().erase(
            static_cast<std::uint8_t>(gameId)
        ) > 0;
    }

    static BriefingMode ResolveBriefingMode(MiniGameId gameId) {
        std::scoped_lock lock(Mutex());
        return BriefingProgress().ResolveMode(gameId, false);
    }

    static bool HasCompletedBriefing(MiniGameId gameId) {
        std::scoped_lock lock(Mutex());
        return BriefingProgress().HasCompleted(gameId);
    }

    static void MarkBriefingCompleted(MiniGameId gameId) {
        std::scoped_lock lock(Mutex());
        BriefingProgress().MarkCompleted(gameId);
    }

    static void ResetBriefingProgress() {
        std::scoped_lock lock(Mutex());
        BriefingProgress().Reset();
        BriefingBypassGames().clear();
    }

    static void SetMajorTelegraphActive(
        SceneToken sceneToken,
        bool active
    ) {
        if (sceneToken == 0) {
            return;
        }
        std::scoped_lock lock(Mutex());
        if (active) {
            MajorTelegraphScenes().insert(sceneToken);
        } else {
            MajorTelegraphScenes().erase(sceneToken);
        }
    }

    static bool IsMajorTelegraphActive(SceneToken sceneToken) {
        std::scoped_lock lock(Mutex());
        return MajorTelegraphScenes().contains(sceneToken);
    }

    static void SubmitPresentation(RuntimePresentationCommand command) {
        if (command.sceneToken == 0) {
            return;
        }

        std::scoped_lock lock(Mutex());

        if (PracticeScenes().contains(command.sceneToken)) {
            if (command.type == RuntimePresentationCommandType::Result) {
                // Result画面へ入る代わりにPractice Overlayへラウンド終了を通知する。
                PracticeRoundFinishedScenes().insert(command.sceneToken);
                return;
            }
            if (command.type == RuntimePresentationCommandType::Success ||
                command.type == RuntimePresentationCommandType::Failure) {
                return;
            }
        }

        if (GuidanceScenes().contains(command.sceneToken) &&
            command.type != RuntimePresentationCommandType::BeginScene &&
            command.type != RuntimePresentationCommandType::Countdown &&
            command.type != RuntimePresentationCommandType::Cancel) {
            return;
        }

        if (MajorTelegraphScenes().contains(command.sceneToken) &&
            (command.type == RuntimePresentationCommandType::Score ||
             command.type == RuntimePresentationCommandType::NearMiss)) {
            return;
        }

        if (command.type == RuntimePresentationCommandType::BeginScene) {
            LastLowScorePresentationTimes().erase(command.sceneToken);
        }

        // 通常の1マス取得は最大4人から高頻度で届く。
        // すべてをカメラ揺れ・発光Effect・HUD Burstへ変換すると画面全体が点滅するため、
        // 低強度ScoreだけScene単位で間引く。首位変動、爆弾、スター等の強演出は通す。
        if (command.type == RuntimePresentationCommandType::Score &&
            command.intensity < LowScorePresentationIntensityThreshold) {
            const auto now = PresentationClock::now();
            auto& lastTimes = LastLowScorePresentationTimes();
            const auto found = lastTimes.find(command.sceneToken);
            if (found != lastTimes.end() &&
                now - found->second < LowScorePresentationInterval) {
                auto& commands = PresentationCommands();
                for (auto it = commands.rbegin(); it != commands.rend(); ++it) {
                    if (it->sceneToken == command.sceneToken &&
                        it->type == RuntimePresentationCommandType::Score &&
                        it->intensity < LowScorePresentationIntensityThreshold) {
                        if (command.intensity >= it->intensity) {
                            *it = command;
                        }
                        return;
                    }
                }
                return;
            }
            lastTimes[command.sceneToken] = now;
        }

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
        LastLowScorePresentationTimes().erase(sceneToken);
        GuidanceScenes().erase(sceneToken);
        PracticeScenes().erase(sceneToken);
        PracticeRoundFinishedScenes().erase(sceneToken);
        MajorTelegraphScenes().erase(sceneToken);
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
    using PresentationClock = std::chrono::steady_clock;
    static constexpr float LowScorePresentationIntensityThreshold = 1.0f;
    static constexpr auto LowScorePresentationInterval =
        std::chrono::milliseconds(160);

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

    static std::unordered_map<SceneToken, PresentationClock::time_point>&
    LastLowScorePresentationTimes() {
        static std::unordered_map<SceneToken, PresentationClock::time_point> times;
        return times;
    }

    static std::unordered_map<SceneToken, ShutdownCallback>& ShutdownCallbacks() {
        static std::unordered_map<SceneToken, ShutdownCallback> callbacks;
        return callbacks;
    }

    static std::unordered_set<SceneToken>& GuidanceScenes() {
        static std::unordered_set<SceneToken> scenes;
        return scenes;
    }

    static std::unordered_set<SceneToken>& PracticeScenes() {
        static std::unordered_set<SceneToken> scenes;
        return scenes;
    }

    static std::unordered_set<SceneToken>& PracticeRoundFinishedScenes() {
        static std::unordered_set<SceneToken> scenes;
        return scenes;
    }

    static std::unordered_set<std::uint8_t>& BriefingBypassGames() {
        static std::unordered_set<std::uint8_t> games;
        return games;
    }

    static std::unordered_set<SceneToken>& MajorTelegraphScenes() {
        static std::unordered_set<SceneToken> scenes;
        return scenes;
    }

    static MiniGameBriefingSessionState& BriefingProgress() {
        static MiniGameBriefingSessionState state;
        return state;
    }
};

} // namespace MiniGameCollection::Runtime
