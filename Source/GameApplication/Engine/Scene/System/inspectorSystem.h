// GameEngine/Source/GameApplication/Engine/Scene/System/inspectorSystem.h
#pragma once
#include "../Interface/ISystem.h"
#include "../Entity/Entity.h" // Entityの定義をインクルード

#include "../../../Backends/ImGui/imgui.h"

#include <string>
#include <filesystem>
#include <memory>
#include <unordered_map>

struct SceneContext;
class TransformComponent;
class SpriteRendererComponent;

struct TextureData;

enum FileIconType {
	UNDEFINED = 0,
	FOLDER,
	MAX,
};

class InspectorSystem: public ISystem
{
public:
	InspectorSystem(SceneContext* context): m_context(context){}
	~InspectorSystem(){}
	void Initialize() override;
	void Finalize()override;

	void Start() override{}
	void Update(float deltaTime) override{}
	void FixedUpdate(float fidedDeltaTime) override{}
	void Draw() override;
	void EditorUpdate(float deltaTime) override{}

private:
	SceneContext* m_context;

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
	void DrawSceneHierarchy(SceneContext* context);
	void DrawInspector(SceneContext* context);
	void DrawAssetsBrowser();


	void DrawDirectoryTree(const std::filesystem::path& directory, std::string& selectedPath);

	void DrawAssetsInDirectory(std::string & selectedPath);

	TextureData* GetIconTexture(std::string filepath);

	// メンバー変数
	Entity selectedEntity = 0;

	bool* showSceneHierarchy = nullptr;
	bool* showInspector = nullptr;
	bool* showConsole = nullptr;
	bool* showAssetsBrowser = nullptr;


	TextureData* fileIcon[FileIconType::MAX]{};
	std::unordered_map<std::string, TextureData*> previewCache;

};