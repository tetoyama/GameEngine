// =======================================================================
// 
// ImGuiSystem.cpp
// 
// =======================================================================
#include <Windows.h>
#include <stdio.h>
#include <string>
#include "ImGuiSystem.h"

#include "Backends/ImGui/imgui.h"
#include "Backends/ImGui/imgui_impl_win32.h"
#include "Backends/ImGui/imgui_impl_dx11.h"
#include "Backends/ImGui/imguizmo.h"
#include "Backends/ImGui/imnodes.h"

#include "Editor/editorService.h"
#include "Editor/UI/MenuBar.h"
#include "Editor/UI/ModernImGui/ModernImGui.h"

#include "Graphics/GraphicsContext.h"
#include "Platform/WindowSystem/MainWindow.h"


#include "GameApplication.h"
#include "time.h"
#include <psapi.h>
#include <ImGui/imgui_internal.h>


#pragma comment(lib, "Psapi.lib")


bool ImGuiService::Initialize(IWindow* window, GraphicsContext* graphics){

	m_GraphicsContext = graphics;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImNodes::CreateContext();
	ImGuiIO& io = ImGui::GetIO();

	// Enable docking(available in imgui `docking` branch at the moment)
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
	io.ConfigWindowsResizeFromEdges = true;
	 
	
	//io.Fonts->AddFontFromFileTTF("Asset\\Fonts\\PixelMplus12-Bold.ttf", 15.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());

	// メイン日本語フォントを読み込み
	io.Fonts->AddFontFromFileTTF("Asset\\Font\\NotoSansJP-Regular.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());

	//// 絵文字用フォント設定（マージ用）
	//ImFontConfig emojiConfig;
	//emojiConfig.MergeMode = true;      // マージモードをONにする
	//emojiConfig.PixelSnapH = true;
	//emojiConfig.GlyphMinAdvanceX = 13.0f;  // 必要に応じて調整

	//// 絵文字のUnicode範囲（Noto EmojiやSegoe UI Emojiで使う）
	//// 例として絵文字の範囲は下記のように指定
	//static const ImWchar emoji_ranges[] = {
	//	0x1F300, 0x1F5FF,   // Misc Symbols and Pictographs
	//	0x1F600, 0x1F64F,   // Emoticons
	//	0x1F680, 0x1F6FF,   // Transport & Map Symbols
	//	0x2600,  0x26FF,    // Misc symbols
	//	0x2700,  0x27BF,    // Dingbats
	//	0,
	//};

	//io.Fonts->AddFontFromFileTTF("Asset\\Fonts\\NotoColorEmoji_WindowsCompatible.ttf", 16.0f, &emojiConfig, emoji_ranges);



	io.IniFilename = "Asset\\imgui.ini"; // デフォルトでimgui.iniに保存されます

	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	MImGui::ApplyTheme();
	HWND hwnd = window->GetHWND();

	ImGui_ImplWin32_Init(hwnd);

	initialized_ = ImGui_ImplDX11_Init(
		graphics->GetDevice(),
		graphics->GetDeviceContext()
	);




	return initialized_;
}

void ImGuiService::Shutdown() {

	ImNodes::DestroyContext();
	//ImGuiの終了処理
	{
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}
}

void ImGuiService::SetViewProjectionMatrix(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& projection){
	m_view = view;
	m_projection = projection;
}

DirectX::XMMATRIX ImGuiService::RenderGizmo(const DirectX::XMMATRIX& world) const{

	DirectX::XMFLOAT4X4 viewF, projF, modelF;
	DirectX::XMStoreFloat4x4(&viewF, m_view);
	DirectX::XMStoreFloat4x4(&projF, m_projection);
	DirectX::XMStoreFloat4x4(&modelF, world);

	float* viewPtr = &viewF.m[0][0];
	float* projPtr = &projF.m[0][0];
	float* modelPtr = &modelF.m[0][0];

	ImGuizmo::SetOrthographic(false); // true＝直交投影、false＝透視
	// ギズモの操作モードと操作タイプを設定
	ImGuizmo::Manipulate(viewPtr, projPtr, ImGuizmo::TRANSLATE | ImGuizmo::ROTATE | ImGuizmo::SCALE, ImGuizmo::LOCAL , modelPtr);

	return DirectX::XMLoadFloat4x4(&modelF);
}

DirectX::XMMATRIX ImGuiService::RenderGizmo2D(
	const DirectX::XMMATRIX& world,
	const DirectX::XMFLOAT2& vp
) const{
	ImGuiViewport* viewport = ImGui::GetMainViewport();

	// Ortho (UI座標：左上原点)
	float left = 0.0f;
	float right = vp.x;
	float top = 0.0f;
	float bottom = vp.y;
	float nearZ = 0.0f;
	float farZ = 1.0f;

	DirectX::XMMATRIX proj =
		DirectX::XMMatrixOrthographicOffCenterLH(
			left, right, bottom, top, nearZ, farZ);

	DirectX::XMMATRIX view = DirectX::XMMatrixIdentity();

	DirectX::XMFLOAT4X4 viewF, projF, modelF;
	DirectX::XMStoreFloat4x4(&viewF, view);
	DirectX::XMStoreFloat4x4(&projF, proj);
	DirectX::XMStoreFloat4x4(&modelF, world);

	ImGuizmo::SetOrthographic(true);

	ImGuizmo::Manipulate(
		&viewF.m[0][0],
		&projF.m[0][0],
		ImGuizmo::TRANSLATE | ImGuizmo::ROTATE_SCREEN | ImGuizmo::SCALE,
		ImGuizmo::LOCAL,
		&modelF.m[0][0]
	);

	return DirectX::XMLoadFloat4x4(&modelF);
}

void ImGuiService::Begin(){

	if(!initialized_){
		return;
	}

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	MImGui::BeginFrame();
	
	ImGuizmo::BeginFrame();

	//ImGuizmo::SetRect(ImGui::GetMainViewport()->Pos.x, ImGui::GetMainViewport()->Pos.y, ImGui::GetMainViewport()->Size.x, ImGui::GetMainViewport()->Size.y);

	ImGui::DockSpaceOverViewport(0U,0, ImGuiDockNodeFlags_PassthruCentralNode);  // ドッキングスペースの設置
}

void ImGuiService::End(){

	if(!initialized_){
		return;
	}
	ImGuiIO& io = ImGui::GetIO();

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	if(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable){
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
}

void ImGuiService::OnResize(){
	if(!initialized_) return;
	ImGui_ImplDX11_InvalidateDeviceObjects();
	ImGui_ImplDX11_CreateDeviceObjects();
}
