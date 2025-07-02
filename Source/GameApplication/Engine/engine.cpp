#include "engine.h"
#include "engineContext.h"

#include "Platform/WindowSystem/windowSystem.h"

#include "../Backends/Icon/icon.h"
#include "../Backends/Taskbar/taskbar.h"

#include "Runtime/TimeService/time.h"

#include "Graphics/GraphicsContext.h"
#include "Graphics/mainRenderer.h"

#include "DebugTools/ImGuiSystem.h"
#include "DebugTools/DebugSystem.h"

#include "Platform/InputSystem/InputSystem.h"

#include "Scene/sceneManager.h"
#include "Scene/scene.h"

#include "Engine/Resources/resourceSystem.h"
#include "Engine/EditorUI/ImGuiMainManuBar.h"

#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")

void Engine::Initialize(std::shared_ptr<EngineContext> context, HINSTANCE hInstance, int nCmdShow){
    if (!context) {
        OutputDebugStringA("EngineContext が nullptr です。\n");
        return;
    }
    // デバッグ出力システム取得・初期化
    auto debugLogSystem = context->Get<DebugLogSystem>();
	auto imguiService = context->Get<ImGuiService>();

    if(!debugLogSystem){
        OutputDebugStringA("DebugLogSystem サービスの取得に失敗しました。\n");
        return;
    }
    debugLogSystem->Initialize(&imguiService->GetManubar()->showConsole);
    debugLogSystem->LOG_INFO("DebugLogSystemが起動しました");

    // ウィンドウシステム初期化
    auto windowService = context->Get<WindowService>();
    if (!windowService || !windowService->Initialize(hInstance, nCmdShow)) {
        OutputDebugStringA("WindowService の初期化に失敗しました。\n");
        return;
    }
    debugLogSystem->LOG_DEBUG("WindowServiceが正常に作成されました");

	// タスクバーアイコンの初期化
	if(!SUCCEEDED(InitIcon(windowService->GetMainWindow()->GetHWND()))){
		OutputDebugStringA("アイコンの初期化に失敗しました。\n");
		return;
	}
	InitTaskBar(windowService->GetMainWindow()->GetHWND());

	debugLogSystem->LOG_DEBUG("タスクバーアイコンが正常に作成されました");

    // 時間管理サービスの初期化
    auto timeService = context->Get<TimeService>();
    if (!timeService) return;
    timeService->Initialize();
    debugLogSystem->LOG_DEBUG("TimeServiceが正常に作成されました");

    // DirectX 描画コンテキスト初期化
    auto graphicsContext = context->Get<GraphicsContext>();
    if (!graphicsContext || 
        !graphicsContext->Initialize(windowService->GetMainWindow()->GetHWND(), DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT)) {
        OutputDebugStringA("GraphicsContext の初期化に失敗しました。\n");
        return;
    }
    debugLogSystem->LOG_DEBUG("GraphicsContextが正常に作成されました");

    // リソース管理初期化
    auto resourceService = context->Get<ResourceService>();
    if(!resourceService) return;
    resourceService->Initialize(graphicsContext.get());
    debugLogSystem->LOG_DEBUG("ResourceServiceが正常に作成されました");

    // 入力処理初期化
    auto inputService = context->Get<InputService>();
    if (!inputService) return;
    inputService->Initialize(windowService->GetMainWindow()->GetHWND());
    debugLogSystem->LOG_DEBUG("InputServiceが正常に作成されました");

    // ImGui デバッグUI初期化
    if (!imguiService || 
        !imguiService->Initialize(windowService->GetMainWindow().get(), graphicsContext.get())) {
        OutputDebugStringA("ImGuiService の初期化に失敗しました。\n");
        return;
    }
    debugLogSystem->LOG_DEBUG("ImGuiSystemが正常に作成されました");

    // メインレンダラー初期化
    auto mainRenderer = context->Get<MainRenderer>();
    if (!mainRenderer) return;
    mainRenderer->Initialize(graphicsContext.get(), windowService->GetMainWindow().get());
    debugLogSystem->LOG_DEBUG("MainRendererが正常に作成されました");

    // メインウィンドウに各サービスを紐付け
    auto mainWindow = dynamic_cast<MainWindow*>(windowService->GetMainWindow().get());
    if (mainWindow) {
        mainWindow->SetMainRenderer(mainRenderer.get());
        mainWindow->SetImGuiSystem(imguiService.get());
        mainWindow->SetInputSystem(inputService.get());
    }

    // シーンマネージャ初期化
    auto sceneManager = context->Get<SceneManager>();
    if (!sceneManager) return;

	// SceneManagerの初期化
	SceneManagerContext sceneContext{};
	sceneContext.graphics = graphicsContext.get();
	sceneContext.renderer = mainRenderer.get();
	sceneContext.input = inputService.get();
	sceneContext.resource = resourceService.get();
	sceneContext.hwnd = mainRenderer->GetHWND();
	sceneContext.debug = debugLogSystem.get();
	sceneContext.imgui = imguiService.get();
	sceneContext.sceneManager = sceneManager.get();
	sceneManager->Initialize(sceneContext);

	debugLogSystem->LOG_DEBUG("SceneManagerが正常に作成されました");

    // ImGuiメニューバー操作にイベントを登録
    auto manubar = imguiService->GetManubar();
    manubar->Register(MenuEvent::File_Exit, [windowService](){
		windowService->GetMainWindow()->Close(); 
					  });
    manubar->Register(MenuEvent::File_New,  [sceneManager](){ 
		sceneManager->LoadScene(std::make_shared<Scene>()); 
					  });
    manubar->Register(MenuEvent::File_Save, [sceneManager](){
		sceneManager->SaveScene(); 
					  });
    manubar->Register(MenuEvent::File_Open, [sceneManager](){ 
		sceneManager->OpenScene(); 
					  });

    debugLogSystem->LOG_INFO("EngineContextの初期化が完了しました");
}

