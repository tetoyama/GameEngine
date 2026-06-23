// =======================================================================
// 
// AssetsBrowser.h
// 
// =======================================================================
#pragma once

#include "Editor/editorService.h"
#include "Editor/InterFace/IEditorUI.h"

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
