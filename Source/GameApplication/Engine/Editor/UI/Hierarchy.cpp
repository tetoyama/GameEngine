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
#include "Editor/UI/ModernImGui/ModernImGui.h"
#include "Editor/UI/ModernImGui/EditorIconWidgets.h"
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
	const std::string& lowerSearch
){
	if(lowerSearch.empty()) return true;
	const auto* name = context->component->GetComponent<NameComponent>(entity);
	return ToLower(name ? name->name : "Entity").find(lowerSearch) != std::string::npos;
}

using HierarchyChildMap = std::unordered_map<Entity, std::vector<Entity>>;

static bool HasMatchingChild(
	Entity entity,
	SceneContext* context,
	const HierarchyChildMap& children,
	const std::string& lowerSearch
){
	const auto found = children.find(entity);
	if(found == children.end()) return false;
	for(Entity child : found->second){
		if(EntityMatchesSearch(child, context, lowerSearch) ||
		   HasMatchingChild(child, context, children, lowerSearch)){
			return true;
		}
	}
	return false;
}

void Hierarchy::Draw(const EditorDrawContext ctx){
	(void)ctx;

	ImGuiWindowClass window_class;
	window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
	ImGui::SetNextWindowClass(&window_class);
	bool* showSceneHierarchy = &m_editor->GetUI<MenuBar>()->showSceneHierarchy;

	if(!showSceneHierarchy || !*showSceneHierarchy){
		return;
	}

	ImGuiWindowFlags toolbar_window_flags = 0;
	if(!ImGui::Begin("Hierarchy", showSceneHierarchy, toolbar_window_flags)){
		ImGui::End();
		return;
	}

	const MImGui::Theme& modernTheme = MImGui::GetTheme();

	for(auto& scenePair : m_editor->sceneManager->GetActiveScenes()){

		SceneContext* context = scenePair.second->GetSceneContext();
		EntityRegistry* registry = context->entity;

		auto onPrefabUndone = [this]() {
			if(sceneContext && !sceneContext->entity->IsAlive(selectedEntity))
				selectedEntity = 0;
			if(sceneContext && !sceneContext->entity->IsAlive(pendingRenameEntity))
				pendingRenameEntity = 0;
		};

		if(ImGui::TreeNodeEx((scenePair.second->SceneName + "##" + scenePair.first).c_str(), ImGuiTreeNodeFlags_DefaultOpen)){
			ImGui::PushID(scenePair.first.c_str());

			if(ImGui::BeginPopupContextItem()){

				if(ImGui::MenuItem("Save scene as...")){
					std::string oldSavePath = scenePair.second->ScenePath;
					scenePair.second->ScenePath = "";
					scenePair.second->Save();
					scenePair.second->ScenePath = oldSavePath;
				}

				if(ImGui::MenuItem("Delete Scene")){
					scenePair.second->isDestroy = true;
					selectedEntity = 0;
				}

				ImGui::EndPopup();
			}

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

			const float addWidth = 78.0f;
			if(MImGui::IconButton(
				"AddEntity",
				"Add",
				m_editor->icons.Get(EditorIcon::Add),
				ImVec2(addWidth, modernTheme.compactHeight),
				MImGui::ButtonKind::Secondary,
				14.0f
			)){
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

			MImGui::SearchField(
				"##search",
				"Search objects...",
				searchBuffer,
				sizeof(searchBuffer),
				-1.0f
			);

			ImGui::Separator();

			const auto entities = registry->GetAllAlive();
			HierarchyChildMap children;
			std::vector<Entity> roots;
			children.reserve(entities.size());
			roots.reserve(entities.size());

			for(const Entity& entity : entities){
				if(!registry->IsAlive(entity)) continue;
				const auto* transform = context->component->GetComponent<TransformComponent>(entity);
				if(transform && transform->parent != 0){
					children[transform->parent].push_back(entity);
				} else{
					roots.push_back(entity);
				}
			}

			const std::string lowerSearch = ToLower(searchBuffer);
			for(const Entity& entity : roots){
				const bool match = EntityMatchesSearch(entity, context, lowerSearch);
				const bool childMatch = !lowerSearch.empty() &&
					HasMatchingChild(entity, context, children, lowerSearch);
				if(match || childMatch){
					DrawHierarchyNode(entity, context, children, lowerSearch);
				}
			}

			ImGui::PopID();
			ImGui::TreePop();
		}
	}
	ImGui::End();
}

void Hierarchy::DrawHierarchyNode(Entity entity, SceneContext* context, const ChildMap& children, const std::string& lowerSearch){
	auto* name = context->component->GetComponent<NameComponent>(entity);
	const std::string displayName = name ? name->name : "Entity";
	const auto childIt = children.find(entity);
	const bool hasChildren = childIt != children.end() && !childIt->second.empty();
	const bool selected = selectedEntity == entity && sceneContext == context;
	const bool inRenameMode = pendingRenameEntity != 0 && selected;
	const bool isPrefab = context->component->GetComponent<PrefabComponent>(entity) != nullptr;

	ImGui::PushID(context);
	ImGui::PushID((void*)(intptr_t)entity);

	ImGuiStorage* storage = ImGui::GetStateStorage();
	const ImGuiID openStateID = ImGui::GetID("HierarchyOpenState");
	bool opened = storage->GetBool(openStateID, true);
	if(!lowerSearch.empty() && HasMatchingChild(entity, context, children, lowerSearch)){
		opened = true;
	}

	// Repeated generic icons do not add information. Keep the row focused on
	// hierarchy, name, selection and exceptional state such as Prefab.
	const MImGui::TreeRowResult row = MImGui::TreeRow(
		"##EntityRow",
		inRenameMode ? "" : displayName.c_str(),
		selected,
		hasChildren,
		opened,
		isPrefab ? "Prefab" : nullptr
	);
	opened = row.open;
	storage->SetBool(openStateID, opened);

	const ImVec2 rowMin = ImGui::GetItemRectMin();
	const ImVec2 rowMax = ImGui::GetItemRectMax();
	const ImVec2 cursorAfterRow = ImGui::GetCursorPos();
	const bool rowHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort);

	if(row.activated){
		selectedEntity = entity;
		sceneContext = context;
	}

	if(ImGui::BeginDragDropSource()){
		ImGui::SetDragDropPayload("ENTITY_DRAG_DROP", &entity, sizeof(Entity));
		ImGui::Text("Move %s", displayName.c_str());
		ImGui::EndDragDropSource();
	}
	if(ImGui::BeginDragDropTarget()){
		if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_DRAG_DROP")){
			IM_ASSERT(payload->DataSize == sizeof(Entity));
			Entity draggedEntity = *(const Entity*)payload->Data;
			if(draggedEntity != entity){
				auto* draggedT = context->component->GetComponent<TransformComponent>(draggedEntity);
				Entity oldParent = draggedT ? draggedT->parent : Entity(0, 0);
				auto cmd = std::make_unique<SetParentCommand>(context, draggedEntity, oldParent, entity);
				m_editor->commandManager.Execute(std::move(cmd));
			}
		}
		ImGui::EndDragDropTarget();
	}

	if(ImGui::BeginPopupContextItem("##NodeContext")){

		if(ImGui::MenuItem("名前変更")){
			pendingRenameEntity = entity;
			selectedEntity = entity;
			sceneContext = context;
			if(name){
				strncpy(renameBuffer, name->name.c_str(), sizeof(renameBuffer));
				renameBuffer[sizeof(renameBuffer) - 1] = '\0';
			}
		}

		if(ImGui::BeginMenu("作成")){
			if(ImGui::MenuItem("EmptyParent")){
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
			ImGui::PopID();
			ImGui::PopID();
			return;
		}

		ImGui::EndPopup();
	}

	if(rowHovered && !inRenameMode){
		ImGui::BeginTooltip();
		ImGui::TextUnformatted(displayName.c_str());
		ImGui::TextDisabled("Entity ID: %u", entity.GetIndex());
		if(isPrefab){
			if(const auto* prefab = context->component->GetComponent<PrefabComponent>(entity)){
				if(!prefab->filePath.empty()){
					ImGui::TextDisabled("%s", prefab->filePath.c_str());
				}
			}
		}
		ImGui::EndTooltip();
	}

	if(inRenameMode){
		const float textX = rowMin.x + (hasChildren ? 27.0f : 9.0f);
		float rightReserve = 8.0f;
		if(isPrefab){
			rightReserve += ImGui::CalcTextSize("Prefab").x + 18.0f;
		}
		const float inputWidth = (std::max)(60.0f, rowMax.x - textX - rightReserve);

		ImGui::SetCursorScreenPos(ImVec2(textX, rowMin.y + 1.0f));
		ImGui::SetNextItemWidth(inputWidth);
		if(!ImGui::IsAnyItemActive()){
			ImGui::SetKeyboardFocusHere();
		}

		if(ImGui::InputText("##Rename", renameBuffer, sizeof(renameBuffer), ImGuiInputTextFlags_EnterReturnsTrue)){
			if(name){
				std::string oldName = name->name;
				auto cmd = std::make_unique<RenameCommand>(context, entity, oldName, renameBuffer);
				m_editor->commandManager.Execute(std::move(cmd));
			}
			pendingRenameEntity = 0;
		}
		ImGui::SetCursorPos(cursorAfterRow);
	}

	if(opened && hasChildren){
		ImGui::Indent(18.0f);
		for(Entity child : childIt->second){
			const bool match = EntityMatchesSearch(child, context, lowerSearch);
			const bool childMatch = !lowerSearch.empty() &&
				HasMatchingChild(child, context, children, lowerSearch);
			if(match || childMatch){
				DrawHierarchyNode(child, context, children, lowerSearch);
			}
		}
		ImGui::Unindent(18.0f);
	}

	ImGui::PopID();
	ImGui::PopID();
}
