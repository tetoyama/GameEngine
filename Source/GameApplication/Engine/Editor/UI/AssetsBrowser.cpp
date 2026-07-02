// =======================================================================
// 
// AssetsBrowser.cpp
// 
// =======================================================================
#include "AssetsBrowser.h"
#include "buildSetting.h"
#include <filesystem>
#include <algorithm>
#include <windows.h>

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
	m_editor = editor;
	m_selectedPath = std::filesystem::path(ASSET_PATH).string();
	InvalidateFileSystemCache();

	fileIcon[FileIconType::FILE_UNDEFINED] = resourceService->Load<TextureData>("Asset\\Texture\\UI\\FileIcon\\file_undefied.png");
	fileIcon[FileIconType::FILE_FOLDER] = resourceService->Load<TextureData>("Asset\\Texture\\UI\\FileIcon\\folder.png");
	fileIcon[FileIconType::FILE_TEXT] = resourceService->Load<TextureData>("Asset\\Texture\\UI\\FileIcon\\file_txt.png");
	fileIcon[FileIconType::FILE_YAML] = resourceService->Load<TextureData>("Asset\\Texture\\UI\\FileIcon\\file_yaml.png");
	fileIcon[FileIconType::FILE_FBX] = resourceService->Load<TextureData>("Asset\\Texture\\UI\\FileIcon\\file_fbx.png");
	fileIcon[FileIconType::FILE_OBJ] = resourceService->Load<TextureData>("Asset\\Texture\\UI\\FileIcon\\file_obj.png");
	fileIcon[FileIconType::FILE_TTF] = resourceService->Load<TextureData>("Asset\\Texture\\UI\\FileIcon\\file_ttf.png");
}

void AssetsBrowser::Finalize(){
	for(int i = 0; i < FileIconType::FILE_MAX; i++){
		fileIcon[i].reset();
	}
	ClearPreviewCache();
	InvalidateFileSystemCache();
}

void AssetsBrowser::Draw(const EditorDrawContext ctx){
	bool* showAssetsBrowser = &m_editor->GetUI<MenuBar>()->showAssetsBrowser;
	if(!showAssetsBrowser || !*showAssetsBrowser){
		return;
	}

	const std::filesystem::path assetsRoot = ASSET_PATH;
	if(m_selectedPath.empty()){
		m_selectedPath = assetsRoot.string();
	}

	ImGuiWindowClass windowClass;
	windowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
	ImGui::SetNextWindowClass(&windowClass);

	if(!ImGui::Begin("Assets Browser", showAssetsBrowser, 0)){
		ImGui::End();
		return;
	}

	if(m_fileSystemCacheInvalidationPending){
		m_directoryCache.clear();
		m_assetCache.clear();
		m_fileSystemCacheInvalidationPending = false;
	}

	if(ImGui::Button("Refresh")){
		InvalidateFileSystemCache();
		ClearPreviewCache();
	}
	ImGui::SameLine();
	ImGui::TextDisabled("Filesystem cache");

	ImGui::Columns(2, "AssetColumns", true);
	static bool firstFrame = true;
	if(firstFrame){
		ImGui::SetColumnWidth(0, 400);
		firstFrame = false;
	}

	ImGui::BeginChild("LeftPane", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
	std::error_code ec;
	if(std::filesystem::exists(assetsRoot, ec) && std::filesystem::is_directory(assetsRoot, ec)){
		ImGui::PushID("AssetsRoot");
		if(ImGui::TreeNodeEx("Assets", ImGuiTreeNodeFlags_DefaultOpen)){
			if(ImGui::IsItemClicked() && m_selectedPath != assetsRoot.string()){
				ClearPreviewCache();
				m_selectedPath = assetsRoot.string();
			}
			if(ImGui::BeginPopupContextItem()){
				if(ImGui::MenuItem("新しいフォルダを作成")){
					std::filesystem::path newFolder = assetsRoot / "NewFolder";
					int index = 1;
					while(std::filesystem::exists(newFolder)){
						newFolder = assetsRoot / ("NewFolder" + std::to_string(index++));
					}
					std::filesystem::create_directory(newFolder);
					InvalidateFileSystemCache();
				}
				ImGui::EndPopup();
			}
			DrawDirectoryTree(assetsRoot, m_selectedPath);
			ImGui::TreePop();
		}
		ImGui::PopID();
	} else{
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "Assets directory not found!");
	}
	ImGui::EndChild();

	ImGui::NextColumn();
	ImGui::BeginChild("RightPane", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
	DrawAssetsInDirectory(m_selectedPath);
	ImGui::EndChild();

	ImGui::Columns(1);
	ImGui::End();
}

void AssetsBrowser::InvalidateFileSystemCache(){
	// Context menu操作中はキャッシュ配列を走査中の場合があるため、
	// 実際の破棄は次のDraw開始時まで遅延する。
	m_fileSystemCacheInvalidationPending = true;
}

const AssetsBrowser::CachedEntryList& AssetsBrowser::GetCachedDirectories(
	const std::filesystem::path& directory
){
	const std::string key = directory.lexically_normal().string();
	auto found = m_directoryCache.find(key);
	if(found != m_directoryCache.end()){
		return found->second;
	}

	CachedEntryList entries;
	std::error_code ec;
	for(std::filesystem::directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec)){
		const auto& entry = *it;
		if(!entry.is_directory(ec)) continue;

		CachedAssetEntry cached{};
		cached.path = entry.path();
		cached.name = cached.path.filename().string();
		cached.isDirectory = true;

		std::error_code childError;
		for(std::filesystem::directory_iterator childIt(cached.path, childError), childEnd;
			!childError && childIt != childEnd;
			childIt.increment(childError)){
			if(childIt->is_directory(childError)){
				cached.hasSubDirectories = true;
				break;
			}
		}
		entries.push_back(std::move(cached));
	}

	std::sort(entries.begin(), entries.end(), [](const CachedAssetEntry& a, const CachedAssetEntry& b){
		return a.name < b.name;
	});
	return m_directoryCache.emplace(key, std::move(entries)).first->second;
}

