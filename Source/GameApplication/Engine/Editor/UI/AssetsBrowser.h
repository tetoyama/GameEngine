// =======================================================================
// 
// AssetsBrowser.h
// 
// =======================================================================
#pragma once

#include "GameApplication.h"

#include "Editor/editorService.h"
#include "Editor/InterFace/IEditorUI.h"

#include <unordered_set>
#include <unordered_map>
#include <string>
#include <filesystem>


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
	void ClearPreviewCache();
	void DrawDirectoryTree(const std::filesystem::path& directory, std::string& selectedPath);
	void DrawAssetsInDirectory(std::string& selectedPath);
	TextureData* GetIconTexture(std::string filepath);

	bool m_openRename = false;
	std::filesystem::path m_renameTarget;
	char m_newNameBuffer[256]{};

	EditorService* m_editor = nullptr;
	ResourceService* m_resourceService = nullptr;

	std::shared_ptr<TextureData> fileIcon[FileIconType::FILE_MAX];

	std::unordered_map<std::string, std::shared_ptr<TextureData>> previewCache;
};
