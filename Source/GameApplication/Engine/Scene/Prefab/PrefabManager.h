#pragma once
#include <string>
#include <memory>
#include "Entity/Entity.h"
#include "scene.h"

struct PrefabData;

// プレファブの保存・インスタンス化を管理するクラス
// ロードは ResourceService を経由するため、同じファイルへの複数回のロードはキャッシュから返される
class PrefabManager {
public:
	// エンティティを YAML プレファブファイルとして保存する
	// @param entity  保存対象のエンティティ
	// @param context シーンコンテキスト
	// @param filePath 保存先のファイルパス（例: "Asset/Prefab/Player.prefab"）
	// @return 成功した場合は true
	bool SavePrefab(Entity entity, SceneContext* context, const std::string& filePath);

	// リソースシステム経由でプレファブをロードしてエンティティをインスタンス化する
	// ロード結果は ResourceService にキャッシュされる
	// @param context シーンコンテキスト（context->manager->resource を使用）
	// @param filePath プレファブファイルのパス
	// @return 生成されたエンティティ（失敗時は 0）
	Entity InstantiatePrefab(SceneContext* context, const std::string& filePath);

	// 既にロード済みの PrefabData からエンティティをインスタンス化する
	// @param context シーンコンテキスト
	// @param data    ロード済みのプレファブデータ
	// @return 生成されたエンティティ（失敗時は 0）
	Entity Instantiate(SceneContext* context, const std::shared_ptr<PrefabData>& data);
};