const AssetsBrowser::CachedEntryList& AssetsBrowser::GetCachedAssets(
	const std::filesystem::path& directory
){
	const std::string key = directory.lexically_normal().string();
	auto found = m_assetCache.find(key);
	if(found != m_assetCache.end()){
		return found->second;
	}

	CachedEntryList entries;
	std::error_code ec;
	for(std::filesystem::directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec)){
		const auto& entry = *it;
		const bool isDirectory = entry.is_directory(ec);
		const bool isFile = !ec && entry.is_regular_file(ec);
		if(!isDirectory && !isFile) continue;

		CachedAssetEntry cached{};
		cached.path = entry.path();
		cached.name = cached.path.filename().string();
		cached.isDirectory = isDirectory;
		entries.push_back(std::move(cached));
	}

	std::sort(entries.begin(), entries.end(), [](const CachedAssetEntry& a, const CachedAssetEntry& b){
		if(a.isDirectory != b.isDirectory) return a.isDirectory;
		return a.name < b.name;
	});
	return m_assetCache.emplace(key, std::move(entries)).first->second;
}

void AssetsBrowser::ClearPreviewCache(){
	for(auto& [key, tex] : previewCache){
		previewCache[key].reset();
		resourceService->Unload<TextureData>(key);
	}
	previewCache.clear();
}

