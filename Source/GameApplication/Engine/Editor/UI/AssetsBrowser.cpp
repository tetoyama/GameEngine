// =======================================================================
//
// AssetsBrowser.cpp
//
// =======================================================================
#include "AssetsBrowser.h"

#include "buildSetting.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <windows.h>

#include <ImGui/imgui_internal.h>

#include "Editor/editorService.h"
#include "Editor/UI/MenuBar.h"
#include "Editor/UI/ModernImGui/EditorIconWidgets.h"
#include "Editor/UI/ModernImGui/ModernImGui.h"
#include "Resources/Data/textureData.h"
#include "Resources/Loader/textureLoader.h"
#include "Resources/resourceService.h"

namespace {

std::string Lowercase(std::string value){
	std::transform(
		value.begin(),
		value.end(),
		value.begin(),
		[](unsigned char character){
			return static_cast<char>(std::tolower(character));
		}
	);
	return value;
}

std::string TruncateToWidth(std::string text, float width){
	if(ImGui::CalcTextSize(text.c_str()).x <= width) return text;
	while(!text.empty() &&
		ImGui::CalcTextSize((text + "...").c_str()).x > width){
		text.pop_back();
	}
	return text.empty() ? "..." : text + "...";
}

} // namespace

void AssetsBrowser::Initialize(EditorService* editor){
	resourceService = editor->resourceService;
	m_editor = editor;
	NavigateTo(std::filesystem::path(ASSET_PATH));
	InvalidateFileSystemCache();

	fileIcon[FileIconType::FILE_UNDEFINED] = resourceService->Load<TextureData>(
		"Asset\\Texture\\UI\\FileIcon\\file_undefied.png"
	);
	fileIcon[FileIconType::FILE_TEXT] = resourceService->Load<TextureData>(
		"Asset\\Texture\\UI\\FileIcon\\file_txt.png"
	);
	fileIcon[FileIconType::FILE_YAML] = resourceService->Load<TextureData>(
		"Asset\\Texture\\UI\\FileIcon\\file_yaml.png"
	);
	fileIcon[FileIconType::FILE_FBX] = resourceService->Load<TextureData>(
		"Asset\\Texture\\UI\\FileIcon\\file_fbx.png"
	);
	fileIcon[FileIconType::FILE_OBJ] = resourceService->Load<TextureData>(
		"Asset\\Texture\\UI\\FileIcon\\file_obj.png"
	);
	fileIcon[FileIconType::FILE_TTF] = resourceService->Load<TextureData>(
		"Asset\\Texture\\UI\\FileIcon\\file_ttf.png"
	);
}

void AssetsBrowser::Finalize(){
	for(int index = 0; index < FileIconType::FILE_MAX; ++index){
		fileIcon[index].reset();
	}
	ClearPreviewCache();
	InvalidateFileSystemCache();
}

void AssetsBrowser::NavigateTo(
	const std::filesystem::path& destination,
	bool recordHistory
){
	std::error_code error;
	const std::filesystem::path normalized = destination.lexically_normal();
	if(!std::filesystem::exists(normalized, error) ||
		!std::filesystem::is_directory(normalized, error)){
		return;
	}

	const std::string next = normalized.string();
	if(next == m_selectedPath && m_navigationInitialized) return;

	m_selectedPath = next;
	m_selectedAssetPath.clear();
	ClearPreviewCache();

	if(!recordHistory) return;
	if(m_navigationInitialized &&
		m_navigationIndex + 1 < m_navigationHistory.size()){
		m_navigationHistory.erase(
			m_navigationHistory.begin() +
				static_cast<std::ptrdiff_t>(m_navigationIndex + 1),
			m_navigationHistory.end()
		);
	}
	m_navigationHistory.push_back(next);
	m_navigationIndex = m_navigationHistory.size() - 1;
	m_navigationInitialized = true;
}

