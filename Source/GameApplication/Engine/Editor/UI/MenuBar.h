// =======================================================================
// 
// MenuBar.h
// 
// =======================================================================
#pragma once
#include "buildSetting.h"
#include <string>
#include <unordered_map>
#include <functional>

#include "Editor/editorService.h"
#include "Editor/InterFace/IEditorUI.h"

#ifdef _DEBUG_BUILD
#define IMGUI_SHOW_DEFAULT (true)
#else
#define IMGUI_SHOW_DEFAULT (false)
#endif // _DEBUG

enum class MenuEvent
{
	File_New,
	File_Open,
	File_Save,
	File_Exit,
	Edit_Undo,
	Edit_Redo,
	// 必要に応じて追加
};

// メインメニューバーUI
class MenuBar : public IEditorUI {
public:
	MenuBar() {}
	~MenuBar() {}
	void Initialize(EditorService* editor) override { m_editor = editor; }
	void Finalize() override {}
	void Draw(const EditorDrawContext ctx) override;

	using Callback = std::function<void()>;

	void Register(MenuEvent event, const Callback& callback);
	void Invoke(MenuEvent event);

	bool showSceneHierarchy = IMGUI_SHOW_DEFAULT;
	bool showInspector = IMGUI_SHOW_DEFAULT;
	bool showConsole = IMGUI_SHOW_DEFAULT;
	bool showAssetsBrowser = IMGUI_SHOW_DEFAULT;
	bool showBRAIN = IMGUI_SHOW_DEFAULT;
	bool showEditorView = IMGUI_SHOW_DEFAULT;
	bool showPlayerView = IMGUI_SHOW_DEFAULT;
	bool showPerformanceMonitor = IMGUI_SHOW_DEFAULT;
	bool showSystemSetting = IMGUI_SHOW_DEFAULT;
	bool showCB41 = IMGUI_SHOW_DEFAULT;

private:
	EditorService* m_editor = nullptr;
	bool showMenuBar = IMGUI_SHOW_DEFAULT;

	std::unordered_map<MenuEvent, Callback> m_eventCallbacks;

	void RenderFileMenu();
	void RenderEditMenu();
};
