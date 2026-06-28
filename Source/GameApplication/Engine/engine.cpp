#include "buildSetting.h"
#include "engine.h"
#include "engineContext.h"

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

void Engine::Initialize(EngineContext* context, HINSTANCE hInstance, int nCmdShow){
	if(!context) return;

	auto debug = context->Get<DebugLogService>();
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
	auto editor = context->Get<EditorService>();
	auto llama = context->Get<LLAMAService>();

	if(debug) debug->Initialize();
	if(!config || !config->Initialize()) return;
	if(!rhi || !rhi->SelectBackend(config->engineConfig.graphics.backend)) return;
	if(!window || !window->Initialize(hInstance, nCmdShow, config->appConfig)) return;

	auto mainWindow = window->GetMainWindow();
	if(!mainWindow) return;
	InitTaskBar(mainWindow->GetHWND());

	if(time) time->Initialize();
	if(audio) audio->Initialize();
	if(!graphics || !graphics->Initialize(
		mainWindow->GetHWND(), mainWindow->GetWidth(), mainWindow->GetHeight())) return;

	if(rhi->GetSelectedBackend() == RHI::BackendType::Direct3D11 &&
		!RHI::EnsureGraphicsContextRHIDevice(*graphics)){
		return;
	}

	graphics->SetMaximumFrameLatency(
		static_cast<UINT>(config->engineConfig.graphics.maximumFrameLatency)
	);

	if(resources){
		resources->Initialize(graphics.get(), audio.get(), debug.get());
	}
	if(input) input->Initialize(mainWindow->GetHWND());
	if(!imgui || !imgui->Initialize(mainWindow.get(), graphics.get())) return;
	if(renderer) renderer->Initialize(graphics.get(), mainWindow.get());

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

	if(scenes){
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

	if(llama){
		LLAMAServiceContext llamaContext{};
		llamaContext.resourceService = resources.get();
		llama->Initialize(llamaContext);
	}
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
	auto editor = context->Get<EditorService>();

	uint64_t drawFrameSerial = 0;
	uint64_t completedResizeSerial = 0;
	double completedResizeCpuTime = 0.0;

	auto initialScene = std::make_shared<Scene>();
#ifdef _DEBUG_BUILD
	scenes->LoadScene(initialScene);
#else
	scenes->LoadFromFilePath(config->appConfig.startSceneFilePath);
#endif

	while(!window->GetMainWindow()->ShouldClose()){
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
		if(window->GetMainWindow()->ShouldClose()) break;

		const uint64_t activeFrameSerial = ++drawFrameSerial;
		const uint64_t activeResizeSerial = renderer->GetResizeSerial();
		const double activeResizeCpuTime =
			renderer->GetLastResizeCpuTimeSeconds();
		time->BeginDraw(activeFrameSerial);

		time->BeginDrawSection(DrawTimingSection::FramePacingWait);
		graphics->WaitForFrameLatency();
		time->EndDrawSection(DrawTimingSection::FramePacingWait);

		graphics->BeginGpuFrameTiming(activeFrameSerial);
		const auto resolvedGpuFrameTimings =
			graphics->ConsumeResolvedGpuFrameTimings();

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
		imgui->End();
		time->EndDrawSection(DrawTimingSection::ImGuiRender);

		graphics->EndGpuFrameTiming();

		time->BeginDrawSection(DrawTimingSection::Present);
		renderer->EndFrame(config->appConfig.Vsync);
		time->EndDrawSection(DrawTimingSection::Present);

		time->EndDraw();
		completedResizeSerial = activeResizeSerial;
		completedResizeCpuTime = activeResizeCpuTime;
	}
}
