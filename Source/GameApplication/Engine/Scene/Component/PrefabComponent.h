#pragma once
#include "Interface/IComponent.h"
#include "BackEnds/YAMLConverters.h"
#include <string>

// プレファブからインスタンス化されたエンティティのルートに付与されるコンポーネント
// ソースファイルパスとインスタンス化時の YAML スナップショットを保持する
// プレファブ上書き保存や差分検出に利用する
class PrefabComponent : public IComponent {
public:
	std::string filePath;    // ソースプレファブファイルのパス
	std::string sourceYaml;  // インスタンス化時の YAML スナップショット（差分検出用）

	YAML::Node encode() override {
		YAML::Node node;
		node["FilePath"] = filePath;
		node["SourceYaml"] = sourceYaml;
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override {
		if (node["FilePath"])   filePath   = node["FilePath"].as<std::string>();
		if (node["SourceYaml"]) sourceYaml = node["SourceYaml"].as<std::string>();
		return true;
	}

	void inspector(SceneContext* context) override {
		ImGui::Text("Prefab:");
		ImGui::SameLine();
		ImGui::TextWrapped("%s", filePath.empty() ? "(none)" : filePath.c_str());
	}
};
