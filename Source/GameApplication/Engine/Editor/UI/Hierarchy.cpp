// =======================================================================
// 
// Hierarchy.cpp
// 
// =======================================================================
#include "Hierarchy.h"
#include <ImGui/imgui_internal.h>
#include <memory>
#include <vector>
#include <algorithm>
#include <typeindex>
#include <cinttypes>
#include <cstring>
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

// ------------------------------------------------------------
// 検索文字列を小文字化
// ------------------------------------------------------------
static std::string ToLower(const std::string& s){
	std::string r = s;
	std::transform(r.begin(), r.end(), r.begin(),
				   [](unsigned char c){ return (char)std::tolower(c); });
	return r;
}

// ------------------------------------------------------------
// Entity が検索にヒットするか
// ------------------------------------------------------------
static bool EntityMatchesSearch(
	Entity entity,
	SceneContext* context,
	const std::string& search){
	if(search.empty()) return true;

	auto* name = context->component->GetComponent<NameComponent>(entity);

	std::string entityName = name ? name->name : "Entity";

	std::string lowerName = ToLower(entityName);
	std::string lowerSearch = ToLower(search);

	return lowerName.find(lowerSearch) != std::string::npos;
}

// ------------------------------------------------------------
// 子が検索にヒットするか（再帰）
// ------------------------------------------------------------
static bool HasMatchingChild(
	Entity entity,
	SceneContext* context,
	const std::unordered_set<Entity>& allEntities,
	const std::string& search){
	for(Entity child : allEntities){
		auto* t = context->component->GetComponent<TransformComponent>(child);
		if(!t) continue;

		if(t->parent == entity){
			if(EntityMatchesSearch(child, context, search))
				return true;

			if(HasMatchingChild(child, context, allEntities, search))
				return true;
		}
	}

	return false;
}

void Hierarchy::Draw(const EditorDrawContext ctx){

	ImGuiWindowClass window_class;
	window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
	ImGui::SetNextWindowClass(&window_class);
	bool* showSceneHierarchy = &m_editor->GetUI<MenuBar>()->showSceneHierarchy;

	if(!showSceneHierarchy || !*showSceneHierarchy){
		return;
	}

	//ImGuiWindowFlags toolbar_window_flags = ImGuiWindowFlags_NoCollapse;
	ImGuiWindowFlags toolbar_window_flags = 0;
	ImGui::Begin("Hierarchy", showSceneHierarchy, toolbar_window_flags);

	for(auto& scenePair : m_editor->sceneManager->GetActiveScenes()){

		SceneContext* context = scenePair.second->GetSceneContext();

		EntityRegistry* registry = context->entity;

		// PrefabInstantiateCommand の Undo 後に選択状態をリセットする共通コールバック
		auto onPrefabUndone = [this]() {
			if(sceneContext && !sceneContext->entity->IsAlive(selectedEntity))
				selectedEntity = 0;
			if(sceneContext && !sceneContext->entity->IsAlive(pendingRenameEntity))
				pendingRenameEntity = 0;
		};

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
								},
								onPrefabUndone);
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
											},
											onPrefabUndone);
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
						ofn.lpstrInitialDir = "Asset\\Prefab";
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
								},
								onPrefabUndone);
							m_editor->commandManager.Execute(std::move(cmd));
						}
					}
				}
				ImGui::EndPopup();
			}
			ImGui::SameLine();

			ImGui::SetNextItemWidth(-1);
			ImGui::InputTextWithHint("##search", "Search objects...", searchBuffer, sizeof(searchBuffer));

			ImGui::SetCursorPos(ImVec2(10, ImGui::GetCursorPos().y));
			ImGui::Separator();

			// GetAllAlive() の参照ではなくコピーを取得する。
			// DrawHierarchyNode 内でエンティティを削除すると EntityRegistry::Destroy() が
			// m_alive を書き換えるため、参照のままだとイテレータが無効化されクラッシュする。
			const auto entities = registry->GetAllAlive();

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

	if(ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)){
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
				auto cmd = std::make_unique<EmptyParentCommand>(
					context, entity,
					[this, context](Entity e, SceneContext*){
						selectedEntity = e;
						sceneContext   = context;
					},
					[this](){
						if(sceneContext && !sceneContext->entity->IsAlive(selectedEntity))
							selectedEntity = 0;
						if(sceneContext && !sceneContext->entity->IsAlive(pendingRenameEntity))
							pendingRenameEntity = 0;
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
			auto cmd = std::make_unique<EntityDuplicateCommand>(
				context, entity,
				[this, context](Entity e, SceneContext*){
					selectedEntity = e;
					sceneContext   = context;
				},
				[this](){
					if(sceneContext && !sceneContext->entity->IsAlive(selectedEntity))
						selectedEntity = 0;
					if(sceneContext && !sceneContext->entity->IsAlive(pendingRenameEntity))
						pendingRenameEntity = 0;
				});
			m_editor->commandManager.Execute(std::move(cmd));
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
				[this](){
					// 削除後、選択中エンティティが生存していなければ選択を解除する
					// （削除対象の親だけでなく子エンティティが選択されていた場合も対応）
					if(this->sceneContext && !this->sceneContext->entity->IsAlive(this->selectedEntity))
						this->selectedEntity = 0;
					if(this->sceneContext && !this->sceneContext->entity->IsAlive(this->pendingRenameEntity))
						this->pendingRenameEntity = 0;
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
				std::string oldName = name->name;
				auto cmd = std::make_unique<RenameCommand>(context, entity, oldName, renameBuffer);
				m_editor->commandManager.Execute(std::move(cmd));
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
}
