#include "ImGuiSystem.h"

#include "Backends/ImGui/imgui.h"
#include "Backends/ImGui/imgui_impl_win32.h"
#include "Backends/ImGui/imgui_impl_dx11.h"

#include "Engine/Graphics/GraphicsContext.h"
#include "Engine/Platform/WindowSystem/MainWindow.h"



bool ImGuiSystem::Initialize(IWindow* window, GraphicsContext* graphics){

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	//ImGuiIO& io = ImGui::GetIO();
	//io.Fonts->AddFontFromFileTTF("Asset\\Fonts\\NotoSansJP-Regular.ttf", 20.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	ImGui::StyleColorsDark();


	HWND hwnd = window->GetHWND();

	ImGui_ImplWin32_Init(hwnd);

	initialized_ = ImGui_ImplDX11_Init(
		graphics->GetDevice(),
		graphics->GetContext()
	);

	return initialized_;
}

void ImGuiSystem::Begin(){
	if(!initialized_){
		return;
	}

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void ImGuiSystem::End(){
	if(!initialized_){
		return;
	}
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiSystem::OnResize(){
	if(!initialized_) return;
	ImGui_ImplDX11_InvalidateDeviceObjects();
	ImGui_ImplDX11_CreateDeviceObjects();
}
