#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

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

class ImGuiManubar
{
public:
	using Callback = std::function<void()>;

	void Register(MenuEvent event, const Callback& callback);
	void Render(); // MainMenuBar を表示する
	void Invoke(MenuEvent event);

private:
	std::unordered_map<MenuEvent, Callback> m_eventCallbacks;

	void RenderFileMenu();
	void RenderEditMenu();


};