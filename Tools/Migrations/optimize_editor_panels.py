from pathlib import Path


def load(path: str) -> str:
    return Path(path).read_text(encoding="utf-8-sig")


def save(path: str, text: str) -> None:
    Path(path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


def replace_cpp_function(text: str, signature: str, replacement: str) -> str:
    start = text.find(signature)
    if start < 0:
        raise RuntimeError(f"function not found: {signature}")
    brace = text.find("{", start)
    if brace < 0:
        raise RuntimeError(f"function brace not found: {signature}")
    depth = 0
    end = None
    for i in range(brace, len(text)):
        c = text[i]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                end = i + 1
                break
    if end is None:
        raise RuntimeError(f"function end not found: {signature}")
    return text[:start] + replacement.rstrip() + text[end:]


# -----------------------------------------------------------------------------
# Assets Browser: cache directory metadata instead of enumerating every frame.
# -----------------------------------------------------------------------------
assets_h = "Source/GameApplication/Engine/Editor/UI/AssetsBrowser.h"
save(assets_h, r'''// =======================================================================
// 
// AssetsBrowser.h
// 
// =======================================================================
#pragma once

#include "Editor/editorService.h"
#include "Editor/InterFace/IEditorUI.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

struct TextureData;
struct SceneManagerContext;
class ResourceService;

enum FileIconType {
	FILE_UNDEFINED = 0,
	FILE_FOLDER,
	FILE_TEXT,
	FILE_YAML,
	FILE_FBX,
	FILE_OBJ,
	FILE_TTF,
	FILE_MAX,
};

// アセットブラウザUI
class AssetsBrowser: public IEditorUI{
public:
	void Initialize(EditorService* editor) override;
	void Finalize() override;
	void Draw(const EditorDrawContext ctx) override;

private:
	struct CachedAssetEntry {
		std::filesystem::path path;
		std::string name;
		bool isDirectory = false;
		bool hasSubDirectories = false;
	};

	using CachedEntryList = std::vector<CachedAssetEntry>;

	void ClearPreviewCache();
	void InvalidateFileSystemCache();
	const CachedEntryList& GetCachedDirectories(const std::filesystem::path& directory);
	const CachedEntryList& GetCachedAssets(const std::filesystem::path& directory);
	void DrawDirectoryTree(const std::filesystem::path& directory, std::string& selectedPath);
	void DrawAssetsInDirectory(std::string& selectedPath);
	TextureData* GetIconTexture(std::string filepath);

	bool openRename = false;
	std::filesystem::path renameTarget;
	char newNameBuffer[256]{};

	EditorService* m_editor = nullptr;
	ResourceService* resourceService = nullptr;
	std::string m_selectedPath;

	std::shared_ptr<TextureData> fileIcon[FileIconType::FILE_MAX];
	std::unordered_map<std::string, std::shared_ptr<TextureData>> previewCache;
	std::unordered_map<std::string, CachedEntryList> m_directoryCache;
	std::unordered_map<std::string, CachedEntryList> m_assetCache;
};
''')

assets_cpp = "Source/GameApplication/Engine/Editor/UI/AssetsBrowser.cpp"
text = load(assets_cpp)
text = replace_once(text, '#include <filesystem>\n', '#include <filesystem>\n#include <algorithm>\n', 'assets includes')
text = replace_cpp_function(text, "void AssetsBrowser::Initialize(EditorService* editor)", r'''void AssetsBrowser::Initialize(EditorService* editor){
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
}''')
text = replace_cpp_function(text, "void AssetsBrowser::Finalize()", r'''void AssetsBrowser::Finalize(){
	for(int i = 0; i < FileIconType::FILE_MAX; i++){
		fileIcon[i].reset();
	}
	ClearPreviewCache();
	InvalidateFileSystemCache();
}''')
text = replace_cpp_function(text, "void AssetsBrowser::Draw(const EditorDrawContext ctx)", r'''void AssetsBrowser::Draw(const EditorDrawContext ctx){
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
}''')

insert_marker = "void AssetsBrowser::ClearPreviewCache()"
helpers = r'''void AssetsBrowser::InvalidateFileSystemCache(){
	m_directoryCache.clear();
	m_assetCache.clear();
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

'''
text = replace_once(text, insert_marker, helpers + insert_marker, 'asset cache helpers')
text = replace_cpp_function(text, "void AssetsBrowser::DrawDirectoryTree", r'''void AssetsBrowser::DrawDirectoryTree(const std::filesystem::path& directory, std::string& selectedPath){
	const CachedEntryList& directories = GetCachedDirectories(directory);
	for(const CachedAssetEntry& entry : directories){
		const std::filesystem::path& path = entry.path;
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
			ImGuiTreeNodeFlags_SpanAvailWidth |
			ImGuiTreeNodeFlags_DefaultOpen;
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
}''')
text = replace_cpp_function(text, "void AssetsBrowser::DrawAssetsInDirectory", r'''void AssetsBrowser::DrawAssetsInDirectory(std::string& selectedPath){
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
}''')
save(assets_cpp, text)


# -----------------------------------------------------------------------------
# Hierarchy: build one parent->children map instead of repeated full scans.
# -----------------------------------------------------------------------------
hierarchy_h = "Source/GameApplication/Engine/Editor/UI/Hierarchy.h"
text = load(hierarchy_h)
text = replace_once(text, '#include <unordered_set>\n', '#include <unordered_map>\n#include <unordered_set>\n#include <vector>\n', 'hierarchy includes')
text = replace_once(
    text,
    'private:\n\tvoid DrawHierarchyNode(Entity entity, SceneContext* context, const std::unordered_set<Entity>& allEntities);',
    'private:\n\tusing ChildMap = std::unordered_map<Entity, std::vector<Entity>>;\n\tvoid DrawHierarchyNode(Entity entity, SceneContext* context, const ChildMap& children, const std::string& lowerSearch);',
    'hierarchy signature',
)
save(hierarchy_h, text)

hierarchy_cpp = "Source/GameApplication/Engine/Editor/UI/Hierarchy.cpp"
text = load(hierarchy_cpp)
helper_start = text.find('// ------------------------------------------------------------\n// Entity が検索にヒットするか')
helper_end = text.find('void Hierarchy::Draw(const EditorDrawContext ctx)')
if helper_start < 0 or helper_end < 0:
    raise RuntimeError('hierarchy helper range not found')
new_helpers = r'''// ------------------------------------------------------------
// Entity が検索にヒットするか
// ------------------------------------------------------------
static bool EntityMatchesSearch(
	Entity entity,
	SceneContext* context,
	const std::string& lowerSearch
){
	if(lowerSearch.empty()) return true;
	const auto* name = context->component->GetComponent<NameComponent>(entity);
	return ToLower(name ? name->name : "Entity").find(lowerSearch) != std::string::npos;
}

using HierarchyChildMap = std::unordered_map<Entity, std::vector<Entity>>;

static bool HasMatchingChild(
	Entity entity,
	SceneContext* context,
	const HierarchyChildMap& children,
	const std::string& lowerSearch
){
	const auto found = children.find(entity);
	if(found == children.end()) return false;
	for(Entity child : found->second){
		if(EntityMatchesSearch(child, context, lowerSearch) ||
		   HasMatchingChild(child, context, children, lowerSearch)){
			return true;
		}
	}
	return false;
}

'''
text = text[:helper_start] + new_helpers + text[helper_end:]
text = replace_once(
    text,
    'ImGui::Begin("Hierarchy", showSceneHierarchy, toolbar_window_flags);',
    'if(!ImGui::Begin("Hierarchy", showSceneHierarchy, toolbar_window_flags)){\n\t\tImGui::End();\n\t\treturn;\n\t}',
    'hierarchy collapsed early return',
)
old_entity_block = r'''			const auto entities = registry->GetAllAlive();

			// --- ルートエンティティの描画（親を持たないもの） ---
			for(const Entity& entity : entities){

				// 同一フレーム内で削除済みのエンティティを飛ばす
				if(!registry->IsAlive(entity)) continue;

				auto* transform = context->component->GetComponent<TransformComponent>(entity);
				if(transform && transform->parent != 0){
					continue;
				}
				std::string search = searchBuffer;

				bool match = EntityMatchesSearch(entity, context, search);
				bool childMatch = HasMatchingChild(entity, context, entities, search);

				if(match || childMatch){
					DrawHierarchyNode(entity, context, entities);
				}
			}
'''
new_entity_block = r'''			const auto entities = registry->GetAllAlive();
			HierarchyChildMap children;
			std::vector<Entity> roots;
			children.reserve(entities.size());
			roots.reserve(entities.size());

			for(const Entity& entity : entities){
				if(!registry->IsAlive(entity)) continue;
				const auto* transform = context->component->GetComponent<TransformComponent>(entity);
				if(transform && transform->parent != 0){
					children[transform->parent].push_back(entity);
				} else{
					roots.push_back(entity);
				}
			}

			const std::string lowerSearch = ToLower(searchBuffer);
			for(const Entity& entity : roots){
				const bool match = EntityMatchesSearch(entity, context, lowerSearch);
				const bool childMatch = !lowerSearch.empty() &&
					HasMatchingChild(entity, context, children, lowerSearch);
				if(match || childMatch){
					DrawHierarchyNode(entity, context, children, lowerSearch);
				}
			}
'''
text = replace_once(text, old_entity_block, new_entity_block, 'hierarchy entity graph')
text = text.replace(
    'void Hierarchy::DrawHierarchyNode(Entity entity, SceneContext* context, const std::unordered_set<Entity>& allEntities)',
    'void Hierarchy::DrawHierarchyNode(Entity entity, SceneContext* context, const ChildMap& children, const std::string& lowerSearch)',
)
old_child_check = r'''	// --- 子の有無チェック ---
	bool hasChildren = false;
	for(Entity child : allEntities){
		auto* childTransform = context->component->GetComponent<TransformComponent>(child);
		if(childTransform && childTransform->parent == entity){
			hasChildren = true;
			break;
		}
	}
'''
new_child_check = r'''	const auto childIt = children.find(entity);
	const bool hasChildren = childIt != children.end() && !childIt->second.empty();
'''
text = replace_once(text, old_child_check, new_child_check, 'hierarchy child check')
old_child_draw = r'''	// --- 子描画 ---
	if(opened && hasChildren){
		for(Entity child : allEntities){
			auto* childTransform = context->component->GetComponent<TransformComponent>(child);
			if(childTransform && childTransform->parent == entity){

				std::string search = searchBuffer;

				bool match = EntityMatchesSearch(child, context, search);
				bool childMatch = HasMatchingChild(child, context, allEntities, search);

				if(match || childMatch){
					DrawHierarchyNode(child, context, allEntities);
				}
			}
		}
		ImGui::TreePop();
	}
'''
new_child_draw = r'''	// --- 子描画 ---
	if(opened && hasChildren){
		for(Entity child : childIt->second){
			const bool match = EntityMatchesSearch(child, context, lowerSearch);
			const bool childMatch = !lowerSearch.empty() &&
				HasMatchingChild(child, context, children, lowerSearch);
			if(match || childMatch){
				DrawHierarchyNode(child, context, children, lowerSearch);
			}
		}
		ImGui::TreePop();
	}
'''
text = replace_once(text, old_child_draw, new_child_draw, 'hierarchy child draw')
save(hierarchy_cpp, text)


# -----------------------------------------------------------------------------
# Log window: revision-based cache, one count pass, and ImGui clipping.
# -----------------------------------------------------------------------------
debug_system_h = "Source/GameApplication/Service/DebugTools/DebugSystem.h"
text = load(debug_system_h)
text = replace_once(text, '#include <chrono>\n', '#include <chrono>\n#include <cstdint>\n', 'debug revision include')
old_sink = r'''class MemoryLogSink: public ILogSink {
public:
	void Write(const LogEntry& entry) override{
		std::lock_guard<std::mutex> lock(mutex);
		entries.push_back(entry);
	}

	const std::vector<LogEntry>& GetEntries() const{
		return entries;
	}

	void Clear(){
		std::lock_guard<std::mutex> lock(mutex);
		entries.clear();
	}

private:
	std::vector<LogEntry> entries;
	mutable std::mutex mutex;
};'''
new_sink = r'''class MemoryLogSink: public ILogSink {
public:
	void Write(const LogEntry& entry) override{
		std::lock_guard<std::mutex> lock(mutex);
		entries.push_back(entry);
		++revision;
	}

	// Legacy read-only view. New UI code should use GetSnapshot() for thread safety.
	const std::vector<LogEntry>& GetEntries() const{
		return entries;
	}

	std::vector<LogEntry> GetSnapshot() const{
		std::lock_guard<std::mutex> lock(mutex);
		return entries;
	}

	uint64_t GetRevision() const{
		std::lock_guard<std::mutex> lock(mutex);
		return revision;
	}

	void Clear(){
		std::lock_guard<std::mutex> lock(mutex);
		entries.clear();
		++revision;
	}

private:
	std::vector<LogEntry> entries;
	uint64_t revision = 0;
	mutable std::mutex mutex;
};'''
text = replace_once(text, old_sink, new_sink, 'memory log sink')
save(debug_system_h, text)

log_h = "Source/GameApplication/Engine/Editor/UI/DebugLogWindow.h"
save(log_h, r'''// =======================================================================
// 
// DebugLogWindow.h
// 
// =======================================================================
#pragma once
#include "Editor/InterFace/IEditorUI.h"
#include "Service/DebugTools/DebugSystem.h"

#include <array>
#include <cstdint>
#include <vector>

// デバッグログ表示ウィンドウ
class DebugLogWindow: public IEditorUI{
public:
	void Initialize(EditorService* editor) override;
	void Finalize() override{}
	void Draw(const EditorDrawContext ctx) override;
	DebugLogWindow();

private:
	EditorService* m_editor = nullptr;
	std::shared_ptr<MemoryLogSink> logSink;

	char searchBuffer[128] = "";
	bool autoScroll = true;
	std::unordered_set<LogLevel> levelFilter;

	std::vector<LogEntry> cachedEntries;
	std::vector<size_t> filteredIndices;
	std::array<int, 6> levelCounts{};
	uint64_t cachedRevision = UINT64_MAX;
	std::string cachedSearch;
	uint32_t cachedFilterMask = 0;

	bool PassesFilter(const LogEntry& entry) const;
	const char* LevelToString(LogLevel level) const;
	std::string LevelFilterString(LogLevel level) const;
	ImVec4 GetColorForLevel(LogLevel level) const;
	uint32_t BuildFilterMask() const;
	void RefreshCache(bool force = false);

	std::string ToU8String(const char* cstr){
		if(!cstr) return {};
		return std::string(cstr, cstr + std::strlen(cstr));
	}
	std::string ToU8String(const std::string& s){
		return std::string(s.begin(), s.end());
	}
};
''')

log_cpp = "Source/GameApplication/Engine/Editor/UI/DebugLogWindow.cpp"
text = load(log_cpp)
text = replace_cpp_function(text, "std::string DebugLogWindow::LevelFilterString", r'''std::string DebugLogWindow::LevelFilterString(LogLevel level) const{
	const size_t index = static_cast<size_t>(level);
	const int count = index < levelCounts.size() ? levelCounts[index] : 0;
	return std::string(LevelToString(level)) + "(" + std::to_string(count) + ")";
}''')
insert_before = "void DebugLogWindow::Initialize(EditorService* editor)"
cache_helpers = r'''uint32_t DebugLogWindow::BuildFilterMask() const{
	uint32_t mask = 0;
	for(LogLevel level : levelFilter){
		mask |= 1u << static_cast<uint32_t>(level);
	}
	return mask;
}

void DebugLogWindow::RefreshCache(bool force){
	if(!logSink) return;
	const uint64_t revision = logSink->GetRevision();
	const uint32_t filterMask = BuildFilterMask();
	const std::string search = searchBuffer;
	if(!force && revision == cachedRevision && filterMask == cachedFilterMask && search == cachedSearch){
		return;
	}

	if(revision != cachedRevision){
		cachedEntries = logSink->GetSnapshot();
		levelCounts.fill(0);
		for(const LogEntry& entry : cachedEntries){
			const size_t index = static_cast<size_t>(entry.level);
			if(index < levelCounts.size()) ++levelCounts[index];
		}
	}

	filteredIndices.clear();
	filteredIndices.reserve(cachedEntries.size());
	for(size_t i = 0; i < cachedEntries.size(); ++i){
		if(PassesFilter(cachedEntries[i])) filteredIndices.push_back(i);
	}

	cachedRevision = revision;
	cachedFilterMask = filterMask;
	cachedSearch = search;
}

'''
text = replace_once(text, insert_before, cache_helpers + insert_before, 'log cache helpers')
text = replace_cpp_function(text, "void DebugLogWindow::Initialize(EditorService* editor)", r'''void DebugLogWindow::Initialize(EditorService* editor){
	m_editor = editor;
	logSink = editor->debugLogSystem->GetSink<MemoryLogSink>();
	RefreshCache(true);
}''')
text = replace_cpp_function(text, "void DebugLogWindow::Draw(const EditorDrawContext ctx)", r'''void DebugLogWindow::Draw(const EditorDrawContext ctx){
	bool* showDebugLogWindow = &m_editor->GetUI<MenuBar>()->showConsole;
	if(!showDebugLogWindow || !*showDebugLogWindow){
		return;
	}

	RefreshCache();

	ImGuiWindowClass windowClass;
	windowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
	ImGui::SetNextWindowClass(&windowClass);
	if(!ImGui::Begin("Debug Log", showDebugLogWindow, 0)){
		ImGui::End();
		return;
	}

	bool filterChanged = ImGui::InputText("Search", searchBuffer, sizeof(searchBuffer));
	ImGui::SameLine();
	if(ImGui::Button("Clear") && logSink){
		logSink->Clear();
		filterChanged = true;
	}
	ImGui::SameLine();
	ImGui::Checkbox("Auto Scroll", &autoScroll);
	ImGui::Separator();

	for(int i = static_cast<int>(LogLevel::Trace); i <= static_cast<int>(LogLevel::Critical); ++i){
		const LogLevel level = static_cast<LogLevel>(i);
		bool selected = levelFilter.find(level) != levelFilter.end();
		if(ImGui::Checkbox(LevelFilterString(level).c_str(), &selected)){
			if(selected) levelFilter.insert(level);
			else levelFilter.erase(level);
			filterChanged = true;
		}
		if(i < static_cast<int>(LogLevel::Critical)) ImGui::SameLine();
	}
	if(filterChanged) RefreshCache(true);

	if(ImGui::BeginChild("LogRegion", ImVec2(0, 0), false)){
		ImGuiListClipper clipper;
		clipper.Begin(static_cast<int>(filteredIndices.size()), ImGui::GetTextLineHeightWithSpacing() * 2.0f);
		while(clipper.Step()){
			for(int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row){
				const LogEntry& entry = cachedEntries[filteredIndices[static_cast<size_t>(row)]];
				ImGui::PushStyleColor(ImGuiCol_Text, GetColorForLevel(entry.level));
				ImGui::Text(
					ToU8String((const char*)u8"[%s] %s\n(関数名 %s,ファイル %s ,行 %d)").c_str(),
					LevelToString(entry.level),
					entry.message.c_str(),
					entry.function.c_str(),
					entry.file.c_str(),
					entry.line
				);
				ImGui::PopStyleColor();
			}
		}
		if(autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10.0f){
			ImGui::SetScrollHereY(1.0f);
		}
	}
	ImGui::EndChild();
	ImGui::End();
}''')
save(log_cpp, text)
