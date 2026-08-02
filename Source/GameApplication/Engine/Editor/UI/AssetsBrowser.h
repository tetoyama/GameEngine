// =======================================================================
// 
// AssetsBrowser.h
// 
// =======================================================================
#pragma once

#include "Editor/editorService.h"
#include "Editor/InterFace/IEditorUI.h"

#include <cstddef>
#include <filesystem>
#include <memory>
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

class AssetsBrowser: public IEditorUI{
public:
	void Initialize(EditorService* editor) override;
	void Finalize() override;
	void Draw(const EditorDrawContext ctx) override;

private:
	// Dear ImGui does not expose a NoHorizontalScrollbar flag. Horizontal
	// scrolling is disabled by default unless HorizontalScrollbar is requested,
	// so zero flags are the correct behavior for the existing call site.
	static constexpr int ImGuiWindowFlags_NoHorizontalScrollbar = 0;

	enum class ViewMode {
		Grid,
		List
	};

	struct CachedAssetEntry {
		std::filesystem::path path;
		std::string name;
		std::string extension;
		std::uintmax_t fileSize = 0;
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
	void DrawToolbar(const std::filesystem::path& assetsRoot);
	void DrawBreadcrumbs(const std::filesystem::path& assetsRoot);
	void DrawRenameModal();
	void DrawGrid(const CachedEntryList& entries, const std::string& lowerSearch);
	void DrawList(const CachedEntryList& entries, const std::string& lowerSearch);
	void DrawAssetContextMenu(const CachedAssetEntry& entry);
	void NavigateTo(const std::filesystem::path& path, bool recordHistory = true);
	void BeginRename(const std::filesystem::path& path);
	bool MatchesSearch(const CachedAssetEntry& entry, const std::string& lowerSearch) const;
	std::string FormatFileSize(std::uintmax_t bytes) const;
	TextureData* GetIconTexture(std::string filepath);

	bool openRename = false;
	std::filesystem::path renameTarget;
	char newNameBuffer[256]{};
	char m_searchBuffer[256]{};

	EditorService* m_editor = nullptr;
	ResourceService* resourceService = nullptr;
	std::string m_selectedPath;
	std::string m_selectedAssetPath;
	ViewMode m_viewMode = ViewMode::Grid;

	std::vector<std::string> m_navigationHistory;
	std::size_t m_navigationIndex = 0;
	bool m_navigationInitialized = false;

	std::shared_ptr<TextureData> fileIcon[FileIconType::FILE_MAX];
	std::unordered_map<std::string, std::shared_ptr<TextureData>> previewCache;
	std::unordered_map<std::string, CachedEntryList> m_directoryCache;
	std::unordered_map<std::string, CachedEntryList> m_assetCache;
	bool m_fileSystemCacheInvalidationPending = true;
};
