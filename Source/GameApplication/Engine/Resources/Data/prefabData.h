#pragma once
#include <string>
#include <backends/yaml-cpp/yaml.h>

// プレファブリソースデータ
// YAML ファイルから読み込んだコンポーネント一覧のスナップショットを保持する
// ResourceService によってキャッシュされる
struct PrefabData {
	std::string filePath;
	YAML::Node root; // "Components" シーケンスを含む YAML ノード
};