void AssetsBrowser::Draw(const EditorDrawContext ctx){
	(void)ctx;
	bool* showAssetsBrowser = &m_editor->GetUI<MenuBar>()->showAssetsBrowser;
	if(!showAssetsBrowser || !*showAssetsBrowser) return;

	const std::filesystem::path assetsRoot =
		std::filesystem::path(ASSET_PATH).lexically_normal();
	if(m_selectedPath.empty()) NavigateTo(assetsRoot);

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

	DrawToolbar(assetsRoot);
	DrawBreadcrumbs(assetsRoot);
	ImGui::Spacing();

	const bool showDirectoryTree = ImGui::GetContentRegionAvail().x >= 680.0f;
	const int columnCount = showDirectoryTree ? 2 : 1;
	if(ImGui::BeginTable(
		"AssetWorkspace",
		columnCount,
		ImGuiTableFlags_Resizable |
		ImGuiTableFlags_SizingStretchProp |
		ImGuiTableFlags_NoSavedSettings
	)){
		if(showDirectoryTree){
			ImGui::TableSetupColumn(
				"Folders",
				ImGuiTableColumnFlags_WidthFixed,
				230.0f
			);
		}
		ImGui::TableSetupColumn("Content", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableNextRow();

		if(showDirectoryTree){
			ImGui::TableSetColumnIndex(0);
			ImGui::PushStyleColor(
				ImGuiCol_ChildBg,
				MImGui::WithAlpha(MImGui::GetTheme().panel, 0.42f)
			);
			if(ImGui::BeginChild("FolderTree", ImVec2(0.0f, 0.0f), false)){
				std::error_code error;
				if(std::filesystem::exists(assetsRoot, error) &&
					std::filesystem::is_directory(assetsRoot, error)){
					ImGuiTreeNodeFlags flags =
						ImGuiTreeNodeFlags_DefaultOpen |
						ImGuiTreeNodeFlags_OpenOnArrow |
						ImGuiTreeNodeFlags_SpanAvailWidth;
					if(std::filesystem::path(m_selectedPath).lexically_normal() == assetsRoot){
						flags |= ImGuiTreeNodeFlags_Selected;
					}
					const bool opened = ImGui::TreeNodeEx("Assets", flags);
					if(ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()){
						NavigateTo(assetsRoot);
					}
					if(ImGui::BeginPopupContextItem("AssetsRootMenu")){
						if(ImGui::MenuItem("New Folder")){
							std::filesystem::path folder = assetsRoot / "NewFolder";
							int suffix = 1;
							while(std::filesystem::exists(folder)){
								folder = assetsRoot /
									("NewFolder" + std::to_string(suffix++));
							}
							std::filesystem::create_directory(folder, error);
							InvalidateFileSystemCache();
						}
						if(ImGui::MenuItem("Show in Explorer")){
							ShellExecuteA(
								nullptr,
								"open",
								assetsRoot.string().c_str(),
								nullptr,
								nullptr,
								SW_SHOW
							);
						}
						ImGui::EndPopup();
					}
					if(opened){
						DrawDirectoryTree(assetsRoot, m_selectedPath);
						ImGui::TreePop();
					}
				}else{
					ImGui::TextColored(
						MImGui::GetTheme().dangerHover,
						"Assets directory not found."
					);
				}
			}
			ImGui::EndChild();
			ImGui::PopStyleColor();
		}

		ImGui::TableSetColumnIndex(showDirectoryTree ? 1 : 0);
		DrawAssetsInDirectory(m_selectedPath);
		ImGui::EndTable();
	}

	DrawRenameModal();
	ImGui::End();
}

void AssetsBrowser::DrawToolbar(const std::filesystem::path& assetsRoot){
	const MImGui::Theme& theme = MImGui::GetTheme();
	const float buttonSize = theme.compactHeight;
	const bool canGoBack = m_navigationInitialized && m_navigationIndex > 0;
	const bool canGoForward = m_navigationInitialized &&
		m_navigationIndex + 1 < m_navigationHistory.size();
	const bool canGoUp =
		std::filesystem::path(m_selectedPath).lexically_normal() != assetsRoot;

	ImGui::BeginDisabled(!canGoBack);
	if(MImGui::Button(
		"<##AssetBack",
		ImVec2(buttonSize, buttonSize),
		MImGui::ButtonKind::Ghost
	)){
		--m_navigationIndex;
		NavigateTo(m_navigationHistory[m_navigationIndex], false);
	}
	ImGui::EndDisabled();
	ImGui::SameLine(0.0f, 3.0f);

	ImGui::BeginDisabled(!canGoForward);
	if(MImGui::Button(
		">##AssetForward",
		ImVec2(buttonSize, buttonSize),
		MImGui::ButtonKind::Ghost
	)){
		++m_navigationIndex;
		NavigateTo(m_navigationHistory[m_navigationIndex], false);
	}
	ImGui::EndDisabled();
	ImGui::SameLine(0.0f, 3.0f);

	ImGui::BeginDisabled(!canGoUp);
	if(MImGui::Button(
		"^##AssetUp",
		ImVec2(buttonSize, buttonSize),
		MImGui::ButtonKind::Ghost
	)){
		NavigateTo(std::filesystem::path(m_selectedPath).parent_path());
	}
	ImGui::EndDisabled();
	ImGui::SameLine(0.0f, 5.0f);

	if(MImGui::Button(
		"Refresh##AssetRefresh",
		ImVec2(72.0f, buttonSize),
		MImGui::ButtonKind::Secondary
	)){
		InvalidateFileSystemCache();
		ClearPreviewCache();
	}

	const float available = ImGui::GetContentRegionAvail().x;
	const bool wideToolbar = available >= 420.0f;
	if(wideToolbar) ImGui::SameLine();
	else ImGui::Spacing();

	const float modeWidth = 54.0f;
	const float searchWidth = wideToolbar
		? (std::max)(150.0f, ImGui::GetContentRegionAvail().x - modeWidth * 2.0f - 8.0f)
		: -1.0f;
	MImGui::SearchField(
		"##AssetSearch",
		"Search current folder...",
		m_searchBuffer,
		sizeof(m_searchBuffer),
		searchWidth
	);

	if(!wideToolbar) ImGui::Spacing();
	else ImGui::SameLine();
	if(MImGui::Button(
		"Grid##AssetGrid",
		ImVec2(modeWidth, buttonSize),
		m_viewMode == ViewMode::Grid
			? MImGui::ButtonKind::Secondary
			: MImGui::ButtonKind::Ghost
	)){
		m_viewMode = ViewMode::Grid;
	}
	ImGui::SameLine(0.0f, 3.0f);
	if(MImGui::Button(
		"List##AssetList",
		ImVec2(modeWidth, buttonSize),
		m_viewMode == ViewMode::List
			? MImGui::ButtonKind::Secondary
			: MImGui::ButtonKind::Ghost
	)){
		m_viewMode = ViewMode::List;
	}
}

void AssetsBrowser::DrawBreadcrumbs(const std::filesystem::path& assetsRoot){
	const std::filesystem::path current =
		std::filesystem::path(m_selectedPath).lexically_normal();
	if(MImGui::Button("Assets##BreadcrumbRoot", ImVec2(0.0f, 0.0f), MImGui::ButtonKind::Ghost)){
		NavigateTo(assetsRoot);
	}

	std::error_code error;
	const std::filesystem::path relative = current.lexically_relative(assetsRoot);
	std::filesystem::path accumulated = assetsRoot;
	for(const auto& component : relative){
		const std::string name = component.string();
		if(name.empty() || name == ".") continue;
		accumulated /= component;
		ImGui::SameLine(0.0f, 4.0f);
		ImGui::TextDisabled(">");
		ImGui::SameLine(0.0f, 4.0f);
		ImGui::PushID(accumulated.string().c_str());
		if(MImGui::Button(
			name.c_str(),
			ImVec2(0.0f, 0.0f),
			MImGui::ButtonKind::Ghost
		)){
			NavigateTo(accumulated);
		}
		ImGui::PopID();
	}
	(void)error;
}

void AssetsBrowser::InvalidateFileSystemCache(){
	m_fileSystemCacheInvalidationPending = true;
}

const AssetsBrowser::CachedEntryList& AssetsBrowser::GetCachedDirectories(
	const std::filesystem::path& directory
){
	const std::string key = directory.lexically_normal().string();
	auto found = m_directoryCache.find(key);
	if(found != m_directoryCache.end()) return found->second;

	CachedEntryList entries;
	std::error_code error;
	for(std::filesystem::directory_iterator iterator(directory, error), end;
		!error && iterator != end;
		iterator.increment(error)){
		if(!iterator->is_directory(error)) continue;

		CachedAssetEntry cached{};
		cached.path = iterator->path();
		cached.name = cached.path.filename().string();
		cached.isDirectory = true;

		std::error_code childError;
		for(std::filesystem::directory_iterator child(cached.path, childError), childEnd;
			!childError && child != childEnd;
			child.increment(childError)){
			if(child->is_directory(childError)){
				cached.hasSubDirectories = true;
				break;
			}
		}
		entries.push_back(std::move(cached));
	}

	std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right){
		return left.name < right.name;
	});
	return m_directoryCache.emplace(key, std::move(entries)).first->second;
}

