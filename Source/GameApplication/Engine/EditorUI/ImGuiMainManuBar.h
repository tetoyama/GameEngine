#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

#ifdef _DEBUG
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
class ImGuiService;

class ImGuiManubar
{
public:
	ImGuiManubar(){

	}

	using Callback = std::function<void()>;

	void Register(MenuEvent event, const Callback& callback);
	void Draw(); // MainMenuBar を表示する
	void Invoke(MenuEvent event);

	bool showSceneHierarchy = IMGUI_SHOW_DEFAULT;
	bool showInspector = IMGUI_SHOW_DEFAULT;
	bool showConsole = IMGUI_SHOW_DEFAULT;
	bool showAssetsBrowser = IMGUI_SHOW_DEFAULT;
	bool showEditorView = IMGUI_SHOW_DEFAULT;
	bool showPlayerView = IMGUI_SHOW_DEFAULT;
	bool showParformanceMonitor = IMGUI_SHOW_DEFAULT;

private:
	std::unordered_map<MenuEvent, Callback> m_eventCallbacks;

	void RenderFileMenu();
	void RenderEditMenu();

};