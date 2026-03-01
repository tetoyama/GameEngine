#pragma once
#include <string>
#include <memory>
#include "Entity/EntityRef.h"

struct SceneContext;
struct PrefabData;

// プレファブの保存・インスタンス化を担うシステム
// ロードは ResourceService を経由するため、同じファイルへの複数回のロードはキャッシュから返される
class PrefabSystem {
public:
	// エンティティ（およびその Transform 階層配下の全子孫）を YAML プレファブファイルとして保存する
	// 親子関係は PrefabParent ローカルインデックスとして記録されるため、
	// インスタンス化時に正しく再現される
	// @param ref     保存対象のルートエンティティへの EntityRef
	// @param filePath 保存先のファイルパス（例: "Asset/Prefab/Player.prefab"）
	// @return 成功した場合は true
	bool SavePrefab(EntityRef ref, const std::string& filePath);

	// リソースシステム経由でプレファブをロードしてエンティティ階層をインスタンス化する
	// ロード結果は ResourceService にキャッシュされる
	// @param context シーンコンテキスト（context->manager->resource を使用）
	// @param filePath プレファブファイルのパス
	// @return 生成されたルートエンティティへの EntityRef（失敗時は無効な EntityRef）
	EntityRef InstantiatePrefab(SceneContext* context, const std::string& filePath);

	// 既にロード済みの PrefabData からエンティティ階層をインスタンス化する
	// TransformComponent の children リストはインスタンス化後に自動で再構築される
	// @param context シーンコンテキスト
	// @param data    ロード済みのプレファブデータ
	// @return 生成されたルートエンティティへの EntityRef（失敗時は無効な EntityRef）
	EntityRef Instantiate(SceneContext* context, const std::shared_ptr<PrefabData>& data);
};
