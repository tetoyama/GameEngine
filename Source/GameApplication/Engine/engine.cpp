#include "buildSetting.h"
#include "engine.h"
#include "engineContext.h"

#include <string>

#include "Platform/WindowSystem/windowSystem.h"
#include "Taskbar/taskbar.h"
#include "Runtime/TimeService/timeService.h"
#include "Graphics/GraphicsContext.h"
#include "Graphics/mainRenderer.h"
#include "Graphics/RHI/RHIService.h"
#include "Graphics/RHI/D3D11/D3D11GraphicsContextInterop.h"
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
#include "Audio/audioContext.h"

namespace {
	bool FailInitialize(DebugLogService* debug, const char* reason){
		std::string message = "Engine::Initialize failed: ";
		message += reason ? reason : "unknown error";

		if(debug){
			debug->Error(message, "Engine::Initialize");
		}

		message += "\n";
		OutputDebugStringA(message.c_str());
		return false;
	}

	void LogRunFailure(DebugLogService* debug, const char* reason){
		std::string message = "Engine::Run aborted: ";
		message += reason ? reason : "unknown error";

		if(debug){
			debug->Error(message, "Engine::Run");
		}

		message += "\n";
		OutputDebugStringA(message.c_str());
	}
}

bool Engine::Initialize(EngineContext* context, HINSTANCE hInstance, int nCmdShow){
	if(!context) return FailInitialize(nullptr, "EngineContext is null");

	auto debug = context->Get<DebugLogService>();
	if(!debug) return FailInitialize(nullptr, "DebugLogService is not registered");
	debug->Initialize();

	auto config = context->Get<ConfigService>();
	auto rhi = context->Get<RHI::RenderHardwareInterfaceService>();
	auto window = context->Get<WindowService>();
	auto time = context->Get<TimeService>();
	auto audio = context->Get<AudioContext>();
	auto graphics = context->Get<GraphicsContext>();
	auto renderer = context->Get<MainRenderer>();
	auto input = context->Get<InputService>();
	auto resources = context->Get<ResourceService>();
	auto scenes = context->Get<SceneManager>();
	auto imgui = context->Get<ImGuiService>();
#ifdef _EDITOR
	auto editor = context->Get<EditorService>();
#else
	ServiceRef<EditorService> editor{};
#endif
	auto llama = context->Get<LLAMAService>();

	if(!config) return FailInitialize(debug.get(), "ConfigService is not registered");
	if(!rhi) return FailInitialize(debug.get(), "RenderHardwareInterfaceService is not registered");
	if(!window) return FailInitialize(debug.get(), "WindowService is not registered");
	if(!time) return FailInitialize(debug.get(), "TimeService is not registered");
	if(!audio) return FailInitialize(debug.get(), "AudioContext is not registered");
	if(!graphics) return FailInitialize(debug.get(), "GraphicsContext is not registered");
	if(!renderer) return FailInitialize(debug.get(), "MainRenderer is not registered");
	if(!input) return FailInitialize(debug.get(), "InputService is not registered");
	if(!resources) return FailInitialize(debug.get(), "ResourceService is not registered");
	if(!scenes) return FailInitialize(debug.get(), "SceneManager is not registered");
	if(!imgui) return FailInitialize(debug.get(), "ImGuiService is not registered");
	if(!llama) return FailInitialize(debug.get(), "LLAMAService is not registered");

	if(!config->Initialize()){
		return FailInitialize(debug.get(), "ConfigService::Initialize returned false");
	}
	if(!rhi->SelectBackend(config->engineConfig.graphics.backend)){
		return FailInitialize(debug.get(), "RHI backend selection failed");
	}
	if(!window->Initialize(hInstance, nCmdShow, config->appConfig)){
		return FailInitialize(debug.get(), "WindowService::Initialize returned false");
	}

	auto mainWindow = window->GetMainWindow();
	if(!mainWindow){
		return FailInitialize(debug.get(), "WindowService returned null main window");
	}
	InitTaskBar(mainWindow->GetHWND());

	time->Initialize();
	audio->Initialize();
	if(!graphics->Initialize(
		mainWindow->GetHWND(), mainWindow->GetWidth(), mainWindow->GetHeight())){
		return FailInitialize(debug.get(), "GraphicsContext::Initialize returned false");
	}

	if(rhi->GetSelectedBackend() == RHI::BackendType::Direct3D11 &&
		!RHI::EnsureGraphicsContextRHIDevice(*graphics.get())){
		return FailInitialize(debug.get(), "D3D11 RHI device binding failed");
	}

	graphics->SetMaximumFrameLatency(
		static_cast<UINT>(config->engineConfig.graphics.maximumFrameLatency)
	);

	resources->Initialize(graphics.get(), audio.get(), debug.get());
	input->Initialize(mainWindow->GetHWND());
	if(!imgui->Initialize(mainWindow.get(), graphics.get())){
		return FailInitialize(debug.get(), "ImGuiService::Initialize returned false");
	}
	renderer->Initialize(graphics.get(), mainWindow.get());

	mainWindow->SetMainRenderer(renderer.get());
	mainWindow->SetImGuiSystem(imgui.get());
	mainWindow->SetInputSystem(input.get());

	if(editor){
		EditorServiceContext editorContext{};
		editorContext.debugLogSystem = debug.get();
		editorContext.llamaService = llama.get();
		editorContext.resourceService = resources.get();
		editorContext.sceneManager = scenes.get();
		editor->Initialize(editorContext);
	}

	{
		SceneManagerContext managerContext{};
		managerContext.audio = audio.get();
		managerContext.graphics = graphics.get();
		managerContext.renderer = renderer.get();
		managerContext.input = input.get();
		managerContext.resource = resources.get();
		managerContext.hwnd = renderer->GetHWND();
		managerContext.debug = debug.get();
		managerContext.imgui = imgui.get();
		managerContext.sceneManager = scenes.get();
		managerContext.config = config.get();
		managerContext.editor = editor.get();
		scenes->Initialize(managerContext);
	}

	if(editor){
		auto menu = editor->GetUI<MenuBar>();
		if(menu){
			menu->Register(MenuEvent::File_Exit, [window](){
				window->GetMainWindow()->Close();
			});
			menu->Register(MenuEvent::File_New, [scenes](){
				scenes->AddScene(std::make_shared<Scene>());
			});
			menu->Register(MenuEvent::File_Save, [scenes](){ scenes->SaveScenes(); });
			menu->Register(MenuEvent::File_Open, [scenes](){
				scenes->AddScene(scenes->OpenFromYAMLFile());
			});
			menu->Register(MenuEvent::Edit_Undo, [editor](){ editor->commandManager.Undo(); });
			menu->Register(MenuEvent::Edit_Redo, [editor](){ editor->commandManager.Redo(); });
		}
	}

	LLAMAServiceContext llamaContext{};
	llamaContext.resourceService = resources.get();
	llama->Initialize(llamaContext);

	return true;
}

