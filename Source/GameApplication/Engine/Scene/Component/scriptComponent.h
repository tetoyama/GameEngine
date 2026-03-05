// =======================================================================
// 
// scriptComponent.h
// 
// =======================================================================
#pragma once
#include "Interface/IComponent.h"
#include "Interface/IScriptComponent.h"
#include <backends/yaml-cpp/yaml.h>

#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Registry/systemRegistry.h"
#include "System/Script/ScriptSystem.h"
#include <variant>

// C#スクリプトの実行を管理するコンポーネント
// ScriptSystem に登録された IScriptComponent 派生クラスのインスタンスを保持し、
// ライフサイクル（Start / Update / Stop）と YAML シリアライズを担う
class ScriptComponent: public IComponent {
public:
	ScriptComponent() = default;

	// デストラクタ: 保持する全スクリプトを停止・解放する
	~ScriptComponent() override{
		for(auto& [_, script] : scripts){
			script->Stop();
			delete script;
		}
		scripts.clear();
	}

	// コピー禁止（スクリプトインスタンスは一意であるべきため）
	ScriptComponent(const ScriptComponent&) = delete;
	ScriptComponent& operator=(const ScriptComponent&) = delete;

	// ムーブは許可（コンポーネントストレージの再配置時に使用）
	ScriptComponent(ScriptComponent&&) = default;
	ScriptComponent& operator=(ScriptComponent&&) = default;

	// 全スクリプトの状態を YAML にエンコードする
	YAML::Node encode() override{
		YAML::Node node;
		for(auto& [name, script] : scripts){
			node[name] = script->Encode();
		}
		return node;
	}

	// YAML からスクリプトを復元する（ScriptSystem::Create で動的にインスタンスを生成）
	bool decode(SceneContext* context, const YAML::Node& node) override;

	// インスペクター UI（スクリプトの追加ボタンとパラメーター表示）
	void inspector(SceneContext* context) override;

	// 指定した名前のスクリプトを追加する
	// 既に同名のスクリプトが存在する場合や ScriptSystem が見つからない場合は失敗する
	bool AddScript(const char* scriptName, SceneContext* context);

	// スクリプト名 → インスタンスのマップ（同一コンポーネントに複数スクリプトを添付可能）
	std::unordered_map<
		std::string,
		IScriptComponent*
	> scripts;
};
