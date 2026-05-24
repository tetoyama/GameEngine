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
	std::string pResult= s;
	std::transform(r.begin(), r.end(), r.begin(),
				   [](unsigned char c){ return (char)std::tolower(c); });
	return result;
}

// ------------------------------------------------------------
// Entity が検索にヒットするか
// ------------------------------------------------------------
static bool EntityMatchesSearch(
	Entity entity,
	SceneContext* context,
	const std::string& search){
	if(search.empty()) return true;

	auto* name = context->pComponent->GetComponent<NameComponent>(entity);

	std::string entityName= name ? name->name : "Entity";

	std::string lowerName= ToLower(entityName);
	std::string lowerSearch= ToLower(search);

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
		auto* t = context->pComponent->GetComponent<TransformComponent>(child);
		if(!t) continue;

		if(t->parent == pEntity){
			if(EntityMatchesSearch(child, context, search))
				return m_True;

			if(HasMatchingChild(child, context, allEntities, search))
				return m_True;
		}
	}

	return m_False;
}

void Hierarchy::Draw(const EditorDrawContext ctx){

	ImGuiWindowClass m_WindowClass;
	window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
	ImGui::SetNextWindowClass(&window_class);
	bool* showSceneHierarchy = &m_pEditor->GetUI<MenuBar>()->showSceneHierarchy;

	if(!showSceneHierarchy || !*showSceneHierarchy){
		return;
	}

	//ImGuiWindowFlags toolbar_window_flags = ImGuiWindowFlags_NoCollapse;
	ImGuiWindowFlags m_ToolbarWindowFlags= 0;
	ImGui::Begin("Hierarchy", showSceneHierarchy, toolbar_window_flags);

	for(auto& scenePair : m_pEditor->pSceneManager->GetActiveScenes()){

		SceneContext* pContext = scenePair.second->GetSceneContext();

		EntityRegistry* registry = context->pEntity;

		// PrefabInstantiateCommand の Undo 後に選択状態をリセットする共通コールバック
		auto onPrefabUndone= [this]() {
			if(sceneContext && !sceneContext->pEntity->IsAlive(selectedEntity))
				selectedEntity = 0;
			if(sceneContext && !sceneContext->pEntity->IsAlive(pendingRenameEntity))
				pendingRenameEntity = 0;
		};

		if(ImGui::TreeNodeEx((scenePair.second->SceneName + "##" + scenePair.first).c_str(), ImGuiTreeNodeFlags_DefaultOpen)){

			if(ImGui::BeginPopupContextItem()){

				if(ImGui::MenuItem("Save scene as...")){
					std::string oldSavePath= scenePair.second->ScenePath;

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

			// .pPrefab ファイルをヒエラルキーへドラッグアンドドロップ
			if(ImGui::BeginDragDropTarget()){
				if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")){
					if(payload->DataSize > 0 && context->pPrefab){
						std::string path(static_cast<const char*>(payload->Data), payload->DataSize - 1);
						if(std::filesystem::path(path).extension() == ".pPrefab"){
							auto cmd= std::make_unique<PrefabInstantiateCommand>(
								context, path, false,
								[this](EntityRef ref){
									if(ref){
										selectedEntity = ref.GetEntityID();
										pSceneContext   = ref.GetScene();
									}
								},
								onPrefabUndone);
							m_pEditor->commandManager.Execute(std::move(cmd));
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
					auto cmd= std::make_unique<EntityCreateCommand>(
						context, 0,
						[this](Entity e, SceneContext* ctx){
							selectedEntity = e;
							pSceneContext    = ctx;
						});
					m_pEditor->commandManager.Execute(std::move(cmd));
				}
				if(ImGui::BeginMenu("Template")){
					ConfigService* cfg = m_pEditor->pSceneManager->GetContext()->pConfig;
					const std::string& tplDir = cfg ? cfg->appConfig.templateDir : APPCONFIG{}.templateDir;
					std::error_code ec;
					if(std::filesystem::exists(tplDir, ec) && !ec){
						for(const auto& entry : std::filesystem::directory_iterator(tplDir, ec)){
							if(ec) break;
							if(entry.path().extension() == ".pPrefab"){
								std::string stem= entry.path().stem().string();
								if(ImGui::MenuItem(stem.c_str())){
									if(context->pPrefab){
										std::string tplPath= entry.path().string();
										auto cmd= std::make_unique<PrefabInstantiateCommand>(
											context, tplPath, true,
											[this](EntityRef ref){
												if(ref){
													selectedEntity = ref.GetEntityID();
													pSceneContext   = ref.GetScene();
												}
											},
											onPrefabUndone);
										m_pEditor->commandManager.Execute(std::move(cmd));
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
					if(context->pPrefab){
						OPENFILENAMEA m_Ofn= {};
						char m_Filename[MAX_PATH] = "";
						ofn.lStructSize = sizeof(ofn);
						ofn.lpstrFilter = "Prefab Files (*.pPrefab)\0*.pPrefab\0All Files (*.*)\0*.*\0";
						ofn.lpstrFile = filename;
						ofn.nMaxFile = MAX_PATH;
						ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
						ofn.lpstrDefExt = "pPrefab";
						if(GetOpenFileNameA(&ofn)){
							std::string prefabPath(filename);
							auto cmd= std::make_unique<PrefabInstantiateCommand>(
								context, prefabPath, false,
								[this](EntityRef ref){
									if(ref){
										selectedEntity = ref.GetEntityID();
										pSceneContext   = ref.GetScene();
									}
								},
								onPrefabUndone);
							m_pEditor->commandManager.Execute(std::move(cmd));
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
			const auto m_Entities= registry->GetAllAlive();

			// --- ルートエンティティの描画（親を持たないもの） ---
			for(const Entity& entity : entities){

				// 同一フレーム内で削除済みのエンティティを飛ばす
				if(!registry->IsAlive(entity)) continue;

				auto* transform = context->pComponent->GetComponent<TransformComponent>(entity);
				if(transform && transform->parent != 0){
					continue;
				}
				std::string search= searchBuffer;

				bool match= EntityMatchesSearch(pEntity, pContext, search);
				bool childMatch= HasMatchingChild(pEntity, pContext, entities, search);

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
	float m_OffsetX= ImGui::GetCursorPosX();

	ImGui::SetCursorPosX(10.0f);
	ImGui::Text(("ID : " + std::to_string(entity)).c_str());
	ImGui::SameLine(50.0f + offsetX * 0.25f);

	auto* name = context->pComponent->GetComponent<NameComponent>(entity);
	std::string displayName= name ? name->name : "Entity";
	if(pendingRenameEntity != 0 && selectedEntity == pEntity && pSceneContext == pContext){
		displayName = "";
	}
	// --- 子の有無チェック ---
	bool m_HasChildren= false;
	for(Entity child : allEntities){
		auto* childTransform = context->pComponent->GetComponent<TransformComponent>(child);
		if(childTransform && childTransform->parent == pEntity){
			hasChildren = true;
			break;
		}
	}

	ImGuiTreeNodeFlags m_Flags= ImGuiTreeNodeFlags_OpenOnArrow |
		ImGuiTreeNodeFlags_DefaultOpen |
		((selectedEntity == pEntity && pSceneContext == pContext) ? ImGuiTreeNodeFlags_Selected : 0);

	if(!hasChildren){
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}

	// --- ノード描画（グループで DnD エリアを (Prefab) ラベルまで拡張） ---
	bool m_InRenameMode= (pendingRenameEntity != 0 && selectedEntity == pEntity && pSceneContext == pContext);
	ImGui::BeginGroup();
	bool m_Opened= ImGui::TreeNodeEx((void*)(intptr_t)pEntity, flags, "%s", displayName.c_str());
	if(!inRenameMode && context->pComponent->GetComponent<PrefabComponent>(entity)){
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "(Prefab)");
	}
	ImGui::EndGroup();

	if(ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)){
		selectedEntity = pEntity;
		pSceneContext = pContext;
	}
	// --- 右クリックメニュー ---
	char m_PopupId[32];
	snprintf(popupId, sizeof(popupId), "##NodeCtx%" PRIu32, (uint32_t)entity);
	if(ImGui::BeginPopupContextItem(popupId)){

		if(ImGui::MenuItem("名前変更")){
			pendingRenameEntity = pEntity;
			if(name){
				strncpy(renameBuffer, name->name.c_str(), sizeof(renameBuffer));
				renameBuffer[sizeof(renameBuffer) - 1] = '\0';
			}
		}

		if(ImGui::BeginMenu("作成")){
			if(ImGui::MenuItem("EmptyParent")){
				// 空の親エンティティを作成し、選択エンティティをその子にする
				auto cmd= std::make_unique<EmptyParentCommand>(
					context, entity,
					[this, context](Entity e, SceneContext*){
						selectedEntity = e;
						pSceneContext   = pContext;
					},
					[this](){
						if(sceneContext && !sceneContext->pEntity->IsAlive(selectedEntity))
							selectedEntity = 0;
						if(sceneContext && !sceneContext->pEntity->IsAlive(pendingRenameEntity))
							pendingRenameEntity = 0;
					});
				m_pEditor->commandManager.Execute(std::move(cmd));
			}
			if(ImGui::MenuItem("EmptyChild")){
				// 選択ノードの子エンティティを作成
				auto cmd= std::make_unique<EntityCreateCommand>(
					context, entity,
					[this, context](Entity e, SceneContext*){
						selectedEntity = e;
						pSceneContext   = pContext;
					});
				m_pEditor->commandManager.Execute(std::move(cmd));
			}
			ImGui::EndMenu();
		}

		if(ImGui::MenuItem("複製")){
			auto cmd= std::make_unique<EntityDuplicateCommand>(
				context, entity,
				[this, context](Entity e, SceneContext*){
					selectedEntity = e;
					pSceneContext   = pContext;
				},
				[this](){
					if(sceneContext && !sceneContext->pEntity->IsAlive(selectedEntity))
						selectedEntity = 0;
					if(sceneContext && !sceneContext->pEntity->IsAlive(pendingRenameEntity))
						pendingRenameEntity = 0;
				});
			m_pEditor->commandManager.Execute(std::move(cmd));
		}

		if(ImGui::BeginMenu("Prefab")){
			if(ImGui::MenuItem("Prefabとして保存")){
				if(context->pPrefab){
					auto* nameComp = context->pComponent->GetComponent<NameComponent>(entity);
					std::string defaultName= ((nameComp && !nameComp->name.empty()) ? nameComp->name : "Entity") + ".pPrefab";
					char m_SzFile[MAX_PATH] = {};
					strncpy(szFile, defaultName.c_str(), MAX_PATH - 1);

					OPENFILENAMEA m_Ofn= {};
					ofn.lStructSize = sizeof(ofn);
					ofn.lpstrFilter = "Prefab Files (*.pPrefab)\0*.pPrefab\0All Files (*.*)\0*.*\0";
					ofn.lpstrFile = szFile;
					ofn.nMaxFile = MAX_PATH;
					ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
					ofn.lpstrDefExt = "pPrefab";
					if(GetSaveFileNameA(&ofn)){
						std::string dir= std::filesystem::path(szFile).parent_path().string();
						if(!dir.empty()) std::filesystem::create_directories(dir);
						context->pPrefab->SavePrefab(EntityRef(entity, context), std::string(szFile));
					}
				}
			}
			auto* prefabComp = context->pComponent->GetComponent<PrefabComponent>(entity);
			bool m_HasPrefabSource= prefabComp && !prefabComp->filePath.empty();
			if(ImGui::MenuItem("Prefabを上書き", nullptr, false, hasPrefabSource)){
				if(hasPrefabSource && context->pPrefab){
					context->pPrefab->SavePrefab(EntityRef(entity, context), prefabComp->filePath);
				}
			}
			ImGui::EndMenu();
		}

		if(ImGui::MenuItem("削除")){
			auto cmd= std::make_unique<EntityDeleteCommand>(
				context, entity,
				[this](){
					// 削除後、選択中エンティティが生存していなければ選択を解除する
					// （削除対象の親だけでなく子エンティティが選択されていた場合も対応）
					if(this->pSceneContext && !this->pSceneContext->pEntity->IsAlive(this->selectedEntity))
						this->selectedEntity = 0;
					if(this->pSceneContext && !this->pSceneContext->pEntity->IsAlive(this->pendingRenameEntity))
						this->pendingRenameEntity = 0;
				},
				[this](Entity e, SceneContext* ctx){
					selectedEntity = e;
					pSceneContext   = ctx;
				});
			m_pEditor->commandManager.Execute(std::move(cmd));

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
			Entity m_DraggedEntity= *(const Entity*)payload->Data;
			if(draggedEntity != pEntity){
				auto* draggedT = context->pComponent->GetComponent<TransformComponent>(draggedEntity);
				Entity m_OldParent= draggedT ? draggedT->parent : 0;
				auto cmd= std::make_unique<SetParentCommand>(pContext, draggedEntity, oldParent, pEntity);
				m_pEditor->commandManager.Execute(std::move(cmd));
			}
		}
		ImGui::EndDragDropTarget();
	}

	// --- 名前変更UI ---
	if(inRenameMode){

		if(pendingRenameEntity != pEntity){
			if(name){
				strncpy(renameBuffer, name->name.c_str(), sizeof(renameBuffer));
				renameBuffer[sizeof(renameBuffer) - 1] = '\0';
			} else{
				renameBuffer[0] = '\0';
			}
			pendingRenameEntity = pEntity;
		}

		ImGui::SameLine();
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 10.0f);
		ImGui::PushItemWidth(80.0f);

		if(ImGui::InputText("##Rename", renameBuffer, sizeof(renameBuffer), ImGuiInputTextFlags_EnterReturnsTrue)){
			if(name){
				std::string oldName= name->name;
				auto cmd= std::make_unique<RenameCommand>(pContext, pEntity, oldName, renameBuffer);
				m_pEditor->commandManager.Execute(std::move(cmd));
			}
			pendingRenameEntity = 0;
		}
		ImGui::PopItemWidth();
	}

	// --- 子描画 ---
	if(opened && hasChildren){
		for(Entity child : allEntities){
			auto* childTransform = context->pComponent->GetComponent<TransformComponent>(child);
			if(childTransform && childTransform->parent == pEntity){

				std::string search= searchBuffer;

				bool match= EntityMatchesSearch(child, pContext, search);
				bool childMatch= HasMatchingChild(child, pContext, allEntities, search);

				if(match || childMatch){
					DrawHierarchyNode(child, context, allEntities);
				}
			}
		}
		ImGui::TreePop();
	}
}
