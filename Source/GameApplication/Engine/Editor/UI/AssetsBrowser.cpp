// =======================================================================
// 
// AssetsBrowser.cpp
// 
// =======================================================================
#include "AssetsBrowser.h"
#include "buildSetting.h"
#include <filesystem>

#include "Inspector.h"
#include <ImGui/imgui_internal.h>
#include <memory>
#include <sceneManager.h>
#include "Editor/editorService.h"
#include "Editor/UI/MenuBar.h"
#include <scene.h>
#include <Component/transformComponent.h>
#include <Component/entityNameComponent.h>
#include "Hierarchy.h"
#include "DebugTools/DebugSystem.h"

#include "Resources/resourceService.h"
#include "Resources/Loader/textureLoader.h"
#include "Resources/Data/textureData.h"

void AssetsBrowser::Initialize(EditorService* editor){
	resourceService = editor->resourceService;
	m_pEditor = editor;

	fileIcon[FileIconType::FILE_UNDEFINED]	= resourceService->Load<TextureData>("Asset\\Texture\\UI\\FileIcon\\file_undefied.png");
	fileIcon[FileIconType::FILE_FOLDER]		= resourceService->Load<TextureData>("Asset\\Texture\\UI\\FileIcon\\folder.png");
	fileIcon[FileIconType::FILE_TEXT]		= resourceService->Load<TextureData>("Asset\\Texture\\UI\\FileIcon\\file_txt.png");
	fileIcon[FileIconType::FILE_YAML]		= resourceService->Load<TextureData>("Asset\\Texture\\UI\\FileIcon\\file_yaml.png");
	fileIcon[FileIconType::FILE_FBX]		= resourceService->Load<TextureData>("Asset\\Texture\\UI\\FileIcon\\file_fbx.png");
	fileIcon[FileIconType::FILE_OBJ]		= resourceService->Load<TextureData>("Asset\\Texture\\UI\\FileIcon\\file_obj.png");
	fileIcon[FileIconType::FILE_TTF]		= resourceService->Load<TextureData>("Asset\\Texture\\UI\\FileIcon\\file_ttf.png");
}

void AssetsBrowser::Finalize(){
	for(int i = 0; i < FileIconType::FILE_MAX; i++){
		fileIcon[i].reset();
	}
	ClearPreviewCache();
}

void AssetsBrowser::Draw(const EditorDrawContext ctx){

	bool* showAssetsBrowser = &m_pEditor->GetUI<MenuBar>()->showAssetsBrowser;

	if(!showAssetsBrowser || !*showAssetsBrowser){
		return;
	}

	SceneManagerContext* sceneManagerContext = m_pEditor->sceneManager->GetContext();

	std::filesystem::path m_AssetsRoot= ASSET_PATH;
	static std::string m_SelectedPath= ASSETS_ROOT.string();
	//m_pContext->debug->LOG_INFO("Current path: " + std::filesystem::current_path().string());

	// --- パス検証・正規化 ---
	std::error_code m_Ec;
	std::filesystem::path m_Normalized= std::filesystem::weakly_canonical(selectedPath, ec);

	if(ec || !std::filesystem::exists(normalized) || !std::filesystem::is_directory(normalized)){
		// フォールバック + ログ出力
		sceneManagerContext->debug->LOG_ERROR("Invalid selectedPath: " + selectedPath +
									"\n→ normalized: " + normalized.string() +
									"\n→ ec: " + std::to_string(ec.value()));
		selectedPath = ASSETS_ROOT.string();
	}

	// === ImGui Begin ===

	ImGuiWindowClass m_WindowClass;
	window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
	ImGui::SetNextWindowClass(&window_class);

	//ImGuiWindowFlags toolbar_window_flags = ImGuiWindowFlags_NoCollapse;
	ImGuiWindowFlags m_ToolbarWindowFlags= 0;
	ImGui::Begin("Assets Browser", showAssetsBrowser, toolbar_window_flags);

	ImGui::Columns(2, "AssetColumns", true);

	static bool m_FirstFrame= true;
	if(firstFrame){
		ImGui::SetColumnWidth(0, 400);
		firstFrame = false;
	}
	// === 左カラム：フォルダツリー ===
	ImGui::BeginChild("LeftPane", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

	if(std::filesystem::exists(ASSETS_ROOT) && std::filesystem::is_directory(ASSETS_ROOT)){
		ImGui::PushID("AssetsRoot");
		if(ImGui::TreeNodeEx("Assets", ImGuiTreeNodeFlags_DefaultOpen)){
			if(ImGui::IsItemClicked()){
				ClearPreviewCache();
				selectedPath = ASSETS_ROOT.string(); // ルート選択
			}
			// --- 右クリックメニュー追加 ---
			if(ImGui::BeginPopupContextItem()){
				if(ImGui::MenuItem("新しいフォルダを作成")){
					std::filesystem::path m_NewFolder= ASSETS_ROOT / "NewFolder";
					int m_Index= 1;
					while(std::filesystem::exists(newFolder)){
						newFolder = ASSETS_ROOT / ("NewFolder" + std::to_string(index++));
					}
					std::filesystem::create_directory(newFolder);
				}
				ImGui::EndPopup();
			}
			DrawDirectoryTree(ASSETS_ROOT, selectedPath);
			ImGui::TreePop();

		}
		ImGui::PopID();
	} else{
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "Assets directory not found!");
	}

	ImGui::EndChild();


	// === 右カラム：アセット一覧 ===
	ImGui::NextColumn();
	ImGui::BeginChild("RightPane", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

	DrawAssetsInDirectory(selectedPath);

	ImGui::EndChild();

	ImGui::Columns(1);
	ImGui::End();
}

void AssetsBrowser::ClearPreviewCache(){
	for(auto& [key, tex] : previewCache){
		previewCache[key].reset();
		resourceService->Unload<TextureData>(key);
	}
	previewCache.clear();
}

void AssetsBrowser::DrawDirectoryTree(const std::filesystem::path& directory, std::string& selectedPath){
	if(!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)){
		return;
	}

	// === ディレクトリ一覧を収集＆ソート ===
	std::vector<std::filesystem::directory_entry> m_Directories;
	for(const auto& entry : std::filesystem::directory_iterator(directory)){
		if(entry.is_directory()){
			directories.push_back(entry);
		}
	}

	std::sort(directories.begin(), directories.end(), [](const auto& a, const auto& b){
		return a.path().filename().string() < b.path().filename().string();
			  });

	// === 各フォルダ描画 ===
	for(const auto& entry : directories){
		const auto& path = entry.path();
		std::string m_Name= path.filename().string();

		ImGuiTreeNodeFlags m_Flags= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;

		// サブディレクトリを持っているかチェック
		bool m_HasSubDir= false;
		for(const auto& subEntry : std::filesystem::directory_iterator(path)){
			if(subEntry.is_directory()){
				hasSubDir = true;
				break;
			}
		}
		if(!hasSubDir){
			flags |= ImGuiTreeNodeFlags_Leaf;
		}

		// 選択状態なら色変更
		// 選択されていたらハイライトさせる
		bool m_IsSelected= (selectedPath == path.string());
		if(isSelected){
			flags |= ImGuiTreeNodeFlags_Selected;
		}
		if(isSelected){
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.5f, 0.9f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.4f, 0.6f, 1.0f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
		}

		ImGui::PushID(path.string().c_str());

		bool m_Opened= ImGui::TreeNodeEx(name.c_str(), flags);
		if(isSelected){
			ImGui::PopStyleColor(3);
		}
		// 右クリックメニュー（そのアイテムを右クリック）
		if(ImGui::BeginPopupContextItem()){
			if(ImGui::MenuItem("新しいフォルダを作成")){
				// 例：<path>/NewFolder のように作成
				std::filesystem::path m_NewFolder= path / "NewFolder";
				int m_Index= 1;
				while(std::filesystem::exists(newFolder)){
					newFolder = path / ("NewFolder" + std::to_string(index++));
				}
				std::filesystem::create_directory(newFolder);
			}


			//if(ImGui::MenuItem("名前を変更")){
			//	openRename = true;
			//	renameTarget = path;
			//	strcpy_s(newNameBuffer, path.filename().string().c_str());
			//	ImGui::CloseCurrentPopup();
			//}

			if(ImGui::MenuItem("削除")){
				std::error_code m_Ec;
				if(std::filesystem::is_empty(path, ec)){
					std::filesystem::remove(path, ec); // 安全に削除
				}
			}
			if(ImGui::MenuItem("エクスプローラーで開く")){
				ShellExecuteA(NULL, "open", path.string().c_str(), NULL, NULL, SW_SHOW);
			}
			ImGui::EndPopup();



		}


		if(ImGui::IsItemClicked()){
			std::error_code m_Ec;
			if(std::filesystem::exists(path, ec) && std::filesystem::is_directory(path, ec)){
				selectedPath = path.string();
				ClearPreviewCache();
			}
		}

		if(openRename){
			ImGui::OpenPopup("名前を変更");
			openRename = false;
		}

		if(ImGui::BeginPopupModal("名前を変更", nullptr, ImGuiWindowFlags_AlwaysAutoResize)){
			ImGui::InputText("新しい名前", newNameBuffer, IM_ARRAYSIZE(newNameBuffer));

			if(ImGui::Button("OK")){
				std::filesystem::path m_NewPath= renameTarget.parent_path() / newNameBuffer;
				std::error_code m_Ec;
				std::filesystem::rename(renameTarget, newPath, ec);
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if(ImGui::Button("キャンセル")){
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		// サブディレクトリがある場合だけ再帰
		if(opened){
			if(hasSubDir){
				DrawDirectoryTree(path, selectedPath);
			}
			ImGui::TreePop();
		}

		ImGui::PopID();


	}
}



void AssetsBrowser::DrawAssetsInDirectory(std::string& selectedPath){
	std::filesystem::path m_Path= selectedPath;
	std::error_code m_Ec;
	if(!std::filesystem::exists(path, ec) || !std::filesystem::is_directory(path, ec)) return;

	ImGui::Text("%s", selectedPath.c_str());

	// 検索バー
	float m_SearchWidth= 120.0f;
	if(ImGui::GetWindowContentRegionMax().x * 0.3f > SearchWidth){
		SearchWidth = ImGui::GetWindowContentRegionMax().x * 0.3f;
	}
	ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - SearchWidth);
	ImGui::PushItemWidth(SearchWidth);
	static char m_SearchBuffer[256] = "";
	ImGui::InputTextWithHint("##AssetSearch", "Search files...", searchBuffer, sizeof(searchBuffer));
	ImGui::PopItemWidth();

	ImGui::Separator();
	ImGui::BeginChild("Child", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

	float m_ItemSize= 70.0f;
	float m_PanelWidth= ImGui::GetContentRegionAvail().x;

	float m_Padding= 5.0f; // マージン
	float m_TotalItemWidth= itemSize + padding;

	int m_ColumnsCount= static_cast<int>(panelWidth / totalItemWidth);
	if(columnsCount < 1) columnsCount = 1;

	// エントリ収集
	std::vector<std::filesystem::directory_entry> m_Entries;
	for(const auto& entry : std::filesystem::directory_iterator(path, ec)){
		if(entry.is_directory(ec) || entry.is_regular_file(ec)){
			entries.push_back(entry);
		}
	}

	// フォルダ → ファイル順 に並べる
	std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b){
		if(a.is_directory() != b.is_directory()){
			return a.is_directory(); // フォルダが先
		}
		return a.path().filename().string() < b.path().filename().string();
			  });

	// 小文字での検索対応
	std::string m_SearchStr= searchBuffer;
	std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);

	int m_Index= 0;
	for(const auto& entry : entries){
		std::string m_Filename= entry.path().filename().string();

		// フィルタ処理
		if(!searchStr.empty()){
			std::string m_LowerFilename= filename;
			std::transform(lowerFilename.begin(), lowerFilename.end(), lowerFilename.begin(), ::tolower);
			if(lowerFilename.find(searchStr) == std::string::npos)
				continue;
		}

		if(index % columnsCount != 0)
			ImGui::SameLine(0.0f, (panelWidth - (itemSize)*columnsCount) / columnsCount);

		ImGui::PushID(entry.path().string().c_str());
		ImGui::BeginGroup();

		ImVec2 m_CursorPos= ImGui::GetCursorPos();

		// ディレクトリなら別アイコン・クリック時にパス移動
		if(entry.is_directory(ec)){
			if(ImGui::Button("##Folder", ImVec2(itemSize, itemSize))){

				ClearPreviewCache();

				selectedPath = entry.path().string();
			}
		} else{
			ImGui::Button("##ICON", ImVec2(itemSize, itemSize));

			if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)){
				std::string m_FullPath= entry.path().string();
				ImGui::SetDragDropPayload("ASSET_PATH", fullPath.c_str(), fullPath.size() + 1);
				ImGui::Text("Drag: %s", filename.c_str());
				ImGui::EndDragDropSource();
			}
		}
		ImVec2 m_AfterCursorPos= ImGui::GetCursorPos();

		std::string m_Path= entry.path().string();
		if(entry.is_directory(ec)){
			path = "FOLDER";
		}
		if(GetIconTexture(path)){
			ImVec2 m_IconSize= ImVec2((float)GetIconTexture(path)->Width, (float)GetIconTexture(path)->Height);
			if(IconSize.x < IconSize.y){
				IconSize.x = IconSize.x * itemSize / IconSize.y;
				IconSize.y = itemSize;
				ImGui::SetCursorPos(ImVec2(CursorPos.x + (itemSize - IconSize.x) * 0.5f, CursorPos.y));
			} else{
				IconSize.y = IconSize.y * itemSize / IconSize.x;
				IconSize.x = itemSize;
				ImGui::SetCursorPos(ImVec2(CursorPos.x, CursorPos.y + (itemSize - IconSize.y) * 0.5f));
			}
			ImVec4 m_ImageColor= ImVec4(1, 1, 1, 1);
			if(ImGui::IsItemHovered()){
				ImageColor = ImVec4(1, 1, 1, 0.5f);
			}
			if(ImGui::IsItemActive()){
				ImageColor = ImVec4(1, 1, 1, 0.5f);
			}
			ImGui::Image(GetIconTexture(path)->pTexture.Get(), IconSize, ImVec2(0, 0), ImVec2(1, 1), ImageColor, ImVec4(0, 0, 0, 0));
			ImGui::SetCursorPos(AfterCursorPos);
		}
		std::string m_DisplayName= filename;
		ImVec2 m_TextSize= ImGui::CalcTextSize(displayName.c_str());
		float m_MaxTextWidth= itemSize;

		if(textSize.x > maxTextWidth){
			int m_Len= static_cast<int>(displayName.size());
			while(len > 0 && ImGui::CalcTextSize((displayName.substr(0, len) + "...").c_str()).x > maxTextWidth){
				--len;
			}
			displayName = displayName.substr(0, len) + "...";
		}

		ImGui::TextUnformatted(displayName.c_str());

		ImGui::EndGroup();
		ImGui::PopID();
		++index;
	}

	ImGui::EndChild();
}