void Engine::Shutdown(std::shared_ptr<EngineContext> context){

	context->Shutdown();

	typedef HRESULT(WINAPI* LPDXGIGetDebugInterface)(REFIID, void**);

	HMODULE dxgiDebugModule = LoadLibraryW(L"dxgidebug.dll");
	if(dxgiDebugModule){
		auto dxgiGetDebugInterface = reinterpret_cast<LPDXGIGetDebugInterface>(
			GetProcAddress(dxgiDebugModule, "DXGIGetDebugInterface"));

		if(dxgiGetDebugInterface){
			IDXGIDebug* dxgiDebug = nullptr;
			if(SUCCEEDED(dxgiGetDebugInterface(IID_PPV_ARGS(&dxgiDebug)))){
				dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_DETAIL);
				dxgiDebug->Release();
			}
		}

		FreeLibrary(dxgiDebugModule);
	}
}

void Engine::Run(std::shared_ptr<EngineContext> context){

	if (!context) {
		OutputDebugStringA("EngineContext が nullptr です。\n");
		return;
	}
	auto debugLogSystem = context->Get<DebugLogSystem>();
	if(!debugLogSystem){
		OutputDebugStringA("DebugLogSystem サービスの取得に失敗しました。\n");
		return;
	}
	debugLogSystem->LOG_DEBUG("Engineの実行を開始します...");

	auto windowService = context->Get<WindowService>();
	if (!windowService) {
		OutputDebugStringA("WindowService サービスの取得に失敗しました。\n");
		return;
	}
	auto timeService = context->Get<TimeService>();
	if (!timeService) {
		OutputDebugStringA("TimeService サービスの取得に失敗しました。\n");
		return;
	}
	auto graphicsContext = context->Get<GraphicsContext>();
	if (!graphicsContext) {
		OutputDebugStringA("GraphicsContext サービスの取得に失敗しました。\n");
		return;
	}
	auto inputService = context->Get<InputService>();
	if (!inputService) {
		OutputDebugStringA("InputService サービスの取得に失敗しました。\n");
		return;
	}
	auto imguiService = context->Get<ImGuiService>();
	if (!imguiService) {
		OutputDebugStringA("ImGuiService サービスの取得に失敗しました。\n");
		return;
	}
	auto mainRenderer = context->Get<MainRenderer>();
	if (!mainRenderer) {
		OutputDebugStringA("MainRenderer サービスの取得に失敗しました。\n");
		return;
	}
	auto sceneManager = context->Get<SceneManager>();
	if (!sceneManager) {
		OutputDebugStringA("SceneManager サービスの取得に失敗しました。\n");
		return;
	}
	debugLogSystem->LOG_DEBUG("EngineContextの取得が完了しました");


	// 最初のシーンを作成・ロード
	auto initialScene = std::make_shared<Scene>();
	sceneManager->LoadScene(initialScene);

	while(!windowService->GetMainWindow()->ShouldClose()){
		timeService->Tick();
		float dt = timeService->GetDeltaTime();

		{	// Update

			windowService->PollEvents();
			inputService->Update();
			sceneManager->Update(dt);
		}
		timeService->EndDeltaUpdate();

		{	// FixedUpdate

			while(timeService->ShouldRunFixedUpdate()){
				sceneManager->FixedUpdate(timeService->GetFixedDeltaTime());
				timeService->EndFixedUpdate();
			}
		}

		if (windowService->GetMainWindow()->ShouldClose()) {
			break;
		}
		
		{	// Render

			mainRenderer->BeginFrame();
			imguiService->Begin();

			{
				double Update = timeService->GetDeltaUpdateTime();
				double Draw = timeService->GetDrawTime();
				imguiService->DrawDebugImGuiWindow(Update * 1000.0f,Draw * 1000.0f, timeService->GetFixedUpdateFPS(),timeService->GetDeltaFPS());
			}
			sceneManager->Render();

			debugLogSystem->Draw();
			imguiService->End();
			mainRenderer->EndFrame(0);
		}
		timeService->EndDraw();
	}
}
