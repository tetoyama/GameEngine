// =======================================================================
// 
// engine.cpp
// 
// =======================================================================

#include "buildSetting.h"

#include "engine.h"
#include "engineContext.h"

#include "Platform/WindowSystem/windowSystem.h"

#include "Icon/icon.h"
#include "Taskbar/taskbar.h"

#include "Runtime/TimeService/timeService.h"

#include "Graphics/GraphicsContext.h"
#include "Graphics/mainRenderer.h"

#include "DebugTools/ImGuiSystem.h"
#include "DebugTools/DebugSystem.h"

#include "Platform/InputSystem/InputSystem.h"

#include "Scene/sceneManager.h"
#include "Scene/scene.h"

#include "Resources/resourceService.h"

#include "Editor/editorService.h"
#include "Editor/UI/MenuBar.h"

#include "Config/ConfigSystem.h"

#include "LlamaService/LLAMAService.h"

#include "Backends/ImGuiFunc.h"

#include <dxgidebug.h>
#include "Audio/audioContext.h"
#pragma comment(lib, "dxguid.lib")

// -----------------------------------------------------------------------
// Engine::Initialize
// エンジン全体を初期化する。各サービスを依存関係の順番に初期化し、
// メインウィンドウ・メニューバーのイベントを登録する。
// 引数:
//   context     - 全サービスを保持するエンジンコンテキスト
//   hInstance   - Win32 アプリケーションインスタンスハンドル
//   nCmdShow    - ウィンドウの初期表示状態（SW_SHOW 等）
// -----------------------------------------------------------------------
void Engine::Initialize(std::shared_ptr<EngineContext> context, HINSTANCE hInstance, int nCmdShow){

    if (!context) {
        OutputDebugStringA("EngineContext が nullptr です。\n");
        return;
    }

    // ContextからEngineContextに登録された全サービスを取得
    auto debugLogSystem = context->Get<DebugLogService>();
	auto imguiService = context->Get<ImGuiService>();
	auto editorService = context->Get<EditorService>();
	auto inputService = context->Get<InputService>();
	auto windowService = context->Get<WindowService>();
	auto configSystem = context->Get<ConfigService>();
	auto timeService = context->Get<TimeService>();
	auto graphicsContext = context->Get<GraphicsContext>();
	auto resourceService = context->Get<ResourceService>();
	auto mainRenderer = context->Get<MainRenderer>();
	auto sceneManager = context->Get<SceneManager>();
	auto audioContext = context->Get<AudioContext>();
	auto llamaService = context->Get<LLAMAService>();

	// --- Step 1: デバッグログ初期化（他のサービスより先に初期化し、ログ出力を有効化）
	if(debugLogSystem){
		debugLogSystem->Initialize();
		debugLogSystem->LOG_INFO("Engine::Initialize を開始します");
	}

	// --- Step 2: アプリ設定の読み込み（ウィンドウサイズ・スタートシーン等の設定を取得）
	if(!configSystem || !configSystem->Initialize()){
		if(debugLogSystem){
			debugLogSystem->LOG_ERROR("ConfigService の初期化に失敗しました");
		}
		OutputDebugStringA("configSystem の初期化に失敗しました。\n");
	} else if(debugLogSystem){
		debugLogSystem->LOG_INFO("ConfigService の初期化が完了しました");
	}

    // --- Step 3: ウィンドウ生成（設定ファイルに基づいて OS ウィンドウを作成）
    if (!windowService || !windowService->Initialize(hInstance, nCmdShow,configSystem->appConfig)) {
        if(debugLogSystem){
			debugLogSystem->LOG_ERROR("WindowService の初期化に失敗しました");
        }
        OutputDebugStringA("WindowService の初期化に失敗しました。\n");
    } else if(debugLogSystem){
		debugLogSystem->LOG_INFO("WindowService の初期化が完了しました");
    }
	auto mainWindow = windowService->GetMainWindow();

	// タスクバーアイコンとプログレスバーの初期化
	InitTaskBar(windowService->GetMainWindow()->GetHWND());

	// --- Step 4: タイムサービス初期化（デルタタイム計測開始）
	if(timeService){
		timeService->Initialize();
		if(debugLogSystem){
			debugLogSystem->LOG_TRACE("TimeService の初期化が完了しました");
		}
	}

	// --- Step 5: オーディオ初期化（XAudio2 エンジン起動）
	if(!audioContext || !audioContext->Initialize()){
		if(debugLogSystem){
			debugLogSystem->LOG_ERROR("AudioContext の初期化に失敗しました");
		}
		OutputDebugStringA("AudioContext の初期化に失敗しました。\n");
	} else if(debugLogSystem){
		debugLogSystem->LOG_INFO("AudioContext の初期化が完了しました");
	}

    // --- Step 6: DirectX 11 グラフィクスコンテキスト初期化
	//             デバイス・デバイスコンテキスト・スワップチェーンを生成
    if (!graphicsContext || 
        !graphicsContext->Initialize(windowService->GetMainWindow()->GetHWND(), mainWindow->GetWidth(), mainWindow->GetHeight())){
        if(debugLogSystem){
			debugLogSystem->LOG_ERROR("GraphicsContext の初期化に失敗しました");
        }
        OutputDebugStringA("GraphicsContext の初期化に失敗しました。\n");
    } else if(debugLogSystem){
		debugLogSystem->LOG_INFO("GraphicsContext の初期化が完了しました");
    }

	// --- Step 7: リソースサービス初期化（テクスチャ・モデル・シェーダのローダー設定）
	if(resourceService){
		resourceService->Initialize(graphicsContext.get(), audioContext.get(), debugLogSystem.get());
	}

	// --- Step 8: 入力サービス初期化（DirectInput/Win32 メッセージによる入力ハンドラ設定）
	if(inputService){
		inputService->Initialize(windowService->GetMainWindow()->GetHWND());
		if(debugLogSystem){
			debugLogSystem->LOG_TRACE("InputService の初期化が完了しました");
		}
	}

	// --- Step 9: ImGui 初期化（Dear ImGui のバックエンドをウィンドウと D3D11 デバイスに紐付け）
    if (!imguiService || 
        !imguiService->Initialize(windowService->GetMainWindow().get(), graphicsContext.get())) {
        if(debugLogSystem){
			debugLogSystem->LOG_ERROR("ImGuiService の初期化に失敗しました");
        }
        OutputDebugStringA("ImGuiService の初期化に失敗しました。\n");
    } else if(debugLogSystem){
		debugLogSystem->LOG_INFO("ImGuiService の初期化が完了しました");
    }

	// --- Step 10: メインレンダラー初期化（レンダーターゲット・デプスバッファ・ビューポート設定）
	if(mainRenderer){
		mainRenderer->Initialize(graphicsContext.get(), windowService->GetMainWindow().get());
		if(debugLogSystem){
			debugLogSystem->LOG_INFO("MainRenderer の初期化が完了しました");
		}
	}

    // メインウィンドウに各サービスを紐付け（ウィンドウリサイズ時の再初期化等に必要）
    if (mainWindow) {
        mainWindow->SetMainRenderer(mainRenderer.get());
        mainWindow->SetImGuiSystem(imguiService.get());
        mainWindow->SetInputSystem(inputService.get());
    }

	// --- Step 11: エディターサービス初期化（エディタービルド時のみ実行）
	if(editorService){
		EditorServiceContext editorContext;
		editorContext.debugLogSystem = debugLogSystem.get();
		editorContext.llamaService = llamaService.get();
		editorContext.resourceService = resourceService.get();
		editorContext.sceneManager = sceneManager.get();

		editorService->Initialize(editorContext);
		if(debugLogSystem){
			debugLogSystem->LOG_INFO("EditorService の初期化が完了しました");
		}
	}

    // --- Step 12: シーンマネージャ初期化（ECS・レンダリングシステムの登録）
	if(sceneManager){
		// シーンマネージャコンテキストの設定（全依存サービスへの参照を渡す）
		SceneManagerContext sceneManagerContext{};
		sceneManagerContext.audio = audioContext.get();
		sceneManagerContext.graphics = graphicsContext.get();
		sceneManagerContext.renderer = mainRenderer.get();
		sceneManagerContext.input = inputService.get();
		sceneManagerContext.resource = resourceService.get();
		sceneManagerContext.hwnd = mainRenderer->GetHWND();
		sceneManagerContext.debug = debugLogSystem.get();
		sceneManagerContext.imgui = imguiService.get();
		sceneManagerContext.sceneManager = sceneManager.get();
		sceneManagerContext.config = configSystem.get();
		sceneManagerContext.editor = editorService.get();
		// SceneManagerの初期化（システムレジストリの構築とシステムの登録）
		sceneManager->Initialize(sceneManagerContext);
		if(debugLogSystem){
			debugLogSystem->LOG_INFO("SceneManager の初期化が完了しました");
		}
	}

    // --- Step 13: メニューバーのイベント登録（File / Edit メニュー操作を各サービスにバインド）
	if(editorService){
		auto menubar = editorService->GetUI<MenuBar>();
		if(menubar){
			// ファイルメニュー: アプリ終了
			menubar->Register(MenuEvent::File_Exit, [windowService](){
				windowService->GetMainWindow()->Close();
							  });
			// ファイルメニュー: 新規シーン作成
			menubar->Register(MenuEvent::File_New, [sceneManager](){
				sceneManager->AddScene(std::make_shared<Scene>());
							  });
			// ファイルメニュー: 現在の全シーンを YAML ファイルに保存
			menubar->Register(MenuEvent::File_Save, [sceneManager](){
				sceneManager->SaveScenes();
							  });
			// ファイルメニュー: ダイアログから YAML シーンファイルを開く
			menubar->Register(MenuEvent::File_Open, [sceneManager](){
				sceneManager->AddScene(sceneManager->OpenFromYAMLFile());
							  });
			// 編集メニュー: アンドゥ（CommandManager の履歴を1つ戻す）
			menubar->Register(MenuEvent::Edit_Undo, [editorService](){
				editorService->commandManager.Undo();
			});
			// 編集メニュー: リドゥ（CommandManager の履歴を1つ進める）
			menubar->Register(MenuEvent::Edit_Redo, [editorService](){
				editorService->commandManager.Redo();
			});
		}
	}

	// --- Step 14: LLAMA（LLM）サービス初期化（モデルファイルのロードと推論エンジンの起動）
	if (llamaService) {

		LLAMAServiceContext llamaContext{};
		llamaContext.resourceService = resourceService.get();

		llamaService->Initialize(llamaContext);
		if(debugLogSystem){
			debugLogSystem->LOG_INFO("LLAMAService の初期化が完了しました");
		}
	}

    if(debugLogSystem){
		debugLogSystem->LOG_INFO("EngineContext の初期化が完了しました");
    }
}