const AssetsBrowser::CachedEntryList& AssetsBrowser::GetCachedAssets(
	const std::filesystem::path& directory
){
	const std::string key = directory.lexically_normal().string();
	auto found = m_assetCache.find(key);
	if(found != m_assetCache.end()) return found->second;

	CachedEntryList entries;
	std::error_code error;
	for(std::filesystem::directory_iterator iterator(directory, error), end;
		!error && iterator != end;
		iterator.increment(error)){
		CachedAssetEntry cached{};
		cached.path = iterator->path();
		cached.name = cached.path.filename().string();
		cached.isDirectory = iterator->is_directory(error);
		const bool isFile = !error && iterator->is_regular_file(error);
		if(!cached.isDirectory && !isFile) continue;

		if(!cached.isDirectory){
			cached.extension = cached.path.extension().string();
			cached.fileSize = iterator->file_size(error);
			if(error){
				error.clear();
				cached.fileSize = 0;
			}
		}
		entries.push_back(std::move(cached));
	}

	std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right){
		if(left.isDirectory != right.isDirectory) return left.isDirectory;
		return left.name < right.name;
	});
	return m_assetCache.emplace(key, std::move(entries)).first->second;
}

void AssetsBrowser::ClearPreviewCache(){
	for(auto& [key, texture] : previewCache){
		texture.reset();
		resourceService->Unload<TextureData>(key);
	}
	previewCache.clear();
}

