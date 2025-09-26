#pragma once
#include "Interface/IComponent.h"
#include "Service/YAMLConverters.h"
#include "Backends/myVector2.h"
#include "meshRendererComponent.h"


class TerrainComponent : public IComponent {
public:
	int Scale = 10;

	int CurrentScale = -1;
	std::vector<float> HeightMap;

	MeshRendererComponent* meshRenderer = nullptr;

	~TerrainComponent() {
		if (meshRenderer) {
			delete meshRenderer;
			meshRenderer = nullptr;
		}
	}

	YAML::Node encode() override {
		YAML::Node node;
		node["Scale"] = Scale;

		node["HeightMap"] = HeightMap;

		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override {
		if (node["Scale"])
			Scale = node["Scale"].as<int>();

		if (node["HeightMap"]) {
			HeightMap = node["HeightMap"].as<std::vector<float>>();
		}

		return true;
	}

    void inspector(SceneContext* context) override {
        ImGui::Text("Terrain Component");
        ImGui::DragInt("Scale", &Scale, 1, 1, 1000);

        if (HeightMap.size() != (Scale + 1)* (Scale + 1)) {
            HeightMap.resize((Scale + 1) * (Scale + 1), 0.0f);
        }

        int mapSize = (int)std::sqrt(HeightMap.size());
        static int brushRadius = 2;
        static float brushStrength = 0.01f;
        static int brushMode = 0; // 0=Raise, 1=Lower, 2=Smooth



        ImGui::Separator();
        ImGui::Text("Brush Settings");
        ImGui::SliderInt("Brush Radius", &brushRadius, 1, 100);
        ImGui::SliderFloat("Brush Strength", &brushStrength, 0.01f, 1.0f);
        ImGui::RadioButton("Raise", &brushMode, 0); ImGui::SameLine();
        ImGui::RadioButton("Lower", &brushMode, 1); ImGui::SameLine();
        ImGui::RadioButton("Smooth", &brushMode, 2);

        ImGui::Separator();
        ImGui::Text("HeightMap Editor");

		float availWidth = ImGui::GetContentRegionAvail().x * 0.5f;

        const float cellSize = availWidth / mapSize; // 1マスのサイズ
        const ImVec2 canvasSize(availWidth, availWidth);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 canvasPos = ImGui::GetCursorScreenPos();

        // グリッドを描画
        for (int y = 0; y < mapSize; y++) {
            for (int x = 0; x < mapSize; x++) {
                int index = y * mapSize + x;
                float h = HeightMap[index];

                // 高さを0〜1に正規化（例: -1〜1）
                float norm = h * 10.0f;
                norm = std::clamp(norm, 0.0f, 1.0f);
                ImU32 col = ImColor(norm, norm, norm);

                ImVec2 p0 = ImVec2(x * cellSize + canvasPos.x, y * cellSize + canvasPos.y);
                ImVec2 p1 = ImVec2(cellSize + p0.x, cellSize + p0.y);
                drawList->AddRectFilled(p0, p1, col);
                drawList->AddRect(p0, p1, IM_COL32(50, 50, 50, 0));
            }
        }

        // 入力領域
        ImGui::InvisibleButton("canvas", canvasSize);
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
            CurrentScale = 0;
            ImVec2 mousePos = ImGui::GetMousePos();
            int gx = (int)((mousePos.x - canvasPos.x) / cellSize);
            int gy = (int)((mousePos.y - canvasPos.y) / cellSize);

            if (gx >= 0 && gx < mapSize && gy >= 0 && gy < mapSize) {
                for (int dy = -brushRadius; dy <= brushRadius; dy++) {
                    for (int dx = -brushRadius; dx <= brushRadius; dx++) {
                        int nx = gx + dx;
                        int ny = gy + dy;
                        if (nx < 0 || nx >= mapSize || ny < 0 || ny >= mapSize) continue;

                        int idx = ny * mapSize + nx;
                        float dist = std::sqrt(float(dx * dx + dy * dy));
                        if (dist <= brushRadius) {
                            float strength = (1.0f - dist / brushRadius) * brushStrength;
                            if (brushMode == 0) { // Raise
                                HeightMap[idx] += strength;
                            } else if (brushMode == 1) { // Lower
                                HeightMap[idx] -= strength;
                            } else if (brushMode == 2) { // Smooth
                                float sum = 0.0f;
                                int count = 0;
                                for (int sy = -1; sy <= 1; sy++) {
                                    for (int sx = -1; sx <= 1; sx++) {
                                        int nnx = nx + sx;
                                        int nny = ny + sy;
                                        if (nnx < 0 || nnx >= mapSize || nny < 0 || nny >= mapSize) continue;
                                        sum += HeightMap[nny * mapSize + nnx];
                                        count++;
                                    }
                                }
                                if (count > 0) {
                                    float avg = sum / count;
                                    HeightMap[idx] = std::lerp(HeightMap[idx], avg, strength);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
};
