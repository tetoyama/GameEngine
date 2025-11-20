// GameEngine/Source/GameApplication/Engine/Scene/System/inspectorSystem.cpp
#define _CRT_SECURE_NO_WARNINGS // for strncpy

#include "inspectorSystem.h"
#include "../engine.h"
#include "../engineContext.h"
#include "../Registry/entityRegistry.h"
#include "../../../Backends/ImGui/imgui.h"
#include "../../../Backends/ImGui/imgui_internal.h" // for DockSpace
#include "../../../Backends/ImGui/ImGuizmo.h"
#include "Engine/Graphics/mainRenderer.h"

#include <set>
#include <Registry/componentRegistry.h>
#include <Component/transformComponent.h>
#include <Component/entityNameComponent.h>
#include <Component/meshRendererComponent.h>

#include "../scene.h"
#include "../sceneManager.h"
#include "Engine/DebugTools/ImGuiSystem.h"
#include "Engine/EditorUI/ImGuiMainManuBar.h"
#include <Component/textureComponent.h>
#include <Component/cameraComponent.h>
#include "Engine/DebugTools/DebugSystem.h" // Add this include to resolve the incomplete type issue
#include <Component/2DspriteRendererComponent.h>

#include "Engine/Resources/resourceService.h"

#include "Engine/Resources/Loader/modelLoader.h"
#include "Engine/Resources/Loader/shaderLoader.h"
#include "Engine/Resources/Loader/textureLoader.h"

#include "Engine/Resources/Data/modelData.h"
#include "Engine/Resources/Data/vertexShaderData.h"
#include "Engine/Resources/Data/pixelShaderData.h"
#include "Engine/Resources/Data/textureData.h"

#include "Backends/checkFileExtention.h"



TextureData* InspectorSystem::GetIconTexture(std::string filepath) {

	FileIconType type = FileIconType::FILE_UNDEFINED;

	//return fileIcon[type].get();


	if (filepath == "FOLDER") {
		type = FileIconType::FILE_FOLDER;
	}
	std::string ext = GetFileExtension(filepath);

	if (ext == ".wav") {

	}
	if (ext == ".txt") {
		type = FileIconType::FILE_TEXT;
	}
	if (ext == ".yaml") {
		type = FileIconType::FILE_YAML;
	}
	if (ext == ".fbx") {
		type = FileIconType::FILE_FBX;
	}
	if (ext == ".obj") {
		type = FileIconType::FILE_OBJ;
	}
	if (ext == ".ttf") {
		type = FileIconType::FILE_TTF;
	}
	if (ext == ".png" || ext == ".tga" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp") {
		// キャッシュされていればそれを返す（重複読み込みを避ける）
		auto it = previewCache.find(filepath);
		if (it != previewCache.end() && it->second->pTexture) {
			return it->second.get();
		}
		// 初回読み込み → テクスチャ作成
		auto tex = m_context->resource->Load<TextureData>(filepath);
		if (tex) {
			previewCache[filepath] = tex;
			tex.reset();
			return previewCache[filepath].get();
		}
	}

	return fileIcon[type].get();
}

void InspectorSystem::Initialize() {

	fileIcon[FileIconType::FILE_UNDEFINED] = m_context->resource->Load<TextureData>("Asset\\Texture\\UI\\FileIcon\\file_undefied.png");
	fileIcon[FileIconType::FILE_FOLDER] = m_context->resource->Load<TextureData>("Asset\\Texture\\UI\\FileIcon\\folder.png");
	fileIcon[FileIconType::FILE_TEXT] = m_context->resource->Load<TextureData>("Asset\\Texture\\UI\\FileIcon\\file_txt.png");
	fileIcon[FileIconType::FILE_YAML] = m_context->resource->Load<TextureData>("Asset\\Texture\\UI\\FileIcon\\file_yaml.png");
	fileIcon[FileIconType::FILE_FBX] = m_context->resource->Load<TextureData>("Asset\\Texture\\UI\\FileIcon\\file_fbx.png");
	fileIcon[FileIconType::FILE_OBJ] = m_context->resource->Load<TextureData>("Asset\\Texture\\UI\\FileIcon\\file_obj.png");
	fileIcon[FileIconType::FILE_TTF] = m_context->resource->Load<TextureData>("Asset\\Texture\\UI\\FileIcon\\file_ttf.png");

	m_InspectorContext = nullptr;
}

void InspectorSystem::Finalize() {
	for (int i = 0; i < FileIconType::FILE_MAX; i++) {
		fileIcon[i].reset();
	}
	ClearPreviewChache();
}

