#pragma once
#include <string>
#include <memory>
#include "Entity/Entity.h"
#include "scene.h"

// プレファブデータ（YAML スナップショット）
struct PrefabData {
	std::string name;
	std::string filePath;
};

// プレファブの保存・読み込み・インスタンス化を管理するクラス
class PrefabManager {
public:
	// エンティティを YAML プレファブファイルとして保存する
	// @param entity  保存対象のエンティティ
	// @param context シーンコンテキスト
	// @param filePath 保存先のファイルパス（例: "Asset/Prefab/Player.prefab"）
	// @return 成功した場合は true
	bool SavePrefab(Entity entity, SceneContext* context, const std::string& filePath);

	// YAML プレファブファイルからエンティティをインスタンス化する
	// @param context シーンコンテキスト
	// @param filePath プレファブファイルのパス
	// @return 生成されたエンティティ（失敗時は 0）
	Entity InstantiatePrefab(SceneContext* context, const std::string& filePath);
};
