#include "Hierarchy.h"
#include <ImGui/imgui_internal.h>
#include <memory>
#include <cinttypes>
#include <sceneManager.h>
#include "Editor/editorService.h"
#include "Editor/UI/MenuBar.h"
#include <scene.h>
#include <Component/transformComponent.h>
#include <Component/entityNameComponent.h>
#include <Component/PrefabComponent.h>
#include "Prefab/PrefabSystem.h"
#include <commdlg.h>
#include <filesystem>

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
				ImGui::OpenPopup("##AddEntityPopup");
			}
			if(ImGui::BeginPopup("##AddEntityPopup")){
				if(ImGui::MenuItem("Empty")){
					Entity newEntity = registry->Create();
					selectedEntity = newEntity;
					sceneContext = context;
					auto* n = context->component->AddComponent<NameComponent>(newEntity);
					n->name = "Entity";
					context->component->AddComponent<TransformComponent>(newEntity);
				}
				if(ImGui::MenuItem("Template")){
					Entity newEntity = registry->Create();
					selectedEntity = newEntity;
					sceneContext = context;
					auto* n = context->component->AddComponent<NameComponent>(newEntity);
					n->name = "Template";
					context->component->AddComponent<TransformComponent>(newEntity);
				}
				if(ImGui::MenuItem("Prefab")){
					if(context->prefab){
						OPENFILENAMEA ofn = {};
						char filename[MAX_PATH] = "";
						ofn.lStructSize = sizeof(ofn);
						ofn.lpstrFilter = "Prefab Files (*.prefab)\0*.prefab\0All Files (*.*)\0*.*\0";
						ofn.lpstrFile = filename;
						ofn.nMaxFile = MAX_PATH;
						ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
						ofn.lpstrDefExt = "prefab";
						if(GetOpenFileNameA(&ofn)){
							EntityRef spawned = context->prefab->InstantiatePrefab(context, std::string(filename));
							if(spawned){
								selectedEntity = spawned.GetEntityID();
								sceneContext = spawned.GetScene();
							}
						}
					}
				}
				ImGui::EndPopup();
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

	// --- ノード描画（グループで DnD エリアを (Prefab) ラベルまで拡張） ---
	bool inRenameMode = (pendingRenameEntity != 0 && selectedEntity == entity && sceneContext == context);
	ImGui::BeginGroup();
	bool opened = ImGui::TreeNodeEx((void*)(intptr_t)entity, flags, "%s", displayName.c_str());
	if(!inRenameMode && context->component->GetComponent<PrefabComponent>(entity)){
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "(Prefab)");
	}
	ImGui::EndGroup();

	if(ImGui::IsItemClicked()){
		selectedEntity = entity;
		sceneContext = context;
	}
	// --- 右クリックメニュー ---
	char popupId[32];
	snprintf(popupId, sizeof(popupId), "##NodeCtx%" PRIu32, (uint32_t)entity);
	if(ImGui::BeginPopupContextItem(popupId)){

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
				if(context->prefab){
					auto* nameComp = context->component->GetComponent<NameComponent>(entity);
					std::string defaultName = ((nameComp && !nameComp->name.empty()) ? nameComp->name : "Entity") + ".prefab";
					char szFile[MAX_PATH] = {};
					strncpy(szFile, defaultName.c_str(), MAX_PATH - 1);

					OPENFILENAMEA ofn = {};
					ofn.lStructSize = sizeof(ofn);
					ofn.lpstrFilter = "Prefab Files (*.prefab)\0*.prefab\0All Files (*.*)\0*.*\0";
					ofn.lpstrFile = szFile;
					ofn.nMaxFile = MAX_PATH;
					ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
					ofn.lpstrDefExt = "prefab";
					if(GetSaveFileNameA(&ofn)){
						std::string dir = std::filesystem::path(szFile).parent_path().string();
						if(!dir.empty()) std::filesystem::create_directories(dir);
						context->prefab->SavePrefab(EntityRef(entity, context), std::string(szFile));
					}
				}
			}
			auto* prefabComp = context->component->GetComponent<PrefabComponent>(entity);
			bool hasPrefabSource = prefabComp && !prefabComp->filePath.empty();
			if(ImGui::MenuItem("Prefabを上書き", nullptr, false, hasPrefabSource)){
				if(hasPrefabSource && context->prefab){
					context->prefab->SavePrefab(EntityRef(entity, context), prefabComp->filePath);
				}
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

	// --- 名前変更UI ---
	if(inRenameMode){

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
