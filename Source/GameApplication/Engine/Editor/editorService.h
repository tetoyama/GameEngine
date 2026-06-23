// =======================================================================
// 
// editorService.h
// 
// =======================================================================
#pragma once
#include "Service/IService.h"
#include <cstdint>
#include <vector>
#include "InterFace/IEditorUI.h"
#include "Command/CommandManager.h"
#include "Runtime/TimeService/timeService.h"
#include "Graphics/GpuFrameTiming.h"

struct EditorDrawContext;
class EditorService;

class ResourceService;
class DebugLogService;
class SceneManager;
class LLAMAService;

class AnalyzerManager;

struct SceneManagerContext;

// 前回完了したEditor Panel描画のCPU時間。
struct EditorPanelTiming {
	const char* name = nullptr;
	double seconds = 0.0;
};

// エディターサービスの初期化に必要な依存サービスをまとめた構造体
struct EditorServiceContext {
	DebugLogService* debugLogSystem = nullptr;
	ResourceService* resourceService = nullptr;
	SceneManager*    sceneManager    = nullptr;
	LLAMAService*    llamaService    = nullptr;
};

class EditorService : public IService {
public:
	void Initialize(EditorServiceContext context);
	void Draw(EditorDrawContext ctx);
	void Shutdown() override;

	template<typename T>
	T* GetUI() {
		static_assert(std::is_base_of<IEditorUI, T>::value, "T must inherit from IEditorUI");
		for (auto& panel : UIs) {
			if (auto p = dynamic_cast<T*>(panel.ui)) {
				return p;
			}
		}
		return nullptr;
	}

	DebugLogService* debugLogSystem = nullptr;
	ResourceService* resourceService = nullptr;
	SceneManager*    sceneManager    = nullptr;
	LLAMAService*    llamaService    = nullptr;
	AnalyzerManager* analyzer = nullptr;
	CommandManager commandManager;

private:
	struct EditorUIPanelEntry {
		const char* name = nullptr;
		IEditorUI* ui = nullptr;
	};

	std::vector<EditorUIPanelEntry> UIs;
	std::vector<EditorPanelTiming> m_CurrentPanelTimings;
	std::vector<EditorPanelTiming> m_CompletedPanelTimings;
};

struct EditorDrawContext {
	double UpdateTime = 0.0;
	double DrawTime = 0.0;
	double FPS = 0.0;
	double FixedUpdateFPS = 0.0;
	DrawTimingBreakdown DrawTiming{};
	const std::vector<EditorPanelTiming>* EditorPanelTimings = nullptr;
	const std::vector<GpuFrameTimingResult>* ResolvedGpuFrameTimings = nullptr;
	bool VSyncEnabled = false;
	bool TearingSupported = false;
	bool FrameLatencyWaitableObjectEnabled = false;
	uint64_t FrameLatencyWaitTimeoutCount = 0;

	// MainRendererのResize実績。serialが変化したフレームをResize直後として扱う。
	uint64_t ResizeSerial = 0;
	double LastResizeCpuTime = 0.0;
};
