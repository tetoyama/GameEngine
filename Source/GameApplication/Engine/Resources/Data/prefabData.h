// =======================================================================
// 
// prefabData.h
// 
// =======================================================================
#pragma once
#include <string>
#include <backends/yaml-cpp/yaml.h>

// プレファブリソースデータ
// YAML ファイルから読み込んだエンティティ階層のスナップショットを保持する
// ResourceService によってキャッシュされる
//
// YAML フォーマット:
//   Entities:
//     - LocalID: 0           # プレファブ内の連番 ID（0 = ルート）
//       PrefabParent: <省略> # 親なし（ルート）の場合はキー自体を省略する
//       Components: [...]
//     - LocalID: 1
//       PrefabParent: 0      # LocalID 0 が親（常に 0 以上の整数）
//       Components: [...]
struct PrefabData {
	std::string filePath;
	YAML::Node root; // "Entities" シーケンスを含む YAML ノード
};