// -----------------------------------------------------------------------
// Engine::Shutdown
// エンジン全体を終了する。全サービスを逆順にシャットダウンし、
// DXGI デバッグインターフェースを通じてリソースリークを報告する。
// 引数:
//   context - 終了対象のエンジンコンテキスト
// -----------------------------------------------------------------------
void Engine::Shutdown(std::shared_ptr<EngineContext> context){

	// 全サービスを依存関係の逆順にシャットダウン
	context->Shutdown();

	// DXGI デバッグ: 解放漏れの DirectX オブジェクトをデバッグ出力に報告
	// （dxgidebug.dll は実行時に動的ロードすることでリリースビルドの依存を回避）
	typedef HRESULT(WINAPI* LPDXGIGetDebugInterface)(REFIID, void**);
	HMODULE dxgiDebugModule = LoadLibraryW(L"dxgidebug.dll");
	if(dxgiDebugModule){
		auto dxgiGetDebugInterface = reinterpret_cast<LPDXGIGetDebugInterface>(
			GetProcAddress(dxgiDebugModule, "DXGIGetDebugInterface"));

		if(dxgiGetDebugInterface){
			IDXGIDebug* dxgiDebug = nullptr;
			if(SUCCEEDED(dxgiGetDebugInterface(IID_PPV_ARGS(&dxgiDebug)))){
				// 生存中の DirectX オブジェクトを詳細付きで列挙
				dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_DETAIL);
				dxgiDebug->Release();
			}
		}
		FreeLibrary(dxgiDebugModule);
	}
}