void Engine::Shutdown(EngineContext* context){
	if(context) context->Shutdown();
}

void Engine::Run(EngineContext* context){
	if(!context) return;

	auto debug = context->Get<DebugLogService>();
	auto window = context->Get<WindowService>();
	auto config = context->Get<ConfigService>();
	auto time = context->Get<TimeService>();
	auto input = context->Get<InputService>();
	auto imgui = context->Get<ImGuiService>();
	auto graphics = context->Get<GraphicsContext>();
	auto renderer = context->Get<MainRenderer>();
	auto scenes = context->Get<SceneManager>();
#ifdef _EDITOR
	auto editor = context->Get<EditorService>();
#else
	ServiceRef<EditorService> editor{};
#endif

	if(!debug) return LogRunFailure(nullptr, "DebugLogService is not registered");
	if(!window) return LogRunFailure(debug.get(), "WindowService is not registered");
	if(!config) return LogRunFailure(debug.get(), "ConfigService is not registered");
	if(!time) return LogRunFailure(debug.get(), "TimeService is not registered");
	if(!input) return LogRunFailure(debug.get(), "InputService is not registered");
	if(!imgui) return LogRunFailure(debug.get(), "ImGuiService is not registered");
	if(!graphics) return LogRunFailure(debug.get(), "GraphicsContext is not registered");
	if(!renderer) return LogRunFailure(debug.get(), "MainRenderer is not registered");
	if(!scenes) return LogRunFailure(debug.get(), "SceneManager is not registered");

	auto mainWindow = window->GetMainWindow();
	if(!mainWindow){
		LogRunFailure(debug.get(), "WindowService returned null main window");
		return;
	}

	uint64_t drawFrameSerial = 0;
	uint64_t completedResizeSerial = 0;
	double completedResizeCpuTime = 0.0;

	// GameApplication may preload a persistent Multi-Scene composition before Run.
	// Only create the historical default scene when no scene has been supplied.
	if(scenes->GetActiveScenes().empty()){
		auto initialScene = std::make_shared<Scene>();
#ifdef _DEBUG_BUILD
		scenes->LoadScene(initialScene);
#else
		scenes->LoadFromFilePath(config->appConfig.startSceneFilePath);
#endif
	}

	while(!mainWindow->ShouldClose()){
		time->Tick();
		while(time->ShouldRunFixedUpdate()){
			scenes->FixedUpdate(time->GetFixedDeltaTime());
			time->EndFixedUpdate();
		}

		time->BeginDeltaUpdate();
		window->PollEvents();
		input->Update();
		scenes->Update(time->GetDeltaTime());
		time->EndDeltaUpdate();
		if(mainWindow->ShouldClose()) break;

		const uint64_t activeFrameSerial = ++drawFrameSerial;
		const uint64_t activeResizeSerial = renderer->GetResizeSerial();
		const double activeResizeCpuTime =
			renderer->GetLastResizeCpuTimeSeconds();
		time->BeginDraw(activeFrameSerial);

		time->BeginDrawSection(DrawTimingSection::FramePacingWait);
		graphics->WaitForFrameLatency();
		time->EndDrawSection(DrawTimingSection::FramePacingWait);

		GpuPassTimingProfiler& gpuPassTiming =
			renderer->GetGpuPassTimingProfiler();
		gpuPassTiming.BeginFrame(
			graphics->GetDevice(),
			graphics->GetDeviceContext(),
			activeFrameSerial
		);
		const auto resolvedGpuFrameTimings =
			gpuPassTiming.ConsumeResolved(graphics->GetDeviceContext());

		time->BeginDrawSection(DrawTimingSection::FrameSetup);
		renderer->BeginFrame();
		time->EndDrawSection(DrawTimingSection::FrameSetup);

		time->BeginDrawSection(DrawTimingSection::ImGuiBegin);
		imgui->Begin();
		if(editor) ImGui::SetCommandManager(&editor->commandManager);
		time->EndDrawSection(DrawTimingSection::ImGuiBegin);

		time->BeginDrawSection(DrawTimingSection::RenderSchedule);
		scenes->Draw();
		time->EndDrawSection(DrawTimingSection::RenderSchedule);

		time->BeginDrawSection(DrawTimingSection::DebugDraw);
		if(debug) debug->Draw();
		time->EndDrawSection(DrawTimingSection::DebugDraw);

		if(editor){
			EditorDrawContext draw{};
			draw.DrawTiming = time->GetDrawTimingBreakdown();
			draw.UpdateTime = draw.DrawTiming.update;
			draw.DrawTime = draw.DrawTiming.total;
			draw.FPS = time->GetFrameFPS();
			draw.FixedUpdateFPS = time->GetFixedUpdateFPS();
			draw.ResolvedGpuFrameTimings = &resolvedGpuFrameTimings;
			draw.VSyncEnabled = config->appConfig.Vsync;
			draw.TearingSupported = graphics->IsTearingSupported();
			draw.FrameLatencyWaitableObjectEnabled = graphics->IsFrameLatencyWaitableObjectEnabled();
			draw.FrameLatencyWaitTimeoutCount = graphics->GetFrameLatencyWaitTimeoutCount();
			draw.ResizeSerial = completedResizeSerial;
			draw.LastResizeCpuTime = completedResizeCpuTime;

			time->BeginDrawSection(DrawTimingSection::EditorUIBuild);
			editor->Draw(draw);
			time->EndDrawSection(DrawTimingSection::EditorUIBuild);
		}

		time->BeginDrawSection(DrawTimingSection::ImGuiRender);
		{
			ScopedGpuPassTiming gpuTiming(
				gpuPassTiming,
				graphics->GetDeviceContext(),
				GpuPassTimingScope::ImGui
			);
			imgui->End();
		}
		time->EndDrawSection(DrawTimingSection::ImGuiRender);

		gpuPassTiming.EndFrame(graphics->GetDeviceContext());

		time->BeginDrawSection(DrawTimingSection::Present);
		renderer->EndFrame(config->appConfig.Vsync);
		time->EndDrawSection(DrawTimingSection::Present);

		time->EndDraw();
		completedResizeSerial = activeResizeSerial;
		completedResizeCpuTime = activeResizeCpuTime;

		// H2: デバイスロスト(TDR/ドライバ更新/GPU切替)を検出したら、
		// 無効なDevice/SwapChainを参照し続けてCrash/黒画面固定になる前に制御された終了へ移る。
		// (Device/SwapChain/全View/Query Poolの完全再生成は後続フェーズで対応)
		if(graphics->IsDeviceLost()){
			if(debug) debug->Error(
				"デバイスロストを検出したためGraceful終了します(H2)。",
				"Engine::Run"
			);
			break;
		}
	}
}