void AssetsBrowser::DrawDirectoryTree(
	const std::filesystem::path& directory,
	std::string& selectedPath
){
	const CachedEntryList directories = GetCachedDirectories(directory);
	for(const CachedAssetEntry& entry : directories){
		ImGuiTreeNodeFlags flags =
			ImGuiTreeNodeFlags_OpenOnArrow |
			ImGuiTreeNodeFlags_SpanAvailWidth;
		if(!entry.hasSubDirectories) flags |= ImGuiTreeNodeFlags_Leaf;
		if(selectedPath == entry.path.string()) flags |= ImGuiTreeNodeFlags_Selected;

		ImGui::PushID(entry.path.string().c_str());
		const bool opened = ImGui::TreeNodeEx(entry.name.c_str(), flags);
		if(ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()){
			NavigateTo(entry.path);
		}
		if(ImGui::BeginPopupContextItem("FolderTreeMenu")){
			if(ImGui::MenuItem("Open")) NavigateTo(entry.path);
			if(ImGui::MenuItem("New Folder")){
				std::filesystem::path folder = entry.path / "NewFolder";
				int suffix = 1;
				while(std::filesystem::exists(folder)){
					folder = entry.path / ("NewFolder" + std::to_string(suffix++));
				}
				std::error_code error;
				std::filesystem::create_directory(folder, error);
				InvalidateFileSystemCache();
			}
			if(ImGui::MenuItem("Rename")) BeginRename(entry.path);
			if(ImGui::MenuItem("Show in Explorer")){
				ShellExecuteA(
					nullptr,
					"open",
					entry.path.string().c_str(),
					nullptr,
					nullptr,
					SW_SHOW
				);
			}
			std::error_code error;
			const bool empty = std::filesystem::is_empty(entry.path, error);
			ImGui::BeginDisabled(!empty || error);
			if(ImGui::MenuItem("Delete Empty Folder")){
				std::filesystem::remove(entry.path, error);
				InvalidateFileSystemCache();
			}
			ImGui::EndDisabled();
			ImGui::EndPopup();
		}

		if(opened){
			if(entry.hasSubDirectories){
				DrawDirectoryTree(entry.path, selectedPath);
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
}

bool AssetsBrowser::MatchesSearch(
	const CachedAssetEntry& entry,
	const std::string& lowerSearch
) const{
	if(lowerSearch.empty()) return true;
	return Lowercase(entry.name).find(lowerSearch) != std::string::npos ||
		Lowercase(entry.extension).find(lowerSearch) != std::string::npos;
}

void AssetsBrowser::DrawAssetsInDirectory(std::string& selectedPath){
	const CachedEntryList& entries = GetCachedAssets(selectedPath);
	const std::string lowerSearch = Lowercase(m_searchBuffer);
	std::size_t visibleCount = 0;
	for(const CachedAssetEntry& entry : entries){
		if(MatchesSearch(entry, lowerSearch)) ++visibleCount;
	}

	const float footerHeight = ImGui::GetTextLineHeightWithSpacing() + 8.0f;
	if(ImGui::BeginChild(
		"AssetContent",
		ImVec2(0.0f, -footerHeight),
		false,
		ImGuiWindowFlags_NoHorizontalScrollbar
	)){
		if(visibleCount == 0){
			ImGui::Dummy(ImVec2(0.0f, 16.0f));
			ImGui::TextDisabled(
				lowerSearch.empty()
					? "This folder is empty."
					: "No assets match the current search."
			);
		}else if(m_viewMode == ViewMode::Grid){
			DrawGrid(entries, lowerSearch);
		}else{
			DrawList(entries, lowerSearch);
		}
	}
	ImGui::EndChild();

	ImGui::Separator();
	ImGui::TextDisabled(
		"%llu item%s",
		static_cast<unsigned long long>(visibleCount),
		visibleCount == 1 ? "" : "s"
	);
	if(!m_selectedAssetPath.empty()){
		ImGui::SameLine();
		ImGui::TextDisabled("· %s", std::filesystem::path(m_selectedAssetPath).filename().string().c_str());
	}
}

void AssetsBrowser::DrawGrid(
	const CachedEntryList& entries,
	const std::string& lowerSearch
){
	const float tileWidth = 94.0f;
	const float tileHeight = 100.0f;
	const float spacing = 8.0f;
	const float available = ImGui::GetContentRegionAvail().x;
	const int columns = (std::max)(
		1,
		static_cast<int>((available + spacing) / (tileWidth + spacing))
	);
	int visibleIndex = 0;

	for(const CachedAssetEntry& entry : entries){
		if(!MatchesSearch(entry, lowerSearch)) continue;
		if(visibleIndex % columns != 0) ImGui::SameLine(0.0f, spacing);

		ImGui::PushID(entry.path.string().c_str());
		ImGui::InvisibleButton("##AssetTile", ImVec2(tileWidth, tileHeight));
		const bool hovered = ImGui::IsItemHovered();
		const bool selected = m_selectedAssetPath == entry.path.string();
		const ImVec2 minimum = ImGui::GetItemRectMin();
		const ImVec2 maximum = ImGui::GetItemRectMax();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const MImGui::Theme& theme = MImGui::GetTheme();

		if(selected || hovered){
			drawList->AddRectFilled(
				minimum,
				maximum,
				ImGui::GetColorU32(MImGui::WithAlpha(
					selected ? theme.selected : theme.raised,
					selected ? 0.30f : 0.22f
				)),
				6.0f
			);
		}
		if(selected){
			drawList->AddLine(
				ImVec2(minimum.x + 7.0f, maximum.y - 2.0f),
				ImVec2(maximum.x - 7.0f, maximum.y - 2.0f),
				ImGui::GetColorU32(theme.accent),
				2.0f
			);
		}

		const float iconSize = 42.0f;
		const ImVec2 iconMinimum(
			(minimum.x + maximum.x - iconSize) * 0.5f,
			minimum.y + 12.0f
		);
		if(entry.isDirectory){
			MImGui::DrawEditorIcon(
				m_editor->icons.Get(EditorIcon::Assets),
				iconMinimum,
				iconSize,
				hovered ? 0.96f : 0.76f
			);
		}else if(TextureData* icon = GetIconTexture(entry.path.string());
			icon && icon->pTexture){
			const float sourceWidth = static_cast<float>(icon->Width);
			const float sourceHeight = static_cast<float>(icon->Height);
			const float scale = sourceWidth > 0.0f && sourceHeight > 0.0f
				? (std::min)(iconSize / sourceWidth, iconSize / sourceHeight)
				: 1.0f;
			const ImVec2 imageSize(sourceWidth * scale, sourceHeight * scale);
			const ImVec2 imageMinimum(
				(minimum.x + maximum.x - imageSize.x) * 0.5f,
				iconMinimum.y + (iconSize - imageSize.y) * 0.5f
			);
			drawList->AddImage(
				icon->pTexture.Get(),
				imageMinimum,
				ImVec2(imageMinimum.x + imageSize.x, imageMinimum.y + imageSize.y),
				ImVec2(0.0f, 0.0f),
				ImVec2(1.0f, 1.0f),
				IM_COL32(255, 255, 255, hovered ? 255 : 220)
			);
		}

		const std::string label = TruncateToWidth(entry.name, tileWidth - 12.0f);
		const ImVec2 labelSize = ImGui::CalcTextSize(label.c_str());
		drawList->AddText(
			ImVec2(
				(minimum.x + maximum.x - labelSize.x) * 0.5f,
				maximum.y - labelSize.y - 10.0f
			),
			ImGui::GetColorU32(ImGuiCol_Text),
			label.c_str()
		);

		if(ImGui::IsItemClicked()) m_selectedAssetPath = entry.path.string();
		if(hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
			entry.isDirectory){
			NavigateTo(entry.path);
		}
		if(hovered) ImGui::SetTooltip("%s", entry.name.c_str());
		DrawAssetContextMenu(entry);

		if(!entry.isDirectory && ImGui::BeginDragDropSource()){
			const std::string fullPath = entry.path.string();
			ImGui::SetDragDropPayload(
				"ASSET_PATH",
				fullPath.c_str(),
				fullPath.size() + 1
			);
			ImGui::TextUnformatted(entry.name.c_str());
			ImGui::EndDragDropSource();
		}
		ImGui::PopID();
		++visibleIndex;
	}
}

void AssetsBrowser::DrawList(
	const CachedEntryList& entries,
	const std::string& lowerSearch
){
	const bool compact = ImGui::GetContentRegionAvail().x < 520.0f;
	const int columns = compact ? 2 : 3;
	if(!ImGui::BeginTable(
		"AssetList",
		columns,
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_SizingStretchProp |
		ImGuiTableFlags_NoSavedSettings
	)) return;

	ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 92.0f);
	if(!compact){
		ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 92.0f);
	}
	ImGui::TableHeadersRow();

	for(const CachedAssetEntry& entry : entries){
		if(!MatchesSearch(entry, lowerSearch)) continue;
		ImGui::PushID(entry.path.string().c_str());
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		const bool selected = m_selectedAssetPath == entry.path.string();
		if(ImGui::Selectable(
			entry.name.c_str(),
			selected,
			ImGuiSelectableFlags_SpanAllColumns
		)){
			m_selectedAssetPath = entry.path.string();
		}
		const bool hovered = ImGui::IsItemHovered();
		if(hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
			entry.isDirectory){
			NavigateTo(entry.path);
		}
		DrawAssetContextMenu(entry);
		if(!entry.isDirectory && ImGui::BeginDragDropSource()){
			const std::string fullPath = entry.path.string();
			ImGui::SetDragDropPayload(
				"ASSET_PATH",
				fullPath.c_str(),
				fullPath.size() + 1
			);
			ImGui::TextUnformatted(entry.name.c_str());
			ImGui::EndDragDropSource();
		}

		ImGui::TableSetColumnIndex(1);
		ImGui::TextDisabled(
			"%s",
			entry.isDirectory
				? "Folder"
				: (entry.extension.empty() ? "File" : entry.extension.c_str())
		);
		if(!compact){
			ImGui::TableSetColumnIndex(2);
			ImGui::TextDisabled(
				"%s",
				entry.isDirectory ? "--" : FormatFileSize(entry.fileSize).c_str()
			);
		}
		ImGui::PopID();
	}
	ImGui::EndTable();
}

void AssetsBrowser::DrawAssetContextMenu(const CachedAssetEntry& entry){
	if(!ImGui::BeginPopupContextItem("AssetContext")) return;

	if(entry.isDirectory && ImGui::MenuItem("Open")) NavigateTo(entry.path);
	if(ImGui::MenuItem("Rename")) BeginRename(entry.path);
	if(ImGui::MenuItem("Copy Path")){
		ImGui::SetClipboardText(entry.path.string().c_str());
	}
	if(ImGui::MenuItem("Show in Explorer")){
		const std::filesystem::path target =
			entry.isDirectory ? entry.path : entry.path.parent_path();
		ShellExecuteA(
			nullptr,
			"open",
			target.string().c_str(),
			nullptr,
			nullptr,
			SW_SHOW
		);
	}
	if(entry.isDirectory){
		std::error_code error;
		const bool empty = std::filesystem::is_empty(entry.path, error);
		ImGui::BeginDisabled(!empty || error);
		if(ImGui::MenuItem("Delete Empty Folder")){
			std::filesystem::remove(entry.path, error);
			if(m_selectedAssetPath == entry.path.string()) m_selectedAssetPath.clear();
			InvalidateFileSystemCache();
		}
		ImGui::EndDisabled();
	}
	ImGui::EndPopup();
}

void AssetsBrowser::BeginRename(const std::filesystem::path& path){
	renameTarget = path;
	const std::string filename = path.filename().string();
	std::strncpy(newNameBuffer, filename.c_str(), sizeof(newNameBuffer) - 1);
	newNameBuffer[sizeof(newNameBuffer) - 1] = '\0';
	openRename = true;
}

void AssetsBrowser::DrawRenameModal(){
	if(openRename){
		ImGui::OpenPopup("Rename Asset");
		openRename = false;
	}
	if(!ImGui::BeginPopupModal(
		"Rename Asset",
		nullptr,
		ImGuiWindowFlags_AlwaysAutoResize
	)) return;

	MImGui::TextField(
		"##NewAssetName",
		newNameBuffer,
		IM_ARRAYSIZE(newNameBuffer),
		ImGuiInputTextFlags_EnterReturnsTrue,
		300.0f
	);
	const std::string requestedName = newNameBuffer;
	const bool valid = !requestedName.empty() &&
		requestedName.find('/') == std::string::npos &&
		requestedName.find('\\') == std::string::npos;

	ImGui::BeginDisabled(!valid);
	if(MImGui::PrimaryButton("Rename")){
		const std::filesystem::path newPath =
			renameTarget.parent_path() / requestedName;
		std::error_code error;
		std::filesystem::rename(renameTarget, newPath, error);
		if(!error){
			if(m_selectedAssetPath == renameTarget.string()){
				m_selectedAssetPath = newPath.string();
			}
			if(m_selectedPath == renameTarget.string()){
				NavigateTo(newPath, false);
			}
			InvalidateFileSystemCache();
			ImGui::CloseCurrentPopup();
		}
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if(MImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
	ImGui::EndPopup();
}

std::string AssetsBrowser::FormatFileSize(std::uintmax_t bytes) const{
	char buffer[64]{};
	if(bytes >= 1024ull * 1024ull * 1024ull){
		std::snprintf(
			buffer,
			sizeof(buffer),
			"%.1f GB",
			static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0)
		);
	}else if(bytes >= 1024ull * 1024ull){
		std::snprintf(
			buffer,
			sizeof(buffer),
			"%.1f MB",
			static_cast<double>(bytes) / (1024.0 * 1024.0)
		);
	}else if(bytes >= 1024ull){
		std::snprintf(
			buffer,
			sizeof(buffer),
			"%.1f KB",
			static_cast<double>(bytes) / 1024.0
		);
	}else{
		std::snprintf(
			buffer,
			sizeof(buffer),
			"%llu B",
			static_cast<unsigned long long>(bytes)
		);
	}
	return buffer;
}

TextureData* AssetsBrowser::GetIconTexture(std::string filepath){
	FileIconType type = FileIconType::FILE_UNDEFINED;
	const std::string extension = Lowercase(GetFileExtension(filepath));
	if(extension == ".txt") type = FileIconType::FILE_TEXT;
	if(extension == ".yaml") type = FileIconType::FILE_YAML;
	if(extension == ".fbx") type = FileIconType::FILE_FBX;
	if(extension == ".obj") type = FileIconType::FILE_OBJ;
	if(extension == ".ttf") type = FileIconType::FILE_TTF;

	if(extension == ".png" || extension == ".tga" ||
		extension == ".jpg" || extension == ".jpeg" ||
		extension == ".bmp"){
		auto found = previewCache.find(filepath);
		if(found != previewCache.end() && found->second->pTexture){
			return found->second.get();
		}
		auto texture = resourceService->Load<TextureData>(filepath);
		if(texture){
			previewCache[filepath] = texture;
			return previewCache[filepath].get();
		}
	}
	return fileIcon[type].get();
}
