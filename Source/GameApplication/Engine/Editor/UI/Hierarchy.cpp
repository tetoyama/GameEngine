#include "Hierarchy.h"
#include <ImGui/imgui_internal.h>
#include <memory>
#include <vector>
#include <algorithm>
#include <typeindex>
#include <cinttypes>
#include <sceneManager.h>
#include "Editor/editorService.h"
#include "Editor/UI/MenuBar.h"
#include "Editor/Command/EntityCommand.h"
#include "Editor/Command/PrefabCommand.h"
#include <scene.h>
#include <Component/transformComponent.h>
#include <Component/entityNameComponent.h>
#include <Component/PrefabComponent.h>
#include "Prefab/PrefabSystem.h"
#include <commdlg.h>
#include <filesystem>
#include "Service/Config/configSystem.h"

void Hierarchy::Draw(EditorDrawContext ctx){

	ImGuiWindowClass window_class;
	window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
	ImGui::SetNextWindowClass(&window_class);
	bool* showSceneHierarchy = &m_editor->GetUI<MenuBar>()->showSceneHierarchy;
	SceneManagerContext* sceneManagerContext = m_editor->sceneManager->GetContext();
	std::shared_ptr<Scene> sceneToDelete = nullptr;

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

			// .prefab ファイルをヒエラルキーへドラッグアンドドロップ
			if(ImGui::BeginDragDropTarget()){
				if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")){
					if(payload->DataSize > 0 && context->prefab){
						std::string path(static_cast<const char*>(payload->Data), payload->DataSize - 1);
						if(std::filesystem::path(path).extension() == ".prefab"){
							auto cmd = std::make_unique<PrefabInstantiateCommand>(
								context, path, false,
								[this](EntityRef ref){
									if(ref){
										selectedEntity = ref.GetEntityID();
										sceneContext   = ref.GetScene();
									}
								});
							m_editor->commandManager.Execute(std::move(cmd));
						}
					}
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::SetCursorPos(ImVec2(10, ImGui::GetCursorPos().y));
			// ツールバー
			if(ImGui::Button("+ Add")){
				ImGui::OpenPopup("##AddEntityPopup");
			}
			if(ImGui::BeginPopup("##AddEntityPopup")){
				if(ImGui::MenuItem("Empty")){
					auto cmd = std::make_unique<EntityCreateCommand>(
						context, 0,
						[this](Entity e, SceneContext* ctx){
							selectedEntity = e;
							sceneContext    = ctx;
						});
					m_editor->commandManager.Execute(std::move(cmd));
				}
				if(ImGui::BeginMenu("Template")){
					ConfigService* cfg = m_editor->sceneManager->GetContext()->config;
					const std::string& tplDir = cfg ? cfg->appConfig.templateDir : APPCONFIG{}.templateDir;
					std::error_code ec;
					if(std::filesystem::exists(tplDir, ec) && !ec){
						for(const auto& entry : std::filesystem::directory_iterator(tplDir, ec)){
							if(ec) break;
							if(entry.path().extension() == ".prefab"){
								std::string stem = entry.path().stem().string();
								if(ImGui::MenuItem(stem.c_str())){
									if(context->prefab){
										std::string tplPath = entry.path().string();
										auto cmd = std::make_unique<PrefabInstantiateCommand>(
											context, tplPath, true,
											[this](EntityRef ref){
												if(ref){
													selectedEntity = ref.GetEntityID();
													sceneContext   = ref.GetScene();
												}
											});
										m_editor->commandManager.Execute(std::move(cmd));
									}
								}
							}
						}
					} else{
						ImGui::TextDisabled("(no templates found)");
					}
					ImGui::EndMenu();
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
							std::string prefabPath(filename);
							auto cmd = std::make_unique<PrefabInstantiateCommand>(
								context, prefabPath, false,
								[this](EntityRef ref){
									if(ref){
										selectedEntity = ref.GetEntityID();
										sceneContext   = ref.GetScene();
									}
								});
							m_editor->commandManager.Execute(std::move(cmd));
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

			ImGui::TreePop();
		}
	}
	ImGui::End();
}

void Hierarchy::DestroyEntityRecursive(Entity entity, SceneContext* context){
	// 親の children リストからこのエンティティを削除する
	auto* transform = context->component->GetComponent<TransformComponent>(entity);
	if(transform && transform->parent != 0){
		auto* parentTransform = context->component->GetComponent<TransformComponent>(transform->parent);
		if(parentTransform){
			auto& ch = parentTransform->children;
			ch.erase(std::remove(ch.begin(), ch.end(), entity), ch.end());
		}
	}
	// 子エンティティを先に収集（Destroy中にセットを変更しないため）
	std::vector<Entity> children;
	for(Entity child : context->entity->GetAllAlive()){
		auto* t = context->component->GetComponent<TransformComponent>(child);
		if(t && t->parent == entity){
			children.push_back(child);
		}
	}
	for(Entity child : children){
		DestroyEntityRecursive(child, context);
	}
	context->entity->Destroy(entity);
	context->component->OnEntityDestroyed(entity);
}

void Hierarchy::SetParent(Entity child, Entity newParent, SceneContext* context){
	auto* childTransform = context->component->GetComponent<TransformComponent>(child);
	if(!childTransform) return;

	// 旧親の children リストから削除
	Entity oldParent = childTransform->parent;
	if(oldParent != 0){
		auto* oldParentTransform = context->component->GetComponent<TransformComponent>(oldParent);
		if(oldParentTransform){
			auto& ch = oldParentTransform->children;
			ch.erase(std::remove(ch.begin(), ch.end(), child), ch.end());
		}
	}

	childTransform->parent = newParent;

	// 新親の children リストに追加
	if(newParent != 0){
		auto* newParentTransform = context->component->GetComponent<TransformComponent>(newParent);
		if(newParentTransform){
			newParentTransform->children.push_back(child);
		}
	}
}

Entity Hierarchy::DuplicateEntityRecursive(Entity src, Entity newParent, SceneContext* context){
	Entity newEntity = context->entity->Create();

	// YAML encode/decode で全コンポーネントをコピー
	const auto& idToName = context->component->GetComponentIDToNameMap();
	auto components = context->component->GetAllComponentsOfEntitySorted(src);
	for(IComponent* comp : components){
		ComponentTypeID typeID = context->component->GetComponentIDByTypeIndex(std::type_index(typeid(*comp)));
		if(typeID == static_cast<ComponentTypeID>(-1)) continue;
		auto nameIt = idToName.find(typeID);
		if(nameIt == idToName.end()) continue;
		YAML::Node node = comp->encode();
		context->component->CreateFromYAML(nameIt->second, newEntity, node);
	}

	// TransformComponent の parent/children を修正
	auto* newTransform = context->component->GetComponent<TransformComponent>(newEntity);
	if(newTransform){
		newTransform->children.clear();
		newTransform->parent = newParent;
		if(newParent != 0){
			auto* parentTransform = context->component->GetComponent<TransformComponent>(newParent);
			if(parentTransform){
				parentTransform->children.push_back(newEntity);
			}
		}
	}

	// 子エンティティを再帰的に複製
	auto* srcTransform = context->component->GetComponent<TransformComponent>(src);
	if(srcTransform){
		for(Entity srcChild : srcTransform->children){
			DuplicateEntityRecursive(srcChild, newEntity, context);
		}
	}

	return newEntity;
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
				// 空の親エンティティを作成し、選択エンティティをその子にする
				auto cmd = std::make_unique<EntityCreateCommand>(
					context, 0,
					[this, entity, context](Entity e, SceneContext*){
						SetParent(entity, e, context);
						selectedEntity = e;
						sceneContext   = context;
					});
				m_editor->commandManager.Execute(std::move(cmd));
			}
			if(ImGui::MenuItem("EmptyChild")){
				// 選択ノードの子エンティティを作成
				auto cmd = std::make_unique<EntityCreateCommand>(
					context, entity,
					[this, context](Entity e, SceneContext*){
						selectedEntity = e;
						sceneContext   = context;
					});
				m_editor->commandManager.Execute(std::move(cmd));
			}
			ImGui::EndMenu();
		}

		if(ImGui::MenuItem("複製")){
			auto* transform = context->component->GetComponent<TransformComponent>(entity);
			Entity newParent = transform ? transform->parent : 0;
			Entity dup = DuplicateEntityRecursive(entity, newParent, context);
			selectedEntity = dup;
			sceneContext = context;
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
			auto cmd = std::make_unique<EntityDeleteCommand>(
				context, entity,
				[this, entity](){
					if(selectedEntity == entity) selectedEntity = 0;
					if(pendingRenameEntity == entity) pendingRenameEntity = 0;
				},
				[this](Entity e, SceneContext* ctx){
					selectedEntity = e;
					sceneContext   = ctx;
				});
			m_editor->commandManager.Execute(std::move(cmd));

			ImGui::EndPopup();
			if(opened && hasChildren){
				ImGui::TreePop();
			}
			return;
		}

		ImGui::EndPopup();
	}

	// --- Drag & Drop ---
	// SourceAllowNullID is required because LastItemData.ID becomes 0 after BeginGroup
	if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)){
		ImGui::SetDragDropPayload("ENTITY_DRAG_DROP", &entity, sizeof(Entity));
		ImGui::Text("Move Entity");
		ImGui::EndDragDropSource();
	}
	if(ImGui::BeginDragDropTarget()){
		if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_DRAG_DROP")){
			IM_ASSERT(payload->DataSize == sizeof(Entity));
			Entity draggedEntity = *(const Entity*)payload->Data;
			if(draggedEntity != entity){
				auto* draggedT = context->component->GetComponent<TransformComponent>(draggedEntity);
				Entity oldParent = draggedT ? draggedT->parent : 0;
				auto cmd = std::make_unique<SetParentCommand>(context, draggedEntity, oldParent, entity);
				m_editor->commandManager.Execute(std::move(cmd));
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
