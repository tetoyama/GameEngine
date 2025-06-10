#include "engine.h"
#include "engineContext.h"

#include "Platform/WindowSystem/windowSystem.h"

#include "Runtime/TimeService/time.h"

#include "Graphics/GraphicsContext.h"
#include "Graphics/mainRenderer.h"

#include "DebugTools/ImGuiSystem.h"
#include "DebugTools/DebugSystem.h"

#include "Platform/InputSystem/InputSystem.h"

#include "Scene/sceneManager.h"
#include "Scene/scene.h"

#include "Engine/Resources/resourceSystem.h"

#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")

void Engine::Initialize(std::shared_ptr<EngineContext> context, HINSTANCE hInstance, int nCmdShow){
	
	if (!context) {
		OutputDebugStringA("EngineContext が nullptr です。\n");
		return;
	}

	// DebugLogSystemの取得
	auto debugLogSystem = context->Get<DebugLogSystem>();
	if(!debugLogSystem){
		OutputDebugStringA("DebugLogSystem サービスの取得に失敗しました。\n");
		return;
	}
	debugLogSystem->Initialize();
	debugLogSystem->LOG_INFO(u8"DebugLogSystemが起動しました");
	

	// WindowServiceの取得
	auto windowService = context->Get<WindowService>();
	if (!windowService) {
		OutputDebugStringA("WindowService サービスの取得に失敗しました。\n");
		return;
	}
	// WindowServiceの初期化
	if (!windowService->Initialize(hInstance, nCmdShow)) {
		OutputDebugStringA("WindowService の初期化に失敗しました。\n");
		return;
	}
	debugLogSystem->LOG_DEBUG(u8"WindowServiceが正常に作成されました");

	//Services の初期化

	// TimeServiceの取得
	auto timeService = context->Get<TimeService>();
	if (!timeService) {
		OutputDebugStringA("TimeService サービスの取得に失敗しました。\n");
		return;
	}
	// TimeServiceの初期化
	timeService->Initialize();
	debugLogSystem->LOG_DEBUG(u8"TimeServiceが正常に作成されました");

	// Engine Runtime の初期化

	// GraphicsContextの取得
	auto graphicsContext = context->Get<GraphicsContext>();
	if (!graphicsContext) {
		OutputDebugStringA("GraphicsContext サービスの取得に失敗しました。\n");
		return;
	}
	// GraphicsContextの初期化
	if (!graphicsContext->Initialize(windowService->GetMainWindow()->GetHWND(), DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT)) {
		OutputDebugStringA("GraphicsContext の初期化に失敗しました。\n");
		return;
	}
	debugLogSystem->LOG_DEBUG(u8"GraphicsContextが正常に作成されました");

	// ResourceSystemの取得
	auto resourceService = context->Get<ResourceService>();
	if(!graphicsContext){
		OutputDebugStringA("ResourceService サービスの取得に失敗しました。\n");
		return;
	}
	// ResourceSystemの初期化
	resourceService->Initialize(graphicsContext.get());
	debugLogSystem->LOG_DEBUG(u8"ResourceServiceが正常に作成されました");

	// InputSystemの取得
	auto inputService = context->Get<InputService>();
	if (!inputService) {
		OutputDebugStringA("InputService サービスの取得に失敗しました。\n");
		return;
	}
	// InputSystemの初期化
	inputService->Initialize(windowService->GetMainWindow()->GetHWND());
	debugLogSystem->LOG_DEBUG(u8"InputServiceが正常に作成されました");

	// ImGuiSystemの取得
	auto imguiService = context->Get<ImGuiService>();
	if (!imguiService) {
		OutputDebugStringA("ImGuiService サービスの取得に失敗しました。\n");
		return;
	}
	// ImGuiSystemの初期化
	if (!imguiService->Initialize(windowService->GetMainWindow().get(), graphicsContext.get())) {
		OutputDebugStringA("ImGuiService の初期化に失敗しました。\n");
		return;
	}
	debugLogSystem->LOG_DEBUG(u8"ImGuiSystemが正常に作成されました");

	// MainRendererの取得
	auto mainRenderer = context->Get<MainRenderer>();
	if (!mainRenderer) {
		OutputDebugStringA("MainRenderer サービスの取得に失敗しました。\n");
		return;
	}
	// MainRendererの初期化
	mainRenderer->Initialize(graphicsContext.get(), windowService->GetMainWindow().get());
	debugLogSystem->LOG_DEBUG(u8"MainRendererが正常に作成されました");

	// WindowSystemのメインウィンドウを取得
	auto mainWindow = dynamic_cast<MainWindow*>(windowService->GetMainWindow().get());
	if (mainWindow) {
		// 各サービスをメインウィンドウにセット
		mainWindow->SetMainRenderer(mainRenderer.get());
		mainWindow->SetImGuiSystem(imguiService.get());
		mainWindow->SetInputSystem(inputService.get());
	}
	debugLogSystem->LOG_DEBUG(u8"MainWindowが正常に作成されました");

	// SceneManagerの取得
	auto sceneManager = context->Get<SceneManager>();
	if(!sceneManager){
		OutputDebugStringA("SceneManager サービスの取得に失敗しました。\n");
		return;
	} else{
		// SceneManagerの初期化
		SceneManagerContext sceneContext{};
		sceneContext.graphics = graphicsContext.get();
		sceneContext.renderer = mainRenderer.get();
		sceneContext.input = inputService.get();
		sceneContext.resource = resourceService.get();
		sceneContext.hwnd = mainRenderer->GetHWND();
		sceneContext.debug = debugLogSystem.get();
		sceneManager->Initialize(sceneContext);
	}
	debugLogSystem->LOG_DEBUG(u8"SceneManagerが正常に作成されました");

	debugLogSystem->LOG_INFO(u8"EngineContextの初期化が完了しました");

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
	debugLogSystem->LOG_DEBUG(u8"Engineの実行を開始します...");

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
	debugLogSystem->LOG_DEBUG(u8"EngineContextの取得が完了しました");


	// 最初のシーンを作成・ロード
	auto initialScene = std::make_shared<Scene>();
	sceneManager->LoadScene(initialScene);



	while(!windowService->GetMainWindow()->ShouldClose()){

		windowService->PollEvents();
		timeService->Tick();
		inputService->Update();

		float dt = timeService->GetDeltaTime();
		sceneManager->Update(dt);

		while(timeService->ShouldRunFixedUpdate()){
			sceneManager->FixedUpdate(timeService->GetFixedDeltaTime());
		}
		
		// Render
		mainRenderer->BeginFrame();
		imguiService->Begin();

		sceneManager->Render();

		 // Debug UI をここに書く
		ImGui::Begin("Debug");
		{
			ImGui::Text("FPS: %.2f", 1.0f / timeService->GetDeltaTime());
			ImGui::Text("Time: %.2f", timeService->GetTotalTime());
		}
		ImGui::End();

		ImGui::Begin("Input Debug");

		auto mainWindowHWND = windowService->GetMainWindow()->GetHWND();

		// キーボードの例: Aキー
		ImGui::Text("A: %s", inputService->IsKey(mainWindowHWND, 'A') ? "Down" : "Up");
		ImGui::Text("A Down: %s", inputService->IsKeyDown(mainWindowHWND, 'A') ? "Yes" : "No");
		ImGui::Text("A Up: %s", inputService->IsKeyUp(mainWindowHWND, 'A') ? "Yes" : "No");

		// マウス
		ImGui::Text("Mouse X: %d", inputService->GetMouseX(mainWindowHWND));
		ImGui::Text("Mouse Y: %d", inputService->GetMouseY(mainWindowHWND));
		ImGui::Text("Mouse Left: %s", inputService->IsMouseDown(mainWindowHWND, 0) ? "Down" : "Up");
		ImGui::Text("Mouse Right: %s", inputService->IsMouseDown(mainWindowHWND, 1) ? "Down" : "Up");
		ImGui::Text("Mouse Wheel: %d", inputService->GetMouseWheel(mainWindowHWND));


		// ゲームパッド（例: 0番）
		ImGui::Text("Gamepad 0 Connected: %s", inputService->IsGamepadConnected(0) ? "Yes" : "No");
		if(inputService->IsGamepadConnected(0)){
			ImGui::Text("Gamepad 0 A Button: %s", inputService->GetGamepadButton(0, XINPUT_GAMEPAD_A) ? "Pressed" : "Released");
			POINT left = inputService->GetGamepadLeftStick(0);
			ImGui::Text("Left Stick: x=%ld y=%ld", left.x, left.y);
		}
		ImGui::End();

		mainRenderer->DrawText2D(L"Hello World!", 32, 32, 32.0f, D2D1::ColorF(D2D1::ColorF::Yellow));

		debugLogSystem->Draw();
		imguiService->End();
		mainRenderer->EndFrame(false);
	}
}
