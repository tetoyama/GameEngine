#pragma once

#include "Game/MiniGameCollection/Core/WorldEventTelegraphModel.h"
#include "Game/MiniGameCollection/Runtime/MiniGameRuntimeScriptBase.h"
#include "Game/MiniGameCollection/Runtime/WorldEventTelegraphPresenter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace MiniGameCollection::Runtime {

// ワールド位置を持つTelegraphは、画面上へ疑似投影した2D十字ではなく
// 実際の床面上に薄い3D Geometryとして表示する。
// Screen shapeだけはHUD Presenterへ残す。
class WorldEventTelegraphRuntimeSupport
    : public MiniGameRuntimeScriptBase {
protected:
    explicit WorldEventTelegraphRuntimeSupport(std::string scriptName)
        : MiniGameRuntimeScriptBase(std::move(scriptName)) {
    }

    void QueueWorldTelegraphVisuals(std::string_view namePrefix) {
        if (m_worldVisualsQueued) {
            return;
        }
        m_worldVisualsQueued = true;

        for (std::size_t slotIndex = 0; slotIndex < WorldSlotCount; ++slotIndex) {
            for (std::size_t cellIndex = 0; cellIndex < CellCount; ++cellIndex) {
                QueueCube(
                    std::string(namePrefix) + "_GroundCell_" +
                        std::to_string(slotIndex) + "_" + std::to_string(cellIndex),
                    HiddenPosition(),
                    {},
                    {0.16f, 0.04f, 0.02f, 1.0f},
                    [this, slotIndex, cellIndex](const CubeVisualRefs& refs) {
                        m_worldSlots[slotIndex].cells[cellIndex] = refs.transform;
                        m_worldSlots[slotIndex].cellMaterials[cellIndex] = refs.material;
                        Hide(refs.transform);
                    }
                );
            }

            for (std::size_t borderIndex = 0;
                 borderIndex < BorderCount;
                 ++borderIndex) {
                QueueCube(
                    std::string(namePrefix) + "_GroundBorder_" +
                        std::to_string(slotIndex) + "_" +
                        std::to_string(borderIndex),
                    HiddenPosition(),
                    {},
                    {0.3f, 0.06f, 0.02f, 1.0f},
                    [this, slotIndex, borderIndex](const CubeVisualRefs& refs) {
                        m_worldSlots[slotIndex].borders[borderIndex] = refs.transform;
                        m_worldSlots[slotIndex].borderMaterials[borderIndex] =
                            refs.material;
                        Hide(refs.transform);
                    }
                );
            }

            QueueCube(
                std::string(namePrefix) + "_Beacon_" + std::to_string(slotIndex),
                HiddenPosition(),
                {},
                {0.3f, 0.06f, 0.02f, 1.0f},
                [this, slotIndex](const CubeVisualRefs& refs) {
                    m_worldSlots[slotIndex].beacon = refs.transform;
                    m_worldSlots[slotIndex].beaconMaterial = refs.material;
                    Hide(refs.transform);
                }
            );
        }
    }

    void SyncWorldTelegraphVisuals(const WorldEventTelegraphModel& model) {
        for (WorldVisualSlot& slot : m_worldSlots) {
            HideSlot(slot);
        }

        const std::vector<TelegraphSnapshot> snapshots =
            model.GetVisibleSnapshots();
        std::size_t slotIndex = 0;
        for (const TelegraphSnapshot& snapshot : snapshots) {
            if (snapshot.definition.shape == TelegraphShape::Screen) {
                continue;
            }
            if (slotIndex >= m_worldSlots.size()) {
                break;
            }
            ApplySnapshot(m_worldSlots[slotIndex++], snapshot);
        }
    }

    void DrawTelegraphHud(
        SceneContext* context,
        const WorldEventTelegraphModel& model
    ) const {
        WorldEventTelegraphPresenter::DrawHud(context, model);
    }

    void ClearWorldTelegraphVisuals() {
        for (WorldVisualSlot& slot : m_worldSlots) {
            HideSlot(slot);
            slot = {};
        }
        m_worldVisualsQueued = false;
    }

private:
    static constexpr std::size_t WorldSlotCount = 3;
    static constexpr std::size_t CellCount = 9;
    static constexpr std::size_t BorderCount = 4;
    static constexpr float GroundY = 0.13f;
    static constexpr float BombTileSpacing = 1.1f;

    struct WorldVisualSlot {
        std::array<ComponentRef<TransformComponent>, CellCount> cells;
        std::array<ComponentRef<MaterialComponent>, CellCount> cellMaterials;
        std::array<ComponentRef<TransformComponent>, BorderCount> borders;
        std::array<ComponentRef<MaterialComponent>, BorderCount> borderMaterials;
        ComponentRef<TransformComponent> beacon;
        ComponentRef<MaterialComponent> beaconMaterial;
    };

    static Vector3 HiddenPosition() noexcept {
        return {0.0f, -1000.0f, 0.0f};
    }

    static void Hide(const ComponentRef<TransformComponent>& ref) {
        if (TransformComponent* transform = ref.TryGet()) {
            transform->position = HiddenPosition();
            transform->scale = {};
        }
    }

    static void HideSlot(WorldVisualSlot& slot) {
        for (const ComponentRef<TransformComponent>& cell : slot.cells) {
            Hide(cell);
        }
        for (const ComponentRef<TransformComponent>& border : slot.borders) {
            Hide(border);
        }
        Hide(slot.beacon);
    }

    static bool LabelContains(
        const TelegraphSnapshot& snapshot,
        std::string_view token
    ) {
        return snapshot.definition.label.find(token) != std::string::npos;
    }

    static DirectX::XMFLOAT4 ResolveColor(
        const TelegraphSnapshot& snapshot,
        float urgency
    ) noexcept {
        if (LabelContains(snapshot, "BOMB")) {
            return {
                1.0f,
                0.42f - urgency * 0.24f,
                0.035f,
                1.0f
            };
        }
        if (LabelContains(snapshot, "STAR") ||
            LabelContains(snapshot, "GOLDEN")) {
            return {1.0f, 0.74f + urgency * 0.18f, 0.08f, 1.0f};
        }
        if (LabelContains(snapshot, "SHEEP")) {
            return {0.24f, 0.82f, 1.0f, 1.0f};
        }

        switch (snapshot.phase) {
        case TelegraphPhase::Resolving:
            return {1.0f, 0.12f, 0.04f, 1.0f};
        case TelegraphPhase::Aftermath:
            return {0.35f, 0.94f, 0.56f, 1.0f};
        default:
            return {1.0f, 0.78f, 0.12f, 1.0f};
        }
    }

    static void ApplyMaterial(
        const ComponentRef<MaterialComponent>& ref,
        const DirectX::XMFLOAT4& color,
        float emissiveIntensity
    ) {
        if (MaterialComponent* material = ref.TryGet()) {
            material->Material.BaseColor = {
                color.x * 0.28f,
                color.y * 0.28f,
                color.z * 0.28f,
                1.0f
            };
            material->Material.Metallic = 0.0f;
            material->Material.Roughness = 0.48f;
            material->Material.EmissiveColor = {
                color.x,
                color.y,
                color.z
            };
            material->Material.EmissiveIntensity = emissiveIntensity;
        }
    }

    static void SetPart(
        const ComponentRef<TransformComponent>& transformRef,
        const ComponentRef<MaterialComponent>& materialRef,
        Vector3 position,
        Vector3 scale,
        const DirectX::XMFLOAT4& color,
        float emissiveIntensity
    ) {
        if (TransformComponent* transform = transformRef.TryGet()) {
            transform->position = position;
            transform->scale = scale;
            transform->SetRotationEuler(Vector3(0.0f, 0.0f, 0.0f));
        }
        ApplyMaterial(materialRef, color, emissiveIntensity);
    }

    static void ApplySnapshot(
        WorldVisualSlot& slot,
        const TelegraphSnapshot& snapshot
    ) {
        const float urgency = (std::clamp)(
            snapshot.phaseProgress,
            0.0f,
            1.0f
        );
        const float pulse = 0.5f + 0.5f * std::sin(
            urgency * 18.8495559215f
        );
        const DirectX::XMFLOAT4 color = ResolveColor(snapshot, urgency);
        const float intensity =
            snapshot.phase == TelegraphPhase::Resolving
                ? 8.0f
                : snapshot.phase == TelegraphPhase::Aftermath
                    ? 2.0f
                    : 3.2f + urgency * 2.8f + pulse * 1.8f;
        const float centerX = snapshot.definition.worldPosition.x;
        const float centerZ = snapshot.definition.worldPosition.y;

        if (snapshot.definition.shape == TelegraphShape::Area) {
            const float padScale = 0.88f + pulse * 0.08f;
            std::size_t cellIndex = 0;
            for (int z = -1; z <= 1; ++z) {
                for (int x = -1; x <= 1; ++x) {
                    SetPart(
                        slot.cells[cellIndex],
                        slot.cellMaterials[cellIndex],
                        {
                            centerX + static_cast<float>(x) * BombTileSpacing,
                            GroundY,
                            centerZ + static_cast<float>(z) * BombTileSpacing
                        },
                        {padScale, 0.035f, padScale},
                        color,
                        intensity
                    );
                    ++cellIndex;
                }
            }
        } else {
            const float centerScale =
                snapshot.definition.shape == TelegraphShape::Point
                    ? 0.38f + pulse * 0.16f
                    : 0.58f + pulse * 0.18f;
            SetPart(
                slot.cells[4],
                slot.cellMaterials[4],
                {centerX, GroundY, centerZ},
                {centerScale, 0.045f, centerScale},
                color,
                intensity
            );
        }

        const float radius = (std::max)(
            0.65f,
            snapshot.definition.radius
        );
        const float lineThickness =
            snapshot.definition.shape == TelegraphShape::Area
                ? 0.13f
                : 0.09f + pulse * 0.035f;
        const float lineLength = radius * 2.0f + lineThickness;

        SetPart(
            slot.borders[0],
            slot.borderMaterials[0],
            {centerX, GroundY + 0.025f, centerZ - radius},
            {lineLength, 0.07f, lineThickness},
            color,
            intensity + 1.0f
        );
        SetPart(
            slot.borders[1],
            slot.borderMaterials[1],
            {centerX, GroundY + 0.025f, centerZ + radius},
            {lineLength, 0.07f, lineThickness},
            color,
            intensity + 1.0f
        );
        SetPart(
            slot.borders[2],
            slot.borderMaterials[2],
            {centerX - radius, GroundY + 0.025f, centerZ},
            {lineThickness, 0.07f, lineLength},
            color,
            intensity + 1.0f
        );
        SetPart(
            slot.borders[3],
            slot.borderMaterials[3],
            {centerX + radius, GroundY + 0.025f, centerZ},
            {lineThickness, 0.07f, lineLength},
            color,
            intensity + 1.0f
        );

        const float beaconHeight =
            snapshot.definition.shape == TelegraphShape::Area
                ? 0.55f + pulse * 0.42f
                : 1.05f + pulse * 0.9f;
        SetPart(
            slot.beacon,
            slot.beaconMaterial,
            {
                centerX,
                GroundY + beaconHeight * 0.5f,
                centerZ
            },
            {
                0.055f + pulse * 0.035f,
                beaconHeight,
                0.055f + pulse * 0.035f
            },
            color,
            intensity + 2.0f
        );
    }

    std::array<WorldVisualSlot, WorldSlotCount> m_worldSlots;
    bool m_worldVisualsQueued = false;
};

} // namespace MiniGameCollection::Runtime