// -----------------------------------------------------------------------
// Engine::Run
// メインゲームループを実行する。ウィンドウが閉じられるまで以下のフェーズを繰り返す：
//   1. Update       - 入力取得・シーン更新（可変時間）
//   2. FixedUpdate  - 物理シミュレーション（固定時間、追いつくまで複数回実行可能）
//   3. Draw         - ImGui・シーン・エディター描画、フレームバッファのスワップ
// 引数:
//   context - 実行対象のエンジンコンテキスト
// -----------------------------------------------------------------------
void Engine::Run(std::shared_ptr<EngineContext> context){

	if (!context) {
		OutputDebugStringA("EngineContext が nullptr です。\n");
		return;
	}
	auto debugLogSystem = context->Get<DebugLogService>();

	if(debugLogSystem){
		debugLogSystem->LOG_INFO("Engine の実行を開始します");
	}

	// 必要なサービスを取得
	auto windowService = context->Get<WindowService>();
	auto configSystem = context->Get<ConfigService>();
	auto timeService = context->Get<TimeService>();
	auto graphicsContext = context->Get<GraphicsContext>();
	auto inputService = context->Get<InputService>();
	auto imguiService = context->Get<ImGuiService>();
	auto mainRenderer = context->Get<MainRenderer>();
	auto sceneManager = context->Get<SceneManager>();
	auto editorService = context->Get<EditorService>();

	if(debugLogSystem){
		debugLogSystem->LOG_TRACE("EngineContext から実行時サービスを取得しました");
	}

	// 最初のシーンを作成・ロード
	auto initialScene = std::make_shared<Scene>();
#ifdef _DEBUG_BUILD
	sceneManager->LoadScene(initialScene);
#else
	sceneManager->LoadFromFilePath(configSystem->appConfig.startSceneFilePath);
#endif // _DEBUG_BUILD
	if(debugLogSystem){
		debugLogSystem->LOG_INFO("初期シーンのロードが完了しました");
	}

	// ウィンドウが閉じられるまでメインループを実行
	while(!windowService->GetMainWindow()->ShouldClose()){

		// フレーム開始タイムスタンプを記録
		timeService->Tick();

		float dt = timeService->GetDeltaTime();

		{	// ============ FixedUpdate フェーズ ============
			// 固定タイムステップに追いつくまで物理シミュレーションを複数回実行
			while(timeService->ShouldRunFixedUpdate()){
				sceneManager->FixedUpdate(timeService->GetFixedDeltaTime());
				timeService->EndFixedUpdate();
			}
		}

		timeService->BeginDeltaUpdate();
		{	// ============ Update フェーズ ============
			// OS メッセージ処理（WM_CLOSE 等のイベントを消化）
			windowService->PollEvents();
			// 入力状態を前フレームから更新（押下・離し・保持の状態遷移）
			inputService->Update();
			// 全シーンのゲームロジックを更新（スクリプト・アニメーション等）
			sceneManager->Update(dt);
		}
		// Update フェーズの所要時間を記録
		timeService->EndDeltaUpdate();

		// Update 後にウィンドウが閉じられた場合はループを抜ける
		if (windowService->GetMainWindow()->ShouldClose()) {
			break;
		}
		
		{	// ============ Draw フェーズ ============
			// レンダーターゲットのクリアとビューポート設定
			timeService->BeginDraw();
			mainRenderer->BeginFrame();
			// ImGui フレームの開始（NewFrame 呼び出し）
			imguiService->Begin();
			{
				// アンドゥ対応 ImGui ラッパーに CommandManager を設定
				// （ImGui::UndoDragFloat 等でアンドゥ履歴を記録するために必要）
				if (editorService) {
					ImGui::SetCommandManager(&editorService->commandManager);
				}

				// シーン内の全レンダーシステムを実行（GBuffer → Lighting → PostEffect 等）
				sceneManager->Draw();
				// デバッグログウィンドウを描画
				debugLogSystem->Draw();

				// エディター UI を描画（ヒエラルキー・インスペクター・パフォーマンスモニター等）
				if(editorService){
					EditorDrawContext editorDrawContext{};
					editorDrawContext.UpdateTime = timeService->GetDeltaUpdateTime();
					editorDrawContext.DrawTime = timeService->GetDrawTime();
					editorDrawContext.FPS = timeService->GetFrameFPS();
					editorDrawContext.FixedUpdateFPS = timeService->GetFixedUpdateFPS();
					editorService->Draw(editorDrawContext);
				}
			}
			// ImGui の描画コマンドを実行
			imguiService->End();
			// スワップチェーンのPresent（Vsync 設定を反映）
			mainRenderer->EndFrame(configSystem->appConfig.Vsync);
			// Draw フェーズの所要時間を記録
			timeService->EndDraw();
		}
	}
	if(debugLogSystem){
		debugLogSystem->LOG_INFO("Engine の実行ループを終了します");
	}
}
