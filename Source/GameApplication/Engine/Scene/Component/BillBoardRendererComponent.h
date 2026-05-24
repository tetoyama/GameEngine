// =======================================================================
// 
// BillBoardRendererComponent.h
// 
// =======================================================================
#pragma once
#include "Interface/IComponent.h"
#include "Backends/YAMLConverters.h"
#include <string>

// ビルボード回転軸の設定
struct RotateAxis {
	bool x = true;
	bool y = true;
	bool z = true;
};

// ビルボード描画を管理するコンポーネント
class BillBoardRendererComponent: public IComponent {
public:
	// --- YAMLエンコード ---
	YAML::Node encode() override{
		YAML::Node node;
		node["RotateX"] = RotateXYZ.x;
		node["RotateY"] = RotateXYZ.y;
		node["RotateZ"] = RotateXYZ.z;
		return node;
	}

	// --- YAMLデコード ---
	bool decode(SceneContext* context, const YAML::Node& node) override{
		if(node["RotateX"]) RotateXYZ.x = node["RotateX"].as<bool>();
		if(node["RotateY"]) RotateXYZ.y = node["RotateY"].as<bool>();
		if(node["RotateZ"]) RotateXYZ.z = node["RotateZ"].as<bool>();
		return true;
	}

	// --- ImGuiインスペクター表示 ---
	void inspector(SceneContext* context) override{
		ImGui::Text("BillBoardRendererComponent");

		ImGui::Text("Rotate X:");
		ImGui::SameLine(100);
		ImGui::UndoCheckbox(("##RotateX_" + std::to_string(reinterpret_cast<std::uintptr_t>(this))).c_str(), &RotateXYZ.x);

		ImGui::Text("Rotate Y:");
		ImGui::SameLine(100);
		ImGui::UndoCheckbox(("##RotateY_" + std::to_string(reinterpret_cast<std::uintptr_t>(this))).c_str(), &RotateXYZ.y);

		ImGui::Text("Rotate Z:");
		ImGui::SameLine(100);
		ImGui::UndoCheckbox(("##RotateZ_" + std::to_string(reinterpret_cast<std::uintptr_t>(this))).c_str(), &RotateXYZ.z);
	}

	// --- 回転設定 ---
	RotateAxis rotateXyz;
};
