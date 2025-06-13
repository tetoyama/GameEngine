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

	// Enable docking(available in imgui `docking` branch at the moment)
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
	//io.ConfigDockingWithShift = true;
	io.ConfigWindowsResizeFromEdges = true;;
	//io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	 
	
	//io.Fonts->AddFontFromFileTTF("Asset\\Fonts\\PixelMplus12-Bold.ttf", 15.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
	io.Fonts->AddFontFromFileTTF("Asset\\Fonts\\NotoSansJP-Regular.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	ImGui::StyleColorsDark();

	HWND hwnd = window->GetHWND();

	ImGui_ImplWin32_Init(hwnd);

	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 0.0f;
	style.Colors[ImGuiCol_WindowBg].w = 1.0f;

	initialized_ = ImGui_ImplDX11_Init(
		graphics->GetDevice(),
		graphics->GetDeviceContext()
	);

	return initialized_;
}

void ImGuiService::Shutdown() {

		//ImGuiの終了処理
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

	ImGui::DockSpaceOverViewport(0U,0, ImGuiDockNodeFlags_PassthruCentralNode);  // ドッキングスペースの設置







}

void ImGuiService::End(){
	if(!initialized_){
		return;
	}




	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
}

void ImGuiService::OnResize(){
	if(!initialized_) return;
	ImGui_ImplDX11_InvalidateDeviceObjects();
	ImGui_ImplDX11_CreateDeviceObjects();
}
