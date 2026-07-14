#pragma once

#include "Game/MiniGameCollection/Core/MiniGameCore.h"

#include <cstddef>
#include <mutex>
#include <unordered_map>

namespace MiniGameCollection::Runtime {

class MiniGameRuntimePauseRegistry final {
public:
    static void Pause(SceneToken sceneToken) {
        if (sceneToken == 0) {
            return;
        }
        std::scoped_lock lock(Mutex());
        ++PauseCounts()[sceneToken];
    }

    static void Resume(SceneToken sceneToken) {
        if (sceneToken == 0) {
            return;
        }
        std::scoped_lock lock(Mutex());
        auto found = PauseCounts().find(sceneToken);
        if (found == PauseCounts().end()) {
            return;
        }
        if (found->second <= 1) {
            PauseCounts().erase(found);
        } else {
            --found->second;
        }
    }

    static bool IsPaused(SceneToken sceneToken) {
        if (sceneToken == 0) {
            return false;
        }
        std::scoped_lock lock(Mutex());
        const auto found = PauseCounts().find(sceneToken);
        return found != PauseCounts().end() && found->second > 0;
    }

    static void ClearForScene(SceneToken sceneToken) {
        std::scoped_lock lock(Mutex());
        PauseCounts().erase(sceneToken);
    }

    static void Clear() {
        std::scoped_lock lock(Mutex());
        PauseCounts().clear();
    }

private:
    static std::mutex& Mutex() {
        static std::mutex mutex;
        return mutex;
    }

    static std::unordered_map<SceneToken, std::size_t>& PauseCounts() {
        static std::unordered_map<SceneToken, std::size_t> counts;
        return counts;
    }
};

} // namespace MiniGameCollection::Runtime