void AssetsBrowser::DrawDirectoryTree(const std::filesystem::path& directory, std::string& selectedPath){
	// Recursive expansion may insert another cache entry and rehash the map.
	// Copy cached metadata so the current traversal remains valid.
	const CachedEntryList directories = GetCachedDirectories(directory);
	for(const CachedAssetEntry& entry : directories){
		const std::filesystem::path& path = entry.path;
		// Nested folders are lazy-opened. DefaultOpen here would enumerate the
		// complete Asset tree on the first frame and create a startup spike.
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
			ImGuiTreeNodeFlags_SpanAvailWidth;
		if(!entry.hasSubDirectories){
			flags |= ImGuiTreeNodeFlags_Leaf;
		}

		const bool isSelected = selectedPath == path.string();
		if(isSelected) flags |= ImGuiTreeNodeFlags_Selected;
		if(isSelected){
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.5f, 0.9f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.4f, 0.6f, 1.0f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
		}

		ImGui::PushID(path.string().c_str());
		const bool opened = ImGui::TreeNodeEx(entry.name.c_str(), flags);
		if(isSelected) ImGui::PopStyleColor(3);

		if(ImGui::BeginPopupContextItem()){
			if(ImGui::MenuItem("新しいフォルダを作成")){
				std::filesystem::path newFolder = path / "NewFolder";
				int index = 1;
				while(std::filesystem::exists(newFolder)){
					newFolder = path / ("NewFolder" + std::to_string(index++));
				}
				std::filesystem::create_directory(newFolder);
				InvalidateFileSystemCache();
			}
			if(ImGui::MenuItem("削除")){
				std::error_code ec;
				if(std::filesystem::is_empty(path, ec)){
					std::filesystem::remove(path, ec);
					InvalidateFileSystemCache();
				}
			}
			if(ImGui::MenuItem("エクスプローラーで開く")){
				ShellExecuteA(NULL, "open", path.string().c_str(), NULL, NULL, SW_SHOW);
			}
			ImGui::EndPopup();
		}

		if(ImGui::IsItemClicked() && selectedPath != path.string()){
			selectedPath = path.string();
			ClearPreviewCache();
		}

		if(openRename){
			ImGui::OpenPopup("名前を変更");
			openRename = false;
		}
		if(ImGui::BeginPopupModal("名前を変更", nullptr, ImGuiWindowFlags_AlwaysAutoResize)){
			ImGui::InputText("新しい名前", newNameBuffer, IM_ARRAYSIZE(newNameBuffer));
			if(ImGui::Button("OK")){
				std::filesystem::path newPath = renameTarget.parent_path() / newNameBuffer;
				std::error_code ec;
				std::filesystem::rename(renameTarget, newPath, ec);
				InvalidateFileSystemCache();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if(ImGui::Button("キャンセル")) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		if(opened){
			if(entry.hasSubDirectories) DrawDirectoryTree(path, selectedPath);
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
}



void AssetsBrowser::DrawAssetsInDirectory(std::string& selectedPath){
	const std::filesystem::path path = selectedPath;
	ImGui::Text("%s", selectedPath.c_str());

	float searchWidth = 120.0f;
	if(ImGui::GetWindowContentRegionMax().x * 0.3f > searchWidth){
		searchWidth = ImGui::GetWindowContentRegionMax().x * 0.3f;
	}
	ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - searchWidth);
	ImGui::PushItemWidth(searchWidth);
	static char searchBuffer[256] = "";
	ImGui::InputTextWithHint("##AssetSearch", "Search files...", searchBuffer, sizeof(searchBuffer));
	ImGui::PopItemWidth();
	ImGui::Separator();
	ImGui::BeginChild("Child", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

	const float itemSize = 70.0f;
	const float panelWidth = ImGui::GetContentRegionAvail().x;
	const float padding = 5.0f;
	int columnsCount = static_cast<int>(panelWidth / (itemSize + padding));
	if(columnsCount < 1) columnsCount = 1;

	std::string searchStr = searchBuffer;
	std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(),
		[](unsigned char c){ return static_cast<char>(std::tolower(c)); });

	const CachedEntryList& entries = GetCachedAssets(path);
	int index = 0;
	for(const CachedAssetEntry& entry : entries){
		if(!searchStr.empty()){
			std::string lowerFilename = entry.name;
			std::transform(lowerFilename.begin(), lowerFilename.end(), lowerFilename.begin(),
				[](unsigned char c){ return static_cast<char>(std::tolower(c)); });
			if(lowerFilename.find(searchStr) == std::string::npos) continue;
		}

		if(index % columnsCount != 0){
			ImGui::SameLine(0.0f, (panelWidth - itemSize * columnsCount) / columnsCount);
		}

		ImGui::PushID(entry.path.string().c_str());
		ImGui::BeginGroup();
		const ImVec2 cursorPos = ImGui::GetCursorPos();

		if(entry.isDirectory){
			if(ImGui::Button("##Folder", ImVec2(itemSize, itemSize))){
				ClearPreviewCache();
				selectedPath = entry.path.string();
			}
		} else{
			ImGui::Button("##ICON", ImVec2(itemSize, itemSize));
			if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)){
				const std::string fullPath = entry.path.string();
				ImGui::SetDragDropPayload("ASSET_PATH", fullPath.c_str(), fullPath.size() + 1);
				ImGui::Text("Drag: %s", entry.name.c_str());
				ImGui::EndDragDropSource();
			}
		}
		const ImVec2 afterCursorPos = ImGui::GetCursorPos();

		const std::string iconPath = entry.isDirectory ? "FOLDER" : entry.path.string();
		TextureData* icon = GetIconTexture(iconPath);
		if(icon && icon->pTexture){
			ImVec2 iconSize(static_cast<float>(icon->Width), static_cast<float>(icon->Height));
			if(iconSize.x < iconSize.y){
				iconSize.x = iconSize.x * itemSize / iconSize.y;
				iconSize.y = itemSize;
				ImGui::SetCursorPos(ImVec2(cursorPos.x + (itemSize - iconSize.x) * 0.5f, cursorPos.y));
			} else{
				iconSize.y = iconSize.y * itemSize / iconSize.x;
				iconSize.x = itemSize;
				ImGui::SetCursorPos(ImVec2(cursorPos.x, cursorPos.y + (itemSize - iconSize.y) * 0.5f));
			}
			const ImVec4 imageColor = (ImGui::IsItemHovered() || ImGui::IsItemActive())
				? ImVec4(1, 1, 1, 0.5f)
				: ImVec4(1, 1, 1, 1);
			ImGui::Image(icon->pTexture.Get(), iconSize, ImVec2(0, 0), ImVec2(1, 1), imageColor, ImVec4(0, 0, 0, 0));
			ImGui::SetCursorPos(afterCursorPos);
		}

		std::string displayName = entry.name;
		const float maxTextWidth = itemSize;
		if(ImGui::CalcTextSize(displayName.c_str()).x > maxTextWidth){
			int len = static_cast<int>(displayName.size());
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

	FileIconType type = FileIconType::FILE_UNDEFINED;

	//return fileIcon[type].get();


	if(filepath == "FOLDER"){
		type = FileIconType::FILE_FOLDER;
	}
	std::string ext = GetFileExtension(filepath);

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
		auto it = previewCache.find(filepath);
		if(it != previewCache.end() && it->second->pTexture){
			return it->second.get();
		}
		// 初回読み込み → テクスチャ作成
		auto tex = resourceService->Load<TextureData>(filepath);
		if(tex){
			previewCache[filepath] = tex;
			tex.reset();
			return previewCache[filepath].get();
		}
	}

	return fileIcon[type].get();
}
