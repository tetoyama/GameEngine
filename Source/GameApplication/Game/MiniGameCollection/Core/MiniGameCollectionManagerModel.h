#pragma once

#include "Game/MiniGameCollection/Core/MiniGameCore.h"

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace MiniGameCollection {

struct MiniGameDescriptor {
    MiniGameId gameId = MiniGameId::PresentationSpike;
    std::string displayName;
    std::string ruleText;
    std::string controlText;
    std::string scenePath;
};

class MiniGameCollectionManagerModel {
public:
    explicit MiniGameCollectionManagerModel(
        std::vector<MiniGameDescriptor> games = DefaultGames()
    )
        : m_games(std::move(games)) {
        if (m_games.empty()) {
            throw std::invalid_argument("Mini-game collection requires at least one game");
        }
        ValidateDescriptors();
    }

    static std::vector<MiniGameDescriptor> DefaultGames() {
        return {
            {
                .gameId = MiniGameId::ColorTerritory,
                .displayName = "Color Territory",
                .ruleText = "床を自分の色に塗れ！",
                .controlText = "操作：移動",
                .scenePath = "Asset/Game/MiniGameCollection/Scene/ColorTerritory/ColorTerritory.scene"
            },
            {
                .gameId = MiniGameId::SheepRoundup,
                .displayName = "Sheep Roundup",
                .ruleText = "羊を自分の囲いへ入れろ！",
                .controlText = "操作：移動",
                .scenePath = "Asset/Game/MiniGameCollection/Scene/SheepRoundup/SheepRoundup.scene"
            },
            {
                .gameId = MiniGameId::Backshot,
                .displayName = "Backshot",
                .ruleText = "相手の背中を撃て！",
                .controlText = "操作：移動＋射撃",
                .scenePath = "Asset/Game/MiniGameCollection/Scene/Backshot/Backshot.scene"
            }
        };
    }

    bool SelectIndex(std::size_t index) noexcept {
        if (index >= m_games.size() || m_sessionActive) {
            return false;
        }
        m_selectedIndex = index;
        return true;
    }

    bool SelectGame(MiniGameId gameId) noexcept {
        if (m_sessionActive) {
            return false;
        }
        for (std::size_t index = 0; index < m_games.size(); ++index) {
            if (m_games[index].gameId == gameId) {
                m_selectedIndex = index;
                return true;
            }
        }
        return false;
    }

    const MiniGameDescriptor& GetSelectedGame() const {
        return m_games.at(m_selectedIndex);
    }

    const MiniGameDescriptor& GetGame(std::size_t index) const {
        return m_games.at(index);
    }

    std::size_t GetGameCount() const noexcept { return m_games.size(); }
    std::size_t GetSelectedIndex() const noexcept { return m_selectedIndex; }

    bool BeginSelectedGame(SceneToken sceneToken) {
        if (m_sessionActive || sceneToken == 0) {
            return false;
        }
        m_activeSceneToken = sceneToken;
        m_sessionActive = true;
        m_lastTransitionRequest = TransitionRequest::None;
        return true;
    }

    bool FinishSession(TransitionRequest request) noexcept {
        if (!m_sessionActive || request == TransitionRequest::None) {
            return false;
        }
        m_lastTransitionRequest = request;
        m_sessionActive = false;
        m_activeSceneToken = 0;

        if (request == TransitionRequest::NextGame) {
            m_selectedIndex = (m_selectedIndex + 1) % m_games.size();
        }
        return true;
    }

    bool IsSessionActive() const noexcept { return m_sessionActive; }
    SceneToken GetActiveSceneToken() const noexcept { return m_activeSceneToken; }
    TransitionRequest GetLastTransitionRequest() const noexcept {
        return m_lastTransitionRequest;
    }

private:
    void ValidateDescriptors() const {
        for (std::size_t lhs = 0; lhs < m_games.size(); ++lhs) {
            const MiniGameDescriptor& descriptor = m_games[lhs];
            if (descriptor.displayName.empty() ||
                descriptor.ruleText.empty() ||
                descriptor.controlText.empty() ||
                descriptor.scenePath.empty()) {
                throw std::invalid_argument("Mini-game descriptor is incomplete");
            }
            for (std::size_t rhs = lhs + 1; rhs < m_games.size(); ++rhs) {
                if (descriptor.gameId == m_games[rhs].gameId) {
                    throw std::invalid_argument("Mini-game ids must be unique");
                }
            }
        }
    }

    std::vector<MiniGameDescriptor> m_games;
    std::size_t m_selectedIndex = 0;
    SceneToken m_activeSceneToken = 0;
    TransitionRequest m_lastTransitionRequest = TransitionRequest::None;
    bool m_sessionActive = false;
};

} // namespace MiniGameCollection