void InspectorSystem::ClearPreviewChache() {

	for (auto& [key, tex] : previewCache) {
		previewCache[key].reset();
		m_context->resource->Unload<TextureData>(key);
	}
	previewCache.clear();
}

// メインの更新関数
void InspectorSystem::Draw(){
	//CreateDockSpace();

	showSceneHierarchy = &m_context->imgui->GetManubar()->showSceneHierarchy;
	showInspector = &m_context->imgui->GetManubar()->showInspector;
	showAssetsBrowser = &m_context->imgui->GetManubar()->showAssetsBrowser;
	showConsole = &m_context->imgui->GetManubar()->showConsole;

	if(*showSceneHierarchy) DrawSceneHierarchy(m_context);
	if(*showInspector) DrawInspector(m_InspectorContext);
	if(*showAssetsBrowser) DrawAssetsBrowser();
}

// ドッキングスペースを作成
void InspectorSystem::CreateDockSpace(){
	static bool dockspaceOpen = true;
	static ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_None;

	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse;
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("DockSpace", &dockspaceOpen, windowFlags);
	ImGui::PopStyleVar(3);

	ImGuiIO& io = ImGui::GetIO();
	if(io.ConfigFlags & ImGuiConfigFlags_DockingEnable){
		ImGuiID dockspaceId = ImGui::GetID("MyDockSpace");
		ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockspaceFlags);
	}
	ImGui::End();
}

// シーンヒエラルキーウィンドウ
void InspectorSystem::DrawSceneHierarchy(SceneManagerContext* managerContext){

	ImGuiWindowClass window_class;
	window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
	ImGui::SetNextWindowClass(&window_class);

	std::shared_ptr<Scene> sceneToDelete = nullptr;

	//ImGuiWindowFlags toolbar_window_flags = ImGuiWindowFlags_NoCollapse;
	ImGuiWindowFlags toolbar_window_flags = 0;
	ImGui::Begin("Scene Hierarchy", showSceneHierarchy, toolbar_window_flags);

	for (auto& scenePair : managerContext->sceneManager->GetActiveScenes()) {

		SceneContext* context = scenePair.second->GetSceneContext();

		EntityRegistry* registry = context->entity;

		
		if (ImGui::TreeNodeEx(scenePair.first.c_str(),ImGuiTreeNodeFlags_DefaultOpen)) {

			if (ImGui::BeginPopupContextItem()) {

				if (ImGui::MenuItem("Save scene as...")) {
					std::string oldSavePath = scenePair.second->ScenePath;

					scenePair.second->ScenePath = "";

					scenePair.second->Save();

					scenePair.second->ScenePath = oldSavePath;

				}

				if (ImGui::MenuItem("Delete Scene")) {
					scenePair.second->isDestroy = true; // シーンを削除フラグ付きでマーク
				}

				ImGui::EndPopup();
			}

			ImGui::SetCursorPos(ImVec2(10,ImGui::GetCursorPos().y));
		// ツールバー
			if (ImGui::Button("+ Add")) {
				Entity newEntity = registry->Create(); // 新しいエンティティを追加
				selectedEntity = newEntity;
				NameComponent* name = context->component->AddComponent<NameComponent>(newEntity); // 名前コンポーネントを追加
				name->name = "Entity"; // デフォルトの名前を設定

				context->component->AddComponent<TransformComponent>(newEntity);
			}
			ImGui::SameLine();

			static char searchBuffer[256] = "";
			ImGui::SetNextItemWidth(-1);
			ImGui::InputTextWithHint("##search", "Search objects...", searchBuffer, sizeof(searchBuffer));

			ImGui::SetCursorPos(ImVec2(10, ImGui::GetCursorPos().y));
			ImGui::Separator();

			const auto& entities = registry->GetAllAlive();

			deleteEntity = 0;
			// --- ルートエンティティの描画（親を持たないもの） ---
			for (const Entity& entity : entities) {

				auto* transform = context->component->GetComponent<TransformComponent>(entity);
				if (transform && transform->parent != 0)
					continue;

				DrawHierarchyNode(entity, context, entities);
			}

			if (deleteEntity != 0) {
				context->entity->Destroy(deleteEntity);
				context->component->OnEntityDestroyed(deleteEntity);
				deleteEntity = 0;
			}

			ImGui::TreePop();
		}
	}
	ImGui::End();
}

char renameBuffer[256] = "";

