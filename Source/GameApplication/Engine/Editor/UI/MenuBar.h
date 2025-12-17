#pragma once
#include "buildSetting.h"
#include <string>
#include <unordered_map>
#include <vector>
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

class MenuBar : public IEditorUI {
public:
	MenuBar() {}
	~MenuBar() {}
	void Initialize(EditorService* editor) override {}
	void Finalize() override {}
	void Draw(const EditorDrawContext ctx) override;

	using Callback = std::function<void()>;

	void Register(MenuEvent event, const Callback& callback);
	void Invoke(MenuEvent event);

	bool showSceneHierarchy = IMGUI_SHOW_DEFAULT;
	bool showInspector = IMGUI_SHOW_DEFAULT;
	bool showConsole = IMGUI_SHOW_DEFAULT;
	bool showAssetsBrowser = IMGUI_SHOW_DEFAULT;
	bool showEditorView = IMGUI_SHOW_DEFAULT;
	bool showPlayerView = IMGUI_SHOW_DEFAULT;
	bool showParformanceMonitor = IMGUI_SHOW_DEFAULT;

private:
	bool showMenuBar = IMGUI_SHOW_DEFAULT;

	std::unordered_map<MenuEvent, Callback> m_eventCallbacks;

	void RenderFileMenu();
	void RenderEditMenu();
};