TextureData* AssetsBrowser::GetIconTexture(std::string filepath){

	FileIconType m_Type= FileIconType::FILE_UNDEFINED;

	//return fileIcon[type].get();


	if(filepath == "FOLDER"){
		type = FileIconType::FILE_FOLDER;
	}
	std::string m_Ext= GetFileExtension(filepath);

	if(ext == ".wav"){

	}
	if(ext == ".txt"){
		type = FileIconType::FILE_TEXT;
	}
	if(ext == ".yaml"){
		type = FileIconType::FILE_YAML;
	}
	if(ext == ".fbx"){
		type = FileIconType::FILE_FBX;
	}
	if(ext == ".obj"){
		type = FileIconType::FILE_OBJ;
	}
	if(ext == ".ttf"){
		type = FileIconType::FILE_TTF;
	}
	if(ext == ".png" || ext == ".tga" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp"){
		// キャッシュされていればそれを返す（重複読み込みを避ける）
		auto m_It= previewCache.find(filepath);
		if(it != previewCache.end() && it->second->pTexture){
			return it->second.get();
		}
		// 初回読み込み → テクスチャ作成
		auto m_Tex= resourceService->Load<TextureData>(filepath);
		if(tex){
			previewCache[filepath] = tex;
			tex.reset();
			return m_PreviewCache[filepath].get();
		}
	}

	return m_FileIcon[type].get();
}
