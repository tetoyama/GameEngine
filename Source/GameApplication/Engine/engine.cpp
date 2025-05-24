#include "engine.h"
#include "engineContext.h"

#include "Backends/ImGui/imgui.h"

#include "Platform/WindowSystem/windowSystem.h"

#include "Runtime/TimeService/time.h"

#include "Graphics/GraphicsContext.h"
#include "Graphics/mainRenderer.h"

#include "DebugTools/ImGuiSystem.h"

#include "Platform/InputSystem/InputSystem.h"

#include "Scene/sceneManager.h"
#include "Scene/scene.h"

void Engine::Initialize(std::shared_ptr<EngineContext> context, HINSTANCE hInstance, int nCmdShow){
	if (!context) {
		OutputDebugStringA("EngineContext が nullptr です。\n");
		return;
	}
	// WindowsSystemの取得
	auto windowSystem = context->Get<WindowSystem>();
	if (!windowSystem) {
		OutputDebugStringA("WindowSystem サービスの取得に失敗しました。\n");
		return;
	}
	// WindowSystemの初期化
	if (!windowSystem->Initialize(hInstance, nCmdShow)) {
		OutputDebugStringA("WindowSystem の初期化に失敗しました。\n");
		return;
	}

	//各種サービスの初期化

	// timeServiceの取得
	auto time = context->Get<TimeService>();
	if (!time) {
		OutputDebugStringA("TimeService サービスの取得に失敗しました。\n");
		return;
	}
	// timeServiceの初期化
	time->Initialize();

	// GraphicsContextの取得
	auto graphicsContext = context->Get<GraphicsContext>();
	if (!graphicsContext) {
		OutputDebugStringA("GraphicsContext サービスの取得に失敗しました。\n");
		return;
	}
	// GraphicsContextの初期化
	if (!graphicsContext->Initialize(windowSystem->GetMainWindow()->GetHWND(), DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT)) {
		OutputDebugStringA("GraphicsContext の初期化に失敗しました。\n");
		return;
	}

	// InputSystemの取得
	auto inputSystem = context->Get<InputSystem>();
	if (!inputSystem) {
		OutputDebugStringA("InputSystem サービスの取得に失敗しました。\n");
		return;
	}
	// InputSystemの初期化
	inputSystem->Initialize(windowSystem->GetMainWindow()->GetHWND());

	// ImGuiSystemの取得
	auto imgui = context->Get<ImGuiSystem>();
	if (!imgui) {
		OutputDebugStringA("ImGuiSystem サービスの取得に失敗しました。\n");
		return;
	}
	// ImGuiSystemの初期化
	if (!imgui->Initialize(windowSystem->GetMainWindow().get(), graphicsContext.get())) {
		OutputDebugStringA("ImGuiSystem の初期化に失敗しました。\n");
		return;
	}

	// MainRendererの取得
	auto mainRenderer = context->Get<MainRenderer>();
	if (!mainRenderer) {
		OutputDebugStringA("MainRenderer サービスの取得に失敗しました。\n");
		return;
	}
	// MainRendererの初期化
	mainRenderer->Initialize(graphicsContext.get(), windowSystem->GetMainWindow().get());

	// SceneManagerの取得
	auto sceneManager = context->Get<SceneManager>();
	if (!sceneManager) {
		OutputDebugStringA("SceneManager サービスの取得に失敗しました。\n");
		return;
	}
	// SceneManagerの初期化
	sceneManager->Initialize(graphicsContext.get(), mainRenderer.get());

	// WindowSystemのメインウィンドウを取得
	auto mainWindow = dynamic_cast<MainWindow*>(windowSystem->GetMainWindow().get());
	if (mainWindow) {
		// 各サービスをメインウィンドウにセット
		mainWindow->SetMainRenderer(mainRenderer.get());
		mainWindow->SetImGuiSystem(imgui.get());
		mainWindow->SetInputSystem(inputSystem.get());
	}
}

void Engine::Shutdown(std::shared_ptr<EngineContext> context){

	context->Shutdown();
}

void Engine::Run(std::shared_ptr<EngineContext> context){
	if (!context) {
		OutputDebugStringA("EngineContext が nullptr です。\n");
		return;
	}

	auto windowSystem = context->Get<WindowSystem>();
	if (!windowSystem) {
		OutputDebugStringA("WindowSystem サービスの取得に失敗しました。\n");
		return;
	}
	auto time = context->Get<TimeService>();
	if (!time) {
		OutputDebugStringA("TimeService サービスの取得に失敗しました。\n");
		return;
	}
	auto graphicsContext = context->Get<GraphicsContext>();
	if (!graphicsContext) {
		OutputDebugStringA("GraphicsContext サービスの取得に失敗しました。\n");
		return;
	}
	auto inputSystem = context->Get<InputSystem>();
	if (!inputSystem) {
		OutputDebugStringA("InputSystem サービスの取得に失敗しました。\n");
		return;
	}
	auto imgui = context->Get<ImGuiSystem>();
	if (!imgui) {
		OutputDebugStringA("ImGuiSystem サービスの取得に失敗しました。\n");
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

	// 最初のシーンを作成・ロード
	auto initialScene = std::make_shared<Scene>();
	sceneManager->LoadScene(initialScene);

	while(!windowSystem->GetMainWindow()->ShouldClose()){

		windowSystem->PollEvents();
		time->Tick();
		inputSystem->Update();

		float dt = time->GetDeltaTime();
		sceneManager->Update(dt);

		while(time->ShouldRunFixedUpdate()){
			sceneManager->FixedUpdate(time->GetFixedDeltaTime());
		}
		
		// Render
		mainRenderer->BeginFrame();
		imgui->Begin();

		sceneManager->Render();

		 // Debug UI をここに書く
		ImGui::Begin("Debug");
		ImGui::Text("FPS: %.2f", 1.0f / time->GetDeltaTime());
		ImGui::Text("Time: %.2f", time->GetTotalTime());
		ImGui::End();

		ImGui::Begin("Input Debug");

		// キーボードの例: Aキー
		ImGui::Text("A Press %s", inputSystem->IsKey('A') ? "Yes" : "No");
		ImGui::Text("A Down: %s", inputSystem->IsKeyDown('A') ? "Yes" : "No");
		ImGui::Text("A Up: %s", inputSystem->IsKeyUp('A') ? "Yes" : "No");

		// マウス
		ImGui::Text("Mouse X: %d", inputSystem->GetMouseX());
		ImGui::Text("Mouse Y: %d", inputSystem->GetMouseY());
		ImGui::Text("Mouse Left: %s", inputSystem->IsMouseDown(0) ? "Down" : "Up");
		ImGui::Text("Mouse Right: %s", inputSystem->IsMouseDown(1) ? "Down" : "Up");
		ImGui::Text("Mouse Wheel: %d", inputSystem->GetMouseWheel());

		// ゲームパッド（例: 0番）
		ImGui::Text("Gamepad 0 Connected: %s", inputSystem->IsGamepadConnected(0) ? "Yes" : "No");
		if(inputSystem->IsGamepadConnected(0)){
			ImGui::Text("Gamepad 0 A Button: %s", inputSystem->GetGamepadButton(0, XINPUT_GAMEPAD_A) ? "Pressed" : "Released");
			POINT left = inputSystem->GetGamepadLeftStick(0);
			ImGui::Text("Left Stick: x=%ld y=%ld", left.x, left.y);
		}
		ImGui::End();

		mainRenderer->DrawText2D(L"Hello, Direct2D!", 32, 32, 32.0f, D2D1::ColorF(D2D1::ColorF::Yellow));

		imgui->End();
		mainRenderer->EndFrame();

	}
}