void InspectorSystem::DrawHierarchyNode(Entity entity, SceneContext* context, const std::unordered_set<Entity>& allEntities) {

	float offsetX = ImGui::GetCursorPosX();

	ImGui::SetCursorPosX(10.0f);
	ImGui::Text(("ID : " + std::to_string(entity)).c_str());
	ImGui::SameLine(50.0f + offsetX * 0.25f);

	auto* name = context->component->GetComponent<NameComponent>(entity);
	std::string displayName = name ? name->name : "Entity";
	if (pendingRenameEntity != 0 && selectedEntity == entity && m_InspectorContext == context) {
		displayName = "";
	}
	// --- 子の有無チェック ---
	bool hasChildren = false;
	for (Entity child : allEntities) {
		auto* childTransform = context->component->GetComponent<TransformComponent>(child);
		if (childTransform && childTransform->parent == entity) {
			hasChildren = true;
			break;
		}
	}

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
		ImGuiTreeNodeFlags_DefaultOpen |
		((selectedEntity == entity && m_InspectorContext == context) ? ImGuiTreeNodeFlags_Selected : 0);

	if (!hasChildren) {
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}

	// --- ノード描画 ---
	bool opened = ImGui::TreeNodeEx((void*)(intptr_t)entity, flags, "%s", displayName.c_str());

	if (ImGui::IsItemClicked()) {
		selectedEntity = entity;
		m_InspectorContext = context;
	}
	// --- 右クリックメニュー ---
	if (ImGui::BeginPopupContextItem()) {

		if (ImGui::MenuItem("名前変更")) {
			pendingRenameEntity = entity;
			if (name) {
				strncpy(renameBuffer, name->name.c_str(), sizeof(renameBuffer));
				renameBuffer[sizeof(renameBuffer) - 1] = '\0';
			}
		}

		if (ImGui::BeginMenu("作成")) {
			if (ImGui::MenuItem("EmptyParent")) {
				// 子を持つ空の親エンティティを作成（例）
				Entity newEntity = context->entity->Create();
				auto* newtransform = context->component->AddComponent<TransformComponent>(newEntity);

				auto* transform = context->component->GetComponent<TransformComponent>(entity);
				if (transform) transform->parent = newEntity; // ルートに置く
			}
			if (ImGui::MenuItem("EmptyChild")) {
				// 選択ノードの子エンティティを作成
				Entity newEntity = context->entity->Create();
				auto* transform = context->component->AddComponent<TransformComponent>(newEntity);
				if (transform) transform->parent = entity;
			}
			ImGui::EndMenu();
		}

		if (ImGui::MenuItem("複製")) {
			//context->component->DuplicateEntity(entity);
		}

		if (ImGui::BeginMenu("Prefab")) {
			if (ImGui::MenuItem("Prefabとして保存")) {
				ImGui::OpenPopup("SavePrefabPopup");
			}
			if (ImGui::MenuItem("Prefabからリセット")) {
				// まだ処理未実装。プレースホルダ
			}
			if (ImGui::MenuItem("Prefabへ適用")) {
				// まだ処理未実装。プレースホルダ
			}
			ImGui::EndMenu();
		}

		if (ImGui::MenuItem("削除")) {

			deleteEntity = entity;
			if (selectedEntity == entity) {
				selectedEntity = 0;
			}

			//context->entity->Destroy(entity);
			//context->component->OnEntityDestroyed(entity);
			ImGui::EndPopup();
			if (opened) {
				//ImGui::TreePop();
			}
			return;
		}

		ImGui::EndPopup();
	}

	// --- Prefab保存ダイアログ（見た目だけ） ---
	if (ImGui::BeginPopup("SavePrefabPopup")) {
		ImGui::Text("Prefab saving is not implemented yet.");
		static char prefabName[128] = "";
		ImGui::InputText("Name", prefabName, sizeof(prefabName));
		if (ImGui::Button("OK")) {
			// 保存処理が実装されるまでは閉じるだけ
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	// --- 名前変更UI ---
	if (pendingRenameEntity != 0 && selectedEntity == entity && m_InspectorContext == context) {

		if (pendingRenameEntity != entity) {
			if (name) {
				strncpy(renameBuffer, name->name.c_str(), sizeof(renameBuffer));
				renameBuffer[sizeof(renameBuffer) - 1] = '\0';
			} else {
				renameBuffer[0] = '\0';
			}
			pendingRenameEntity = entity;
		}

		ImGui::SameLine();
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 10.0f);
		ImGui::PushItemWidth(80.0f);

		if (ImGui::InputText("##Rename", renameBuffer, sizeof(renameBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
			if (name) {
				name->name = renameBuffer;
			}
			pendingRenameEntity = 0;
		}
		ImGui::PopItemWidth();
	}

	// --- Drag & Drop ---
	if (ImGui::BeginDragDropSource()) {
		ImGui::SetDragDropPayload("ENTITY_DRAG_DROP", &entity, sizeof(Entity));
		ImGui::Text("Move Entity");
		ImGui::EndDragDropSource();
	}
	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_DRAG_DROP")) {
			IM_ASSERT(payload->DataSize == sizeof(Entity));
			Entity draggedEntity = *(const Entity*)payload->Data;
			if (draggedEntity != entity) {
				auto* transform = context->component->GetComponent<TransformComponent>(draggedEntity);
				if (transform) {
					transform->parent = entity;
				}
			}
		}
		ImGui::EndDragDropTarget();
	}

	// --- 子描画 ---
	if (opened && hasChildren) {
		for (Entity child : allEntities) {
			auto* childTransform = context->component->GetComponent<TransformComponent>(child);
			if (childTransform && childTransform->parent == entity) {
				DrawHierarchyNode(child, context, allEntities);
			}
		}
		ImGui::TreePop();
	}
}


// インスペクターウィンドウ
void InspectorSystem::DrawInspector(SceneContext* context){

	ImGuiWindowClass window_class;
	window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
	ImGui::SetNextWindowClass(&window_class);

	ImGui::Begin("Inspector", showInspector);


	if(selectedEntity == 0 || !m_InspectorContext){
		ImGui::Text("No object selected.");
		ImGui::End();
		return;
	} else 	if (!context || !context->entity) {
		selectedEntity = 0;
		ImGui::Text("No object selected.");
		ImGui::End();
		return;
	} else {
		bool alive = context->entity->IsAlive(selectedEntity); // 選択されたエンティティが生存しているか確認
		if(!alive){
			selectedEntity = 0; // 生存していない場合は選択を解除
			ImGui::End();

			return;
		}
	}

	auto* registry = context->component;

	// オブジェクト名とアクティブ状態
	auto nameComp = registry->GetComponent<NameComponent>(selectedEntity);
	static bool objectActive = true; // TODO: link to actual component property
	ImGui::Checkbox("##active", &objectActive);
	ImGui::SameLine();


	NameComponent* name = context->component->GetComponent<NameComponent>(selectedEntity);
	if(name){
        if (name) {
            // Convert std::string to char buffer for ImGui::InputText
            static char nameBuffer[256];
            strncpy(nameBuffer, name->name.c_str(), sizeof(nameBuffer));
            nameBuffer[sizeof(nameBuffer) - 1] = '\0'; // Ensure null termination

            if (ImGui::InputText("##Name", nameBuffer, sizeof(nameBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
                // Update the std::string with the modified buffer
                name->name = nameBuffer;
            }
        }
	}
	ImGui::SameLine();
	ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 60);

	if(ImGui::Button("- Delete")){
		if(selectedEntity != 0){
			context->entity->Destroy(selectedEntity);
			context->component->OnEntityDestroyed(selectedEntity);
			selectedEntity = 0;
		}
	}

	ImGui::Dummy(ImVec2(0, 10)); // 間隔を空ける
	ImGui::BeginChild("Child", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

	auto Components = registry->GetAllComponentsOfEntitySorted(selectedEntity);
	std::vector<IComponent*> componentsToRemove;
	// ツリーノードヘッダー用カラー
	ImVec4 headerBg = ImVec4(0.25f, 0.30f, 0.35f, 1.00f); // 通常時
	ImVec4 headerBgHover = ImVec4(0.35f, 0.45f, 0.55f, 1.00f); // ホバー時
	ImVec4 headerBgActive = ImVec4(0.30f, 0.40f, 0.50f, 1.00f); // 押下時
	ImVec4 headerText = ImVec4(0.90f, 0.90f, 0.90f, 1.00f); // テキスト
	auto& style = ImGui::GetStyle();
	auto drawList = ImGui::GetWindowDrawList();
	for(auto Component : Components){
		std::string compName = typeid(*Component).name();

		// 必要なImGui情報
		ImVec2 cursorPos = ImGui::GetCursorScreenPos();
		float fullWidth = ImGui::GetContentRegionAvail().x;
		float frameHeight = ImGui::GetFrameHeight();

		// 背景色
		ImU32 bgColor = ImGui::GetColorU32(ImGuiCol_Header);
		drawList->AddRectFilled(cursorPos, ImVec2(cursorPos.x + fullWidth, cursorPos.y + frameHeight), bgColor);

		// ツリーノードの展開矢印だけ表示（幅だけ取るためにNoTreePushOnOpen）
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		bool open = ImGui::TreeNodeEx((void*)Component, flags, "%s", compName.c_str());

		// Removeボタンは同じ行の右端に配置
		ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 60);
		if(ImGui::SmallButton(("Remove##" + compName).c_str())){
			componentsToRemove.push_back(Component);
		}

		// 次の行に移動
		if(open){
			Component->inspector(context);
		}
		ImGui::Dummy(ImVec2(0, 2.5f)); // 間隔を空ける

	}

	// 削除は後からまとめて
	for(IComponent* comp : componentsToRemove){
		std::type_index ti(typeid(*comp));
		ComponentTypeID id = context->component->GetComponentIDByTypeIndex(ti);
		if(id != static_cast<ComponentTypeID>(-1)){
			context->component->RemoveComponentByID(selectedEntity, id);
		}
	}


	// コンポーネント追加ボタン
	ImGui::Separator();
	if(ImGui::Button("+ Add Component", ImVec2(-1, 0))){
		ImGui::OpenPopup("AddComponentPopup");
	}
	if(ImGui::BeginPopup("AddComponentPopup")){
		static char componentSearchBuffer[128] = "";
		ImGui::InputTextWithHint("##ComponentSearch", "Search component...", componentSearchBuffer, sizeof(componentSearchBuffer));
		ImGui::Separator();

		const auto& addableMap = registry->GetAddableComponentList();

		// --- 名前順に並べ替える ---
		std::vector<std::pair<std::string, std::function<void(Entity)>>> addableListSorted(
			addableMap.begin(), addableMap.end()
		);

		std::sort(addableListSorted.begin(), addableListSorted.end(),
				  [](const auto& a, const auto& b){
					  return a.first < b.first;
				  });

		// --- マスク確認用 ---
		const auto& entityMasks = registry->GetEntityMasks();
		auto it = entityMasks.find(selectedEntity);
		ComponentMask currentMask;
		if(it != entityMasks.end()){
			currentMask = it->second;
		}

		// --- 検索小文字化 ---
		std::string searchLower = componentSearchBuffer;
		std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

		// --- メニュー表示 ---
		for(const auto& [name, func] : addableListSorted){
			ComponentTypeID typeID = registry->GetComponentIDByName(name);
			if(currentMask.test(typeID)) continue;

			std::string nameLower = name;
			std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

			if(!searchLower.empty() && nameLower.find(searchLower) == std::string::npos){
				continue;
			}

			if(ImGui::MenuItem(name.c_str())){
				func(selectedEntity);
			}
		}

		ImGui::EndPopup();
	}



	TransformComponent* transform = registry->GetComponent<TransformComponent>(selectedEntity);

	if(transform && m_context->imgui->GetManubar()->showEditorView){

		DirectX::XMMATRIX World = transform->CalculateWorldMatrix(transform,context->component);

		DirectX::XMMATRIX modelMatrix;

		auto* sprite = registry->GetComponent<SpriteRendererComponent>(selectedEntity);

		if(sprite){
			TransformComponent temp = CalculateRectTransform(*sprite, *transform);
			DirectX::XMMATRIX Rotation = DirectX::XMMatrixRotationRollPitchYaw(temp.GetRotationEuler().x, temp.GetRotationEuler().y, temp.GetRotationEuler().z);
			DirectX::XMMATRIX Scale = DirectX::XMMatrixScaling(temp.scale.x, temp.scale.y, temp.scale.z);
			DirectX::XMMATRIX Translation = DirectX::XMMatrixTranslation(temp.position.x, temp.position.y, temp.position.z);

			World = Scale * Rotation * Translation;

			modelMatrix = m_context->imgui->RenderGizmo2D(World);

		} else{
			modelMatrix = m_context->imgui->RenderGizmo(World);
		}
		Entity Parent = transform->parent;
		while (Parent != 0) {
			auto* ParentTransform = registry->GetComponent<TransformComponent>(Parent);
			if (ParentTransform) {

				DirectX::XMMATRIX ParentWorld = ParentTransform->CalculateWorldMatrix(ParentTransform, context->component);

				modelMatrix = modelMatrix * DirectX::XMMatrixInverse(nullptr, ParentWorld);

				Parent = ParentTransform->parent;
			} else {
				Parent = 0;
			}
		}
		if(ImGuizmo::IsUsing()){
			// スケール、回転、並進を格納する変数
			DirectX::XMVECTOR scale, rotationQuat, translation;

			// 行列を分解
			DirectX::XMMatrixDecompose(&scale, &rotationQuat, &translation, modelMatrix);

			// XMVECTOR から XMFLOAT3 に変換
			DirectX::XMFLOAT3 scale3, translation3;
			DirectX::XMStoreFloat3(&scale3, scale);
			DirectX::XMStoreFloat3(&translation3, translation);

			// rotationQuat はクォータニオンなのでそのまま XMFLOAT4 に書き出し
			DirectX::XMFLOAT4 quat;
			DirectX::XMStoreFloat4(&quat, rotationQuat);

			if(sprite){
				TransformComponent edited;
				edited.position = translation3;
				edited.SetRotation(quat);     // クォータニオンを代入
				edited.scale = scale3;

				edited = ReverseCalculateRectTransform(*sprite, edited);

				transform->position = edited.position;
				transform->SetRotation(edited.GetRotation());
				transform->scale = edited.scale;
			} else{
				transform->position = translation3;
				transform->SetRotation(quat); // クォータニオンを保持
				transform->scale = scale3;
			}
		

		}
	}
	ImGui::EndChild();
	ImGui::End();
}

void InspectorSystem::DrawDirectoryTree(const std::filesystem::path& directory, std::string& selectedPath){
	if(!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)){
		return;
	}

	// === ディレクトリ一覧を収集＆ソート ===
	std::vector<std::filesystem::directory_entry> directories;
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
		std::string name = path.filename().string();

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;

		// サブディレクトリを持っているかチェック
		bool hasSubDir = false;
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
		bool isSelected = (selectedPath == path.string());
		if(isSelected){
			flags |= ImGuiTreeNodeFlags_Selected;
		}
		if(isSelected){
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.5f, 0.9f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.4f, 0.6f, 1.0f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
		}

		ImGui::PushID(path.string().c_str());

		bool opened = ImGui::TreeNodeEx(name.c_str(), flags);
		if(isSelected){
			ImGui::PopStyleColor(3);
		}
		// 右クリックメニュー（そのアイテムを右クリック）
		if(ImGui::BeginPopupContextItem()){
			if(ImGui::MenuItem("新しいフォルダを作成")){
				// 例：<path>/NewFolder のように作成
				std::filesystem::path newFolder = path / "NewFolder";
				int index = 1;
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
				std::error_code ec;
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
			std::error_code ec;
			if(std::filesystem::exists(path, ec) && std::filesystem::is_directory(path, ec)){
				selectedPath = path.string();
				ClearPreviewChache();
			}
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



void InspectorSystem::DrawAssetsInDirectory(std::string& selectedPath){
	std::filesystem::path path = selectedPath;
	std::error_code ec;
	if(!std::filesystem::exists(path, ec) || !std::filesystem::is_directory(path, ec)) return;

	ImGui::Text("%s", selectedPath.c_str());

	// 検索バー
	float SearchWidth = 120.0f;
	if(ImGui::GetWindowContentRegionMax().x * 0.3f > SearchWidth){
		SearchWidth = ImGui::GetWindowContentRegionMax().x * 0.3f;
	}
	ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - SearchWidth);
	ImGui::PushItemWidth(SearchWidth);
	static char searchBuffer[256] = "";
	ImGui::InputTextWithHint("##AssetSearch", "Search files...", searchBuffer, sizeof(searchBuffer));
	ImGui::PopItemWidth();

	ImGui::Separator();
	ImGui::BeginChild("Child", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

	float itemSize = 70.0f;
	float panelWidth = ImGui::GetContentRegionAvail().x;

	float padding = 5.0f; // マージン
	float totalItemWidth = itemSize + padding;

	int columnsCount = static_cast<int>(panelWidth / totalItemWidth);
	if (columnsCount < 1) columnsCount = 1;

	// エントリ収集
	std::vector<std::filesystem::directory_entry> entries;
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
	std::string searchStr = searchBuffer;
	std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);

	int index = 0;
	for(const auto& entry : entries){
		std::string filename = entry.path().filename().string();

		// フィルタ処理
		if(!searchStr.empty()){
			std::string lowerFilename = filename;
			std::transform(lowerFilename.begin(), lowerFilename.end(), lowerFilename.begin(), ::tolower);
			if(lowerFilename.find(searchStr) == std::string::npos)
				continue;
		}

		if(index % columnsCount != 0)
			ImGui::SameLine(0.0f, (panelWidth - (itemSize)*columnsCount) / columnsCount);

		ImGui::PushID(entry.path().string().c_str());
		ImGui::BeginGroup();

		ImVec2 CursorPos = ImGui::GetCursorPos();

		// ディレクトリなら別アイコン・クリック時にパス移動
		if(entry.is_directory(ec)){
			if(ImGui::Button("##Folder", ImVec2(itemSize, itemSize))) {

				ClearPreviewChache();

				selectedPath = entry.path().string();
			}
		} else{
			ImGui::Button("##ICON", ImVec2(itemSize, itemSize));

			if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)){
				std::string fullPath = entry.path().string();
				ImGui::SetDragDropPayload("ASSET_PATH", fullPath.c_str(), fullPath.size() + 1);
				ImGui::Text("Drag: %s", filename.c_str());
				ImGui::EndDragDropSource();
			}
		}
		ImVec2 AfterCursorPos = ImGui::GetCursorPos();

		std::string path = entry.path().string();
		if (entry.is_directory(ec)) {
			path = "FOLDER";
		}
		if (GetIconTexture(path)) {
			ImVec2 IconSize = ImVec2((float)GetIconTexture(path)->Width, (float)GetIconTexture(path)->Height);
			if (IconSize.x < IconSize.y) {
				IconSize.x = IconSize.x * itemSize / IconSize.y;
				IconSize.y = itemSize;
				ImGui::SetCursorPos(ImVec2(CursorPos.x + (itemSize - IconSize.x) * 0.5f, CursorPos.y));
			} else {
				IconSize.y = IconSize.y * itemSize / IconSize.x;
				IconSize.x = itemSize;
				ImGui::SetCursorPos(ImVec2(CursorPos.x, CursorPos.y + (itemSize - IconSize.y) * 0.5f));
			}
			ImVec4 ImageColor = ImVec4(1, 1, 1, 1);
			if (ImGui::IsItemHovered()) {
				ImageColor = ImVec4(1, 1, 1, 0.5f);
			}
			if (ImGui::IsItemActive()) {
				ImVec4(1, 1, 1, 0.5f);
			}
			ImGui::Image(GetIconTexture(path)->pTexture.Get(), IconSize, ImVec2(0, 0), ImVec2(1, 1), ImageColor, ImVec4(0, 0, 0, 0));
			ImGui::SetCursorPos(AfterCursorPos);
		}
		std::string displayName = filename;
		ImVec2 textSize = ImGui::CalcTextSize(displayName.c_str());
		float maxTextWidth = itemSize;

		if (textSize.x > maxTextWidth) {
			int len = static_cast<int>(displayName.size());
			while (len > 0 && ImGui::CalcTextSize((displayName.substr(0, len) + "...").c_str()).x > maxTextWidth) {
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


void InspectorSystem::DrawAssetsBrowser(){
	std::filesystem::path ASSETS_ROOT = "Asset";
	static std::string selectedPath = ASSETS_ROOT.string();
	//m_context->debug->LOG_INFO("Current path: " + std::filesystem::current_path().string());

	// --- パス検証・正規化 ---
	std::error_code ec;
	std::filesystem::path normalized = std::filesystem::weakly_canonical(selectedPath, ec);

	if(ec || !std::filesystem::exists(normalized) || !std::filesystem::is_directory(normalized)){
		// フォールバック + ログ出力
		m_context->debug->LOG_ERROR("Invalid selectedPath: " + selectedPath +
											 "\n→ normalized: " + normalized.string() +
											 "\n→ ec: " + std::to_string(ec.value()));
		selectedPath = ASSETS_ROOT.string();
	}

	// === ImGui Begin ===
	
	ImGuiWindowClass window_class;
	window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
	ImGui::SetNextWindowClass(&window_class);

	//ImGuiWindowFlags toolbar_window_flags = ImGuiWindowFlags_NoCollapse;
	ImGuiWindowFlags toolbar_window_flags = 0;
	ImGui::Begin("Assets Browser", showAssetsBrowser, toolbar_window_flags);

	ImGui::Columns(2, "AssetColumns", true);

	static bool firstFrame = true;
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
				ClearPreviewChache();
				selectedPath = ASSETS_ROOT.string(); // ルート選択
			}
			// --- 右クリックメニュー追加 ---
			if(ImGui::BeginPopupContextItem()){
				if(ImGui::MenuItem("新しいフォルダを作成")){
					std::filesystem::path newFolder = ASSETS_ROOT / "NewFolder";
					int index = 1;
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



TransformComponent InspectorSystem::CalculateRectTransform(
	const SpriteRendererComponent& sprite,
	const TransformComponent& originalTransform
){
	TransformComponent adjustedTransform = originalTransform;

	Vector2 screenSize = m_context->EditorScreenSize;
	Vector2 viewportSize = {
		(float)m_context->renderer->GetGraphicsContext()->m_width,
		(float)m_context->renderer->GetGraphicsContext()->m_height
	};

	// 仮想UI基準解像度
	const Vector2 referenceResolution = {1.0f, 1.0f};

	// アスペクト比
	float screenAspect = screenSize.x / screenSize.y;
	float referenceAspect = referenceResolution.x / referenceResolution.y;
	float aspectRatioScaleX = referenceAspect / screenAspect;

	// アンカー位置（画面サイズ基準）
	Vector2 anchoredPosition = {
		viewportSize.x * sprite.anchor.x,
		viewportSize.y * sprite.anchor.y
	};

	// スプライトサイズ（ピクセル単位）
	Vector2 adjustedScale = {
		originalTransform.scale.x * aspectRatioScaleX / referenceResolution.x * viewportSize.x,
		originalTransform.scale.y / referenceResolution.y * viewportSize.y
	};

	// ピボット補正（ピクセル単位）
	Vector2 pivotOffset = {
		adjustedScale.x * -sprite.pivot.x,
		adjustedScale.y * -sprite.pivot.y
	};

	// 仮想座標オフセット（position）→ ピクセルスケーリング ＋ アスペクト比補正付き
	Vector2 positionOffset = {
		originalTransform.position.x * aspectRatioScaleX / referenceResolution.x * viewportSize.x,
		originalTransform.position.y / referenceResolution.y * viewportSize.y
	};

	// 最終位置
	Vector2 finalPosition = {
		anchoredPosition.x - pivotOffset.x + positionOffset.x,
		anchoredPosition.y - pivotOffset.y + positionOffset.y
	};

	adjustedTransform.position = Vector3(finalPosition.x, finalPosition.y, originalTransform.position.z);
	adjustedTransform.scale = Vector3(adjustedScale.x, adjustedScale.y, originalTransform.scale.z);

	return adjustedTransform;
}
TransformComponent InspectorSystem::ReverseCalculateRectTransform(
	const SpriteRendererComponent& sprite,
	const TransformComponent& adjustedTransform
){
	TransformComponent virtualTransform = adjustedTransform;

	Vector2 screenSize = m_context->EditorScreenSize;
	Vector2 viewportSize = {
		(float)m_context->renderer->GetGraphicsContext()->m_width,
		(float)m_context->renderer->GetGraphicsContext()->m_height
	};

	const Vector2 referenceResolution = {1.0f, 1.0f};

	// アスペクト比
	float screenAspect = screenSize.x / screenSize.y;
	float referenceAspect = referenceResolution.x / referenceResolution.y;
	float aspectRatioScaleX = referenceAspect / screenAspect;

	// アンカー位置（ピクセル）
	Vector2 anchoredPosition = {
		viewportSize.x * sprite.anchor.x,
		viewportSize.y * sprite.anchor.y
	};

	// 実スケール → 仮想スケールに変換
	Vector2 virtualScale = {
		adjustedTransform.scale.x / viewportSize.x * referenceResolution.x / aspectRatioScaleX,
		adjustedTransform.scale.y / viewportSize.y * referenceResolution.y
	};

	// ピボット補正（ピクセル）
	Vector2 pivotOffset = {
		adjustedTransform.scale.x * -sprite.pivot.x,
		adjustedTransform.scale.y * -sprite.pivot.y
	};

	// 実座標 → アンカー基準に差分変換（ピクセル空間）
	Vector2 anchoredDiff = {
		adjustedTransform.position.x - anchoredPosition.x + pivotOffset.x,
		adjustedTransform.position.y - anchoredPosition.y + pivotOffset.y
	};

	// ピクセル → 仮想UI座標に逆変換
	Vector2 virtualPosition = {
		anchoredDiff.x / viewportSize.x * referenceResolution.x / aspectRatioScaleX,
		anchoredDiff.y / viewportSize.y * referenceResolution.y
	};

	// 最終反映
	virtualTransform.position = Vector3(virtualPosition.x, virtualPosition.y, adjustedTransform.position.z);
	virtualTransform.scale = Vector3(virtualScale.x, virtualScale.y, adjustedTransform.scale.z);

	return virtualTransform;
}
