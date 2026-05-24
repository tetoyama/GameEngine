// =======================================================================
// 
// editorService.h
// 
// =======================================================================
#pragma once
#include "Service/IService.h"
#include <vector>
#include "InterFace/IEditorUI.h"
#include "Command/CommandManager.h"

struct EditorDrawContext;
class editorService;

class resourceService;
class debugLogService;
class sceneManager;
class llamaservice;

class analyzerManager;

struct SceneManagerContext;

// エディターサービスの初期化に必要な依存サービスをまとめた構造体
struct EditorServiceContext {
	DebugLogService* debugLogSystem = nullptr;   // デバッグログ出力サービス
	ResourceService* resourceService = nullptr;  // リソース読み込みサービス
	SceneManager*    sceneManager    = nullptr;  // シーン管理サービス
	LLAMAService*    llamaService    = nullptr;  // LLM エージェントサービス
};

// エディターのUIパネル・コマンド履歴・コード解析機能をまとめて管理するサービス
// IEditorUI を継承した各パネル（Hierarchy, Inspector 等）のライフサイクルを制御する
class EditorService : public IService {

public:

	// エディターサービスを初期化し、全 UI パネルと AnalyzerManager を生成・起動する
	void Initialize(EditorServiceContext context);

	// 毎フレーム全 UI パネルを描画する
	// ctx にはパフォーマンス情報（FPS・更新時間等）が含まれる
	void Draw(EditorDrawContext ctx);

	// 全 UI パネルと AnalyzerManager を解放する
	void Shutdown() override;

	// 型 T の UI パネルを取得する（T は IEditorUI の派生クラスである必要がある）
	// 見つからない場合は nullptr を返す
	template<typename T>
	T* GetUI() {
		static_assert(std::is_base_of<IEditorUI, T>::value, "T must inherit from IEditorUI");
		for (auto& ui : UIs) {
			if (auto p = dynamic_cast<T*>(ui)) {
				return p;
			}
		}
		return nullptr;
	}

	DebugLogService* debugLogSystem = nullptr;  // デバッグログサービスへの参照
	ResourceService* resourceService = nullptr; // リソースサービスへの参照
	SceneManager*    sceneManager    = nullptr; // シーンマネージャへの参照
	LLAMAService*    llamaService    = nullptr; // LLAMA サービスへの参照

	AnalyzerManager* analyzer = nullptr;        // ソースコード解析マネージャ

	CommandManager commandManager;              // アンドゥ・リドゥ対応コマンド履歴

private:

	std::vector<IEditorUI*> UIs;  // 管理する全 UI パネルのリスト
};

// エディター描画フェーズに渡すパフォーマンス統計情報
struct EditorDrawContext {
	double updateTime;		// 更新フェーズの所要時間（秒）
	double drawTime;		// 描画フェーズの所要時間（秒）
	double fps;				// 更新フレームレート（fps）
	double fixedUpdateFps;	// 固定更新フレームレート（fps）
};
