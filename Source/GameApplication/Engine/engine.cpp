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

void Engine::Initialize(std::shared_ptr<EngineContext> context, HINSTANCE hInstance, int nCmdShow){

    if (!context) {
        OutputDebugStringA("EngineContext が nullptr です。\n");
        return;
    }
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

	if(debugLogSystem){
		debugLogSystem->Initialize();
	}

	if(!configSystem || !configSystem->Initialize()){
		OutputDebugStringA("configSystem の初期化に失敗しました。\n");
	}

    if (!windowService || !windowService->Initialize(hInstance, nCmdShow,configSystem->appConfig)) {
        OutputDebugStringA("WindowService の初期化に失敗しました。\n");
    }
	auto mainWindow = windowService->GetMainWindow();

	InitTaskBar(windowService->GetMainWindow()->GetHWND());

	if(timeService){
		timeService->Initialize();
	}

	if(!audioContext || !audioContext->Initialize()){
		OutputDebugStringA("AudioContext の初期化に失敗しました。\n");
	}

    if (!graphicsContext || 
        !graphicsContext->Initialize(windowService->GetMainWindow()->GetHWND(), mainWindow->GetWidth(), mainWindow->GetHeight())){
        OutputDebugStringA("GraphicsContext の初期化に失敗しました。\n");
    }

	if(resourceService){
		resourceService->Initialize(graphicsContext.get(), audioContext.get());
	}

	if(inputService){
		inputService->Initialize(windowService->GetMainWindow()->GetHWND());
	}
    if (!imguiService || 
        !imguiService->Initialize(windowService->GetMainWindow().get(), graphicsContext.get())) {
        OutputDebugStringA("ImGuiService の初期化に失敗しました。\n");
    }

	if(mainRenderer){
		mainRenderer->Initialize(graphicsContext.get(), windowService->GetMainWindow().get());
	}

    // メインウィンドウに各サービスを紐付け
    if (mainWindow) {
        mainWindow->SetMainRenderer(mainRenderer.get());
        mainWindow->SetImGuiSystem(imguiService.get());
        mainWindow->SetInputSystem(inputService.get());
    }

	if(editorService){
		EditorServiceContext editorContext;
		editorContext.debugLogSystem = debugLogSystem.get();
		editorContext.llamaService = llamaService.get();
		editorContext.resourceService = resourceService.get();
		editorContext.sceneManager = sceneManager.get();

		editorService->Initialize(editorContext);
	}

    // シーンマネージャ初期化
	if(sceneManager){
		// シーンマネージャコンテキストの設定
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
		// SceneManagerの初期化
		sceneManager->Initialize(sceneManagerContext);
	}

    // ImGuiメニューバー操作にイベントを登録
	if(editorService){
		auto menubar = editorService->GetUI<MenuBar>();
		if(menubar){
			menubar->Register(MenuEvent::File_Exit, [windowService](){
				windowService->GetMainWindow()->Close();
							  });
			menubar->Register(MenuEvent::File_New, [sceneManager](){
				sceneManager->AddScene(std::make_shared<Scene>());
							  });
			menubar->Register(MenuEvent::File_Save, [sceneManager](){
				sceneManager->SaveScenes();
							  });
			menubar->Register(MenuEvent::File_Open, [sceneManager](){
				sceneManager->AddScene(sceneManager->OpenFromYAMLFile());
							  });
			// アンドゥ／リドゥを CommandManager に接続
			menubar->Register(MenuEvent::Edit_Undo, [editorService](){
				editorService->commandManager.Undo();
			});
			menubar->Register(MenuEvent::Edit_Redo, [editorService](){
				editorService->commandManager.Redo();
			});
		}
	}

	// LLAMAサービス初期化
	if (llamaService) {

		LLAMAServiceContext llamaContext{};
		llamaContext.resourceService = resourceService.get();

		llamaService->Initialize(llamaContext);
	}

    debugLogSystem->LOG_INFO("EngineContextの初期化が完了しました");
}

void Engine::Shutdown(std::shared_ptr<EngineContext> context){

	context->Shutdown();

	// デバッグ出力システムの終了
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
	auto debugLogSystem = context->Get<DebugLogService>();

	debugLogSystem->LOG_DEBUG("Engineの実行を開始します...");

	auto windowService = context->Get<WindowService>();
	auto configSystem = context->Get<ConfigService>();
	auto timeService = context->Get<TimeService>();
	auto graphicsContext = context->Get<GraphicsContext>();
	auto inputService = context->Get<InputService>();
	auto imguiService = context->Get<ImGuiService>();
	auto mainRenderer = context->Get<MainRenderer>();
	auto sceneManager = context->Get<SceneManager>();
	auto editorService = context->Get<EditorService>();

	debugLogSystem->LOG_DEBUG("EngineContextの取得が完了しました");

	// 最初のシーンを作成・ロード
	auto initialScene = std::make_shared<Scene>();
#ifdef _DEBUG_BUILD
	sceneManager->LoadScene(initialScene);
#else
	sceneManager->LoadFromFilePath(configSystem->appConfig.startSceneFilePath);
#endif // _DEBUG

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
			// ウィンドウが閉じられた場合はループを抜ける
			break;
		}
		
		{	// Draw
			mainRenderer->BeginFrame();
			imguiService->Begin();
			{
				// アンドゥ対応 ImGui ラッパーに CommandManager を設定
				if (editorService) {
					ImGui::SetCommandManager(&editorService->commandManager);
				}

				sceneManager->Draw();
				debugLogSystem->Draw();

				if(editorService){
					EditorDrawContext editorDrawContext{};
					editorDrawContext.UpdateTime = timeService->GetDeltaUpdateTime();
					editorDrawContext.DrawTime = timeService->GetDrawTime();
					editorDrawContext.FPS = timeService->GetDeltaFPS();
					editorDrawContext.FixedUpdateFPS = timeService->GetFixedUpdateFPS();
					editorService->Draw(editorDrawContext);
				}
			}
			imguiService->End();
			mainRenderer->EndFrame(configSystem->appConfig.Vsync);
		}
		timeService->EndDraw();
	}
	debugLogSystem->LOG_DEBUG("Engineを終了します");
}
