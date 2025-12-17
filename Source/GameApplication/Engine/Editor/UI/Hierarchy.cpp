#define _CRT_SECURE_NO_WARNINGS // for strncpy

#include "Hierarchy.h"
#include <ImGui/imgui_internal.h>
#include <memory>
#include <sceneManager.h>
#include "Editor/editorService.h"
#include "Editor/UI/MenuBar.h"
#include <scene.h>
#include <Component/transformComponent.h>
#include <Component/entityNameComponent.h>

void Hierarchy::Draw(EditorDrawContext ctx){

	ImGuiWindowClass window_class;
	window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
	ImGui::SetNextWindowClass(&window_class);
	bool* showSceneHierarchy = &m_editor->GetUI<MenuBar>()->showSceneHierarchy;
	SceneManagerContext* sceneManagerContext = m_editor->sceneManager->GetContext();
	std::shared_ptr<Scene> sceneToDelete = nullptr;
	Entity deleteEntity = 0;

	if(!showSceneHierarchy || !*showSceneHierarchy){
		return;
	}

	//ImGuiWindowFlags toolbar_window_flags = ImGuiWindowFlags_NoCollapse;
	ImGuiWindowFlags toolbar_window_flags = 0;
	ImGui::Begin("Hierarchy", showSceneHierarchy, toolbar_window_flags);

	for(auto& scenePair : m_editor->sceneManager->GetActiveScenes()){

		SceneContext* context = scenePair.second->GetSceneContext();

		EntityRegistry* registry = context->entity;

		if(ImGui::TreeNodeEx((scenePair.second->SceneName + "##" + scenePair.first).c_str(), ImGuiTreeNodeFlags_DefaultOpen)){

			if(ImGui::BeginPopupContextItem()){

				if(ImGui::MenuItem("Save scene as...")){
					std::string oldSavePath = scenePair.second->ScenePath;

					scenePair.second->ScenePath = "";

					scenePair.second->Save();

					scenePair.second->ScenePath = oldSavePath;

				}

				if(ImGui::MenuItem("Delete Scene")){
					scenePair.second->isDestroy = true; // シーンを削除フラグ付きでマーク
					selectedEntity = 0;
				}

				ImGui::EndPopup();
			}

			ImGui::SetCursorPos(ImVec2(10, ImGui::GetCursorPos().y));
			// ツールバー
			if(ImGui::Button("+ Add")){
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
			for(const Entity& entity : entities){

				auto* transform = context->component->GetComponent<TransformComponent>(entity);
				if(transform && transform->parent != 0){
					continue;
				}
				DrawHierarchyNode(entity, context, entities);
			}

			if(deleteEntity != 0){
				context->entity->Destroy(deleteEntity);
				context->component->OnEntityDestroyed(deleteEntity);
				deleteEntity = 0;
			}

			ImGui::TreePop();
		}
	}
	ImGui::End();
}

void Hierarchy::DrawHierarchyNode(Entity entity, SceneContext* context, const std::unordered_set<Entity>& allEntities){
	float offsetX = ImGui::GetCursorPosX();

	ImGui::SetCursorPosX(10.0f);
	ImGui::Text(("ID : " + std::to_string(entity)).c_str());
	ImGui::SameLine(50.0f + offsetX * 0.25f);

	auto* name = context->component->GetComponent<NameComponent>(entity);
	std::string displayName = name ? name->name : "Entity";
	if(pendingRenameEntity != 0 && selectedEntity == entity && sceneContext == context){
		displayName = "";
	}
	// --- 子の有無チェック ---
	bool hasChildren = false;
	for(Entity child : allEntities){
		auto* childTransform = context->component->GetComponent<TransformComponent>(child);
		if(childTransform && childTransform->parent == entity){
			hasChildren = true;
			break;
		}
	}

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
		ImGuiTreeNodeFlags_DefaultOpen |
		((selectedEntity == entity && sceneContext == context) ? ImGuiTreeNodeFlags_Selected : 0);

	if(!hasChildren){
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}

	// --- ノード描画 ---
	bool opened = ImGui::TreeNodeEx((void*)(intptr_t)entity, flags, "%s", displayName.c_str());

	if(ImGui::IsItemClicked()){
		selectedEntity = entity;
		sceneContext = context;
	}
	// --- 右クリックメニュー ---
	if(ImGui::BeginPopupContextItem()){

		if(ImGui::MenuItem("名前変更")){
			pendingRenameEntity = entity;
			if(name){
				strncpy(renameBuffer, name->name.c_str(), sizeof(renameBuffer));
				renameBuffer[sizeof(renameBuffer) - 1] = '\0';
			}
		}

		if(ImGui::BeginMenu("作成")){
			if(ImGui::MenuItem("EmptyParent")){
				// 子を持つ空の親エンティティを作成（例）
				Entity newEntity = context->entity->Create();
				auto* newtransform = context->component->AddComponent<TransformComponent>(newEntity);

				auto* transform = context->component->GetComponent<TransformComponent>(entity);
				if(transform) transform->parent = newEntity; // ルートに置く
			}
			if(ImGui::MenuItem("EmptyChild")){
				// 選択ノードの子エンティティを作成
				Entity newEntity = context->entity->Create();
				auto* transform = context->component->AddComponent<TransformComponent>(newEntity);
				if(transform) transform->parent = entity;
			}
			ImGui::EndMenu();
		}

		if(ImGui::MenuItem("複製")){
			//context->component->DuplicateEntity(entity);
		}

		if(ImGui::BeginMenu("Prefab")){
			if(ImGui::MenuItem("Prefabとして保存")){
				ImGui::OpenPopup("SavePrefabPopup");
			}
			if(ImGui::MenuItem("Prefabからリセット")){
				// まだ処理未実装。プレースホルダ
			}
			if(ImGui::MenuItem("Prefabへ適用")){
				// まだ処理未実装。プレースホルダ
			}
			ImGui::EndMenu();
		}

		if(ImGui::MenuItem("削除")){

			deleteEntity = entity;
			if(selectedEntity == entity){
				selectedEntity = 0;
			}

			//context->entity->Destroy(entity);
			//context->component->OnEntityDestroyed(entity);
			ImGui::EndPopup();
			if(opened){
				//ImGui::TreePop();
			}
			return;
		}

		ImGui::EndPopup();
	}

	// --- Prefab保存ダイアログ（見た目だけ） ---
	if(ImGui::BeginPopup("SavePrefabPopup")){
		ImGui::Text("Prefab saving is not implemented yet.");
		static char prefabName[128] = "";
		ImGui::InputText("Name", prefabName, sizeof(prefabName));
		if(ImGui::Button("OK")){
			// 保存処理が実装されるまでは閉じるだけ
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if(ImGui::Button("Cancel")){
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	// --- 名前変更UI ---
	if(pendingRenameEntity != 0 && selectedEntity == entity && sceneContext == context){

		if(pendingRenameEntity != entity){
			if(name){
				strncpy(renameBuffer, name->name.c_str(), sizeof(renameBuffer));
				renameBuffer[sizeof(renameBuffer) - 1] = '\0';
			} else{
				renameBuffer[0] = '\0';
			}
			pendingRenameEntity = entity;
		}

		ImGui::SameLine();
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 10.0f);
		ImGui::PushItemWidth(80.0f);

		if(ImGui::InputText("##Rename", renameBuffer, sizeof(renameBuffer), ImGuiInputTextFlags_EnterReturnsTrue)){
			if(name){
				name->name = renameBuffer;
			}
			pendingRenameEntity = 0;
		}
		ImGui::PopItemWidth();
	}

	// --- Drag & Drop ---
	if(ImGui::BeginDragDropSource()){
		ImGui::SetDragDropPayload("ENTITY_DRAG_DROP", &entity, sizeof(Entity));
		ImGui::Text("Move Entity");
		ImGui::EndDragDropSource();
	}
	if(ImGui::BeginDragDropTarget()){
		if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_DRAG_DROP")){
			IM_ASSERT(payload->DataSize == sizeof(Entity));
			Entity draggedEntity = *(const Entity*)payload->Data;
			if(draggedEntity != entity){
				auto* transform = context->component->GetComponent<TransformComponent>(draggedEntity);
				if(transform){
					transform->parent = entity;
				}
			}
		}
		ImGui::EndDragDropTarget();
	}

	// --- 子描画 ---
	if(opened && hasChildren){
		for(Entity child : allEntities){
			auto* childTransform = context->component->GetComponent<TransformComponent>(child);
			if(childTransform && childTransform->parent == entity){
				DrawHierarchyNode(child, context, allEntities);
			}
		}
		ImGui::TreePop();
	}
}
