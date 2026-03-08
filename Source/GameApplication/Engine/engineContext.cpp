// =======================================================================
// 
// engineContext.cpp
// 
// =======================================================================

#include <memory>
#include "engineContext.h"

#include "Platform/WindowSystem/windowSystem.h"
#include "Runtime/TimeService/timeService.h"
#include "Graphics/GraphicsContext.h"
#include "DebugTools/ImGuiSystem.h"
#include "DebugTools/DebugSystem.h"
#include "Platform/InputSystem/InputSystem.h"
#include "Scene/sceneManager.h"
#include "Graphics/mainRenderer.h"
#include "Resources/resourceService.h"
#include "Audio/audioContext.h"
#include "Config/ConfigSystem.h"
#include "Editor/editorService.h"
#include "LlamaService/LLAMAService.h"

// -----------------------------------------------------------------------
// EngineContextBuilder::Build
// 全エンジンサービスを生成して EngineContext に登録し返す
// 注意: 登録順序は Shutdown 時の逆順になるため、依存関係を考慮すること
//   依存関係の例: GraphicsContext → MainRenderer → SceneManager
// -----------------------------------------------------------------------
std::shared_ptr<EngineContext> EngineContextBuilder::Build(){

	std::shared_ptr<EngineContext> context = std::make_shared<EngineContext>();

	// DebugLogService 登録（他サービスのコンストラクタDIに利用し、Shutdown時は最後に終了させる）
	auto debugLogSystem = std::make_shared<DebugLogService>();
	context->Register<DebugLogService>(debugLogSystem);

	// ConfigService 登録（アプリ設定の読み込み。他サービスより先に初期化が必要）
	auto configSystem = std::make_shared<ConfigService>();
	context->Register<ConfigService>(configSystem);

	// WindowService 登録（OS ウィンドウの生成・管理）
	auto windowSystem = std::make_shared<WindowService>(debugLogSystem.get());
	context->Register<WindowService>(windowSystem);

	// TimeService 登録（デルタタイム・固定更新タイマーの管理）
	auto timeService = std::make_shared<TimeService>();
	context->Register<TimeService>(timeService);

	// AudioContext 登録（XAudio2 を用いたオーディオ再生管理）
	auto audioContext = std::make_shared<AudioContext>(debugLogSystem.get());
	context->Register<AudioContext>(audioContext);

	// GraphicsContext 登録（DirectX 11 デバイス・スワップチェーンの管理）
	auto graphicsContext = std::make_shared<GraphicsContext>(debugLogSystem.get());
	context->Register<GraphicsContext>(graphicsContext);

	// MainRenderer 登録（描画フレームの開始・終了とスワップチェーン制御）
	auto mainRenderer = std::make_shared<MainRenderer>();
	context->Register<MainRenderer>(mainRenderer);

	// InputService 登録（キーボード・マウス・ゲームパッド入力の取得）
	auto inputSystem = std::make_shared<InputService>();
	context->Register<InputService>(inputSystem);

	// ResourceService 登録（テクスチャ・モデル・シェーダ等のリソース読み込み・キャッシュ管理）
	auto resourceSystem = std::make_shared<ResourceService>();
	context->Register<ResourceService>(resourceSystem);

	// SceneManager 登録（複数シーンのライフサイクル・再生状態管理）
	auto sceneManager = std::make_shared<SceneManager>();
	context->Register<SceneManager>(sceneManager);

	// ImGuiService 登録（Dear ImGui の初期化・フレーム管理）
	auto imgui = std::make_shared<ImGuiService>();
	context->Register<ImGuiService>(imgui);

#ifdef _EDITOR
	// EditorService 登録（エディター UI・コマンド履歴・アナライザ管理。エディタービルド時のみ有効）
	auto editorService = std::make_shared<EditorService>();
	context->Register<EditorService>(editorService);
#endif // _EDITOR

	// LLAMAService 登録（LLM（大規模言語モデル）によるエージェント機能の管理）
	auto llamaService = std::make_shared<LLAMAService>(debugLogSystem.get());
	context->Register<LLAMAService>(llamaService);

	// 今後: 他のシステムもここで登録

	return context;
}

// -----------------------------------------------------------------------
// EngineContext::Shutdown
// 登録済み全サービスを登録の逆順でシャットダウンする
// 依存するサービスが先に終了しないよう逆順で呼び出す
// -----------------------------------------------------------------------
void EngineContext::Shutdown() {

	// 逆順でShutdown呼び出し（依存性の逆順に終了する）
	for (auto it = m_ServiceOrder.rbegin(); it != m_ServiceOrder.rend(); ++it) {
		auto found = m_Services.find(*it);
		if (found != m_Services.end()) {
			auto service = std::static_pointer_cast<IService>(found->second);
			if (service) service->Shutdown();
		}
	}
	// サービスマップと順序リストを全てクリア
	m_Services.clear();
	m_ServiceOrder.clear();
}
