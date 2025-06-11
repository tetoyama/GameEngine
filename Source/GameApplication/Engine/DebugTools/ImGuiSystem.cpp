#include "ImGuiSystem.h"

#include "Backends/ImGui/imgui.h"
#include "Backends/ImGui/imgui_impl_win32.h"
#include "Backends/ImGui/imgui_impl_dx11.h"

#include "Engine/Graphics/GraphicsContext.h"
#include "Engine/Platform/WindowSystem/MainWindow.h"



bool ImGuiService::Initialize(IWindow* window, GraphicsContext* graphics){

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	//io.Fonts->AddFontFromFileTTF("Asset\\Fonts\\PixelMplus12-Bold.ttf", 15.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
	io.Fonts->AddFontFromFileTTF("Asset\\Fonts\\NotoSansJP-Regular.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	ImGui::StyleColorsDark();

	HWND hwnd = window->GetHWND();

	ImGui_ImplWin32_Init(hwnd);

	initialized_ = ImGui_ImplDX11_Init(
		graphics->GetDevice(),
		graphics->GetDeviceContext()
	);

	return initialized_;
}

void ImGuiService::Shutdown() {

		//ImGuiÇÃèIóπèàóù
	{
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}
}

void ImGuiService::Begin(){
	if(!initialized_){
		return;
	}

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void ImGuiService::End(){
	if(!initialized_){
		return;
	}
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiService::OnResize(){
	if(!initialized_) return;
	ImGui_ImplDX11_InvalidateDeviceObjects();
	ImGui_ImplDX11_CreateDeviceObjects();
}
