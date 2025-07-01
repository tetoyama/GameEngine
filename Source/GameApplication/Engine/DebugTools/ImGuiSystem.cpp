#define _CRT_SECURE_NO_WARNINGS

#include "ImGuiSystem.h"

#include "Backends/ImGui/imgui.h"
#include "Backends/ImGui/imgui_impl_win32.h"
#include "Backends/ImGui/imgui_impl_dx11.h"
#include "Backends/ImGui/imguizmo.h"

#include "Engine/Graphics/GraphicsContext.h"
#include "Engine/Platform/WindowSystem/MainWindow.h"

#ifdef _DEBUG

#include "GameApplication/GameApplication.h"
#include "time.h"
#define SAMPLE_LENGTH (TARGET_FPS)

static float FixedFpsSamples[SAMPLE_LENGTH]{};
static float DeltaFpsSamples[SAMPLE_LENGTH]{};
static float UpdateSamples[SAMPLE_LENGTH]{};
static float DrawSamples[SAMPLE_LENGTH]{};
static int SampleCount = 0;
#endif

void SetModernStyle();
// UI状態管理
bool showSceneHierarchy = true;
bool showInspector = true;
bool showConsole = true;
bool showAssetsBrowser = true;

void DrawAssetsBrowser(){

}

bool ImGuiService::Initialize(IWindow* window, GraphicsContext* graphics){

	m_GraphicsContext = graphics;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();

	// Enable docking(available in imgui `docking` branch at the moment)
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
	io.ConfigWindowsResizeFromEdges = true;;
	 
	
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

	SetModernStyle();
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

DirectX::XMMATRIX ImGuiService::RenderGizmo2D(const DirectX::XMMATRIX& world) const{

	// ImGuiのメインビューポートを取得
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	float left = 0;
	float right = viewport->Size.x;
	float top = 0;
	float bottom = viewport->Size.y;
	float nearZ = -1.0f;
	float farZ = 1.0f;

	// 正射影行列をビューポートに合わせて作成
	DirectX::XMMATRIX ortho = DirectX::XMMatrixOrthographicOffCenterLH(left, right, bottom, top, nearZ, farZ);

	DirectX::XMFLOAT4X4 viewF, projF, modelF;
	DirectX::XMStoreFloat4x4(&viewF, DirectX::XMMatrixIdentity());
	DirectX::XMStoreFloat4x4(&projF, ortho);
	DirectX::XMStoreFloat4x4(&modelF, world);

	float* viewPtr = &viewF.m[0][0];
	float* projPtr = &projF.m[0][0];
	float* modelPtr = &modelF.m[0][0];

	ImGuizmo::SetOrthographic(true); // 2Dギズモのため直交投影を使用
	// ギズモの操作モードと操作タイプを設定
	ImGuizmo::Manipulate(viewPtr, projPtr, ImGuizmo::TRANSLATE | ImGuizmo::ROTATE | ImGuizmo::SCALE, ImGuizmo::LOCAL, modelPtr);

	return DirectX::XMLoadFloat4x4(&modelF);
}

void ImGuiService::Begin(){
	if(!initialized_){
		return;
	}

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGuizmo::BeginFrame();
	ImGuizmo::SetOrthographic(false); // true＝直交投影、false＝透視

	//ImGuizmo::SetRect(ImGui::GetMainViewport()->Pos.x, ImGui::GetMainViewport()->Pos.y, ImGui::GetMainViewport()->Size.x, ImGui::GetMainViewport()->Size.y);

	ImGui::DockSpaceOverViewport(0U,0, ImGuiDockNodeFlags_PassthruCentralNode);  // ドッキングスペースの設置
	manubar.Render();
	DrawAssetsBrowser();
}

void ImGuiService::End(){

	if(!initialized_){
		return;
	}


	ImGuiIO& io = ImGui::GetIO();


	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());


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

void ImGuiService::DrawDebugImGuiWindow(double Update, double Draw, double FPS, double DeltaFPS){

#ifdef _DEBUG
	SampleCount = (SampleCount + 1) % TARGET_FPS;
	if(SampleCount == 0){
		for(int n = 0; n < SAMPLE_LENGTH - 1; n++){
			FixedFpsSamples[n] = FixedFpsSamples[n + 1];
			DeltaFpsSamples[n] = DeltaFpsSamples[n + 1];
		}
		FixedFpsSamples[SAMPLE_LENGTH - 1] = (float)FPS;
		DeltaFpsSamples[SAMPLE_LENGTH - 1] = (float)DeltaFPS;
	}
	for(int n = 0; n < SAMPLE_LENGTH - 1; n++){
		UpdateSamples[n] = UpdateSamples[n + 1];
		DrawSamples[n] = DrawSamples[n + 1];
	}
	UpdateSamples[SAMPLE_LENGTH - 1] = (float)(int)Update;
	DrawSamples[SAMPLE_LENGTH - 1] = (float)(int)Draw;

	{
		ImGui::Begin("Performance Monitor");

		if(ImGui::TreeNodeEx("負荷計測", ImGuiTreeNodeFlags_DefaultOpen)){

			float FixedFPSAvg = 0.0f;
			float DeltaFPSAvg = 0.0f;
			float UpdateAvg = 0.0f;
			float DrawAvg = 0.0f;
			int FPSCount = 0;
			for(int n = 0; n < SAMPLE_LENGTH; n++){
				if(0 < FixedFpsSamples[n]){
					FixedFPSAvg += FixedFpsSamples[n];
					DeltaFPSAvg += DeltaFpsSamples[n];
					FPSCount++;
				}
				UpdateAvg += UpdateSamples[n];
				DrawAvg += DrawSamples[n];
			}
			if(0 < FixedFPSAvg){
				FixedFPSAvg /= FPSCount;
				DeltaFPSAvg /= FPSCount;
			}
			UpdateAvg /= SAMPLE_LENGTH;
			DrawAvg /= SAMPLE_LENGTH;

			char Texts[64]{};
			ImGui::Text("-FPS計測-");
			sprintf(Texts, "Fixed:%.2f Avg:%.2f", FixedFpsSamples[SAMPLE_LENGTH - 1], FixedFPSAvg);
			ImGui::PlotLines(Texts, FixedFpsSamples, SAMPLE_LENGTH, 0, "", 0.0f);
			sprintf(Texts, "Delta:%.2f Avg:%.2f", DeltaFpsSamples[SAMPLE_LENGTH - 1], DeltaFPSAvg);
			ImGui::PlotLines(Texts, DeltaFpsSamples, SAMPLE_LENGTH, 0, "", 0.0f);
			ImGui::Text("-更新処理-");
			sprintf(Texts, "Update:Avg:%.2fms", UpdateAvg);
			ImGui::PlotLines(Texts, UpdateSamples, SAMPLE_LENGTH, 0, "", 0.0f, 1000.0f / 60.0f);
			ImGui::Text("-描画処理-");
			sprintf(Texts, "Draw:Avg:%.2fms", DrawAvg);
			ImGui::PlotLines(Texts, DrawSamples, SAMPLE_LENGTH, 0, "", 0.0f, 1000.0f / 60.0f);
			ImGui::TreePop();
		}
		ImGui::End();
	}
#endif
}

void SetModernStyle(){
	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4* colors = style.Colors;

	// モダンダークテーマ - VS Code / Unreal Engine風
	colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.07f, 0.07f, 0.07f, 0.5f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.10f, 0.94f);
	colors[ImGuiCol_Border] = ImVec4(0.8f, 0.8f, 0.8f, 0.2f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.25f, 0.35f, 1.00f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.35f, 0.35f, 0.55f, 1.00f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.20f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.08f, 0.10f, 0.75f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.50f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.70f, 0.70f, 0.90f, 1.00f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.90f, 0.90f, 1.00f, 1.00f);
	colors[ImGuiCol_Button] = ImVec4(0.20f, 0.25f, 0.30f, 1.00f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.35f, 0.55f, 0.85f, 1.00f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.25f, 0.45f, 0.75f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.30f, 0.35f, 0.40f, 0.60f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.40f, 0.60f, 1.00f, 0.80f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.40f, 0.60f, 1.00f, 1.00f);
	colors[ImGuiCol_Separator] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.70f, 0.70f, 0.90f, 0.78f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(0.90f, 0.90f, 1.00f, 1.00f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.50f, 0.60f, 1.00f, 0.25f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.50f, 0.60f, 1.00f, 0.67f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.50f, 0.60f, 1.00f, 0.95f);
	colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.40f, 0.60f, 1.00f, 0.90f);
	colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.35f, 0.50f, 1.00f);
	colors[ImGuiCol_TabUnfocused] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.20f, 0.25f, 0.30f, 1.00f);
	colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.50f, 0.50f, 1.00f);
	colors[ImGuiCol_PlotHistogram] = ImVec4(1.00f, 0.90f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.70f, 0.00f, 1.00f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.50f, 0.80f, 1.00f, 0.35f);
	colors[ImGuiCol_DragDropTarget] = ImVec4(0.5f, 0.80f, 1.0f, 0.90f);
	colors[ImGuiCol_NavHighlight] = ImVec4(0.50f, 0.60f, 1.00f, 1.00f);
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.20f);
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);

	style.WindowPadding = ImVec2(8.00f, 8.00f);
	style.FramePadding = ImVec2(5.00f, 2.00f);
	style.CellPadding = ImVec2(6.00f, 6.00f);
	style.ItemSpacing = ImVec2(6.00f, 6.00f);
	style.ItemInnerSpacing = ImVec2(6.00f, 6.00f);
	style.TouchExtraPadding = ImVec2(0.00f, 0.00f);
	style.IndentSpacing = 25;
	style.ScrollbarSize = 15;
	style.GrabMinSize = 10;
	style.WindowBorderSize = 1;
	style.ChildBorderSize = 1;
	style.PopupBorderSize = 1;
	style.FrameBorderSize = 1;
	style.TabBorderSize = 1;
	style.WindowRounding = 7;
	style.ChildRounding = 4;
	style.FrameRounding = 3;
	style.PopupRounding = 4;
	style.ScrollbarRounding = 9;
	style.GrabRounding = 3;
	style.LogSliderDeadzone = 4;
	style.TabRounding = 4;
}