// GameScene/System/inspectorSystem.h
#pragma once
#include "../Interface/ISystem.h"
#include "../Entity/Entity.h" // Entityの定義をインクルード

#include "../../../Backends/ImGui/imgui.h"

#include <string>
#include <filesystem>
#include <memory>
#include <unordered_map>
#include <unordered_set>

struct SceneContext;
struct SceneManagerContext;
class TransformComponent;
class SpriteRendererComponent;

struct TextureData;

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

class InspectorSystem: public ISystem
{
public:

	InspectorSystem(SceneManagerContext* context): m_context(context){}
	~InspectorSystem(){}
	void Initialize() override;
	void Finalize()override;

	void Start() override;
	void Stop() override;

	void Update(float deltaTime) override{}
	void FixedUpdate(float fidedDeltaTime) override{}
	void Draw() override;
	void EditorUpdate(float deltaTime) override{}

private:
	SceneManagerContext* m_context;
	SceneContext* m_InspectorContext = nullptr;

	TransformComponent CalculateRectTransform(
		const SpriteRendererComponent& sprite,
		const TransformComponent& originalTransform
	);
	TransformComponent ReverseCalculateRectTransform(
		const SpriteRendererComponent& sprite,
		const TransformComponent& adjustedTransform
	);
	// UIパネル描画関数
	void CreateDockSpace();
	void DrawSceneHierarchy(SceneManagerContext* context);
	void DrawHierarchyNode(Entity entity, SceneContext* context, const std::unordered_set<Entity>& allEntities);
	void DrawInspector(SceneContext* context);
	void DrawAssetsBrowser();

	void ClearPreviewChache();

	void DrawDirectoryTree(const std::filesystem::path& directory, std::string& selectedPath);

	void DrawAssetsInDirectory(std::string & selectedPath);

	TextureData* GetIconTexture(std::string filepath);

	// メンバー変数
	bool openRename = false;
	std::filesystem::path renameTarget;
	char newNameBuffer[256]{};

	Entity selectedEntity = 0;
	Entity deleteEntity = 0;
	Entity pendingRenameEntity = 0;

	bool* showSceneHierarchy = nullptr;
	bool* showInspector = nullptr;
	bool* showConsole = nullptr;
	bool* showAssetsBrowser = nullptr;

	std::shared_ptr<TextureData> fileIcon[FileIconType::FILE_MAX];
	std::unordered_map<std::string, std::shared_ptr<TextureData>> previewCache;
};
