// =======================================================================
// 
// Hierarchy.cpp
// 
// =======================================================================
#include "Hierarchy.h"

#include <ImGui/imgui_internal.h>

#include <algorithm>
#include <cinttypes>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <typeindex>
#include <vector>

#include <commdlg.h>
#include <sceneManager.h>

#include "Editor/editorService.h"
#include "Editor/UI/MenuBar.h"
#include "Editor/UI/ModernImGui/ModernImGui.h"
#include "Editor/UI/ModernImGui/EditorIconWidgets.h"
#include "Editor/UI/ModernImGui/HierarchyDecoratorWidgets.h"
#include "Editor/Command/ComponentCommand.h"
#include "Editor/Command/EntityCommand.h"
#include "Editor/Command/PrefabCommand.h"
#include <scene.h>
#include <Component/transformComponent.h>
#include <Component/entityNameComponent.h>
#include <Component/PrefabComponent.h>
#include "Scene/Component/EntityStateComponents.h"
#include "Prefab/PrefabSystem.h"
#include "Service/Config/configSystem.h"

namespace {

std::string ToLower(const std::string& source){
	std::string result = source;
	std::transform(
		result.begin(),
		result.end(),
		result.begin(),
		[](unsigned char value){
			return static_cast<char>(std::tolower(value));
		}
	);
	return result;
}

bool EntityMatchesSearch(
	Entity entity,
	SceneContext* context,
	const std::string& lowerSearch
){
	if(lowerSearch.empty()) return true;
	const auto* name =
		context->component->GetComponent<NameComponent>(entity);
	return ToLower(name ? name->name : "Entity").find(lowerSearch) !=
		std::string::npos;
}

using HierarchyChildMap = std::unordered_map<Entity, std::vector<Entity>>;

bool HasMatchingChild(
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

void SetHierarchyEntityActive(
	EditorService* editor,
	SceneContext* context,
	Entity entity,
	bool active
){
	if(!editor || !context || !context->component || !context->entity ||
	   !context->entity->IsAlive(entity)){
		return;
	}

	ComponentRegistry* registry = context->component;
	const bool isDisabled =
		registry->HasComponent<DisabledComponent>(entity);
	if(active == !isDisabled) return;

	if(!active){
		auto command = std::make_unique<ComponentAddCommand>(
			context,
			entity,
			"DisabledComponent",
			[registry](Entity target){
				registry->AddComponent<DisabledComponent>(target);
			}
		);
		editor->commandManager.Execute(std::move(command));
		return;
	}

	const ComponentTypeID typeID =
		registry->GetComponentIDByName("DisabledComponent");
	if(typeID == INVALID_COMPONENT_TYPE_ID) return;

	const ComponentView component =
		registry->GetComponentByID(entity, typeID);
	if(!component) return;

	auto command = std::make_unique<ComponentRemoveCommand>(
		context,
		entity,
		component
	);
	editor->commandManager.Execute(std::move(command));
}

std::vector<MImGui::HierarchyAccessoryGlyph> BuildHierarchyGlyphs(
	EditorService* editor,
	ComponentRegistry* registry,
	Entity entity
){
	std::vector<MImGui::HierarchyAccessoryGlyph> glyphs;
	if(!editor || !registry) return glyphs;

	bool hasCollider = false;
	std::vector<std::string> otherComponents;
	const std::vector<ComponentView> components =
		registry->GetInspectorComponentViewsOfEntitySorted(entity);

	for(ComponentView component : components){
		const std::string componentName =
			registry->GetComponentName(component);
		if(componentName.empty() ||
		   componentName == "NameComponent" ||
		   componentName == "TransformComponent" ||
		   componentName == "PrefabComponent" ||
		   componentName == "DisabledComponent"){
			continue;
		}

		if(componentName == "ColliderComponent"){
			hasCollider = true;
			continue;
		}
		otherComponents.push_back(componentName);
	}

	if(hasCollider){
		glyphs.push_back({
			editor->icons.Get(EditorIcon::Collider),
			"ColliderComponent"
		});
	}

	if(!otherComponents.empty()){
		std::string tooltip = otherComponents.size() == 1
			? otherComponents.front()
			: std::to_string(otherComponents.size()) + " additional components";
		if(otherComponents.size() > 1){
			for(const std::string& componentName : otherComponents){
				tooltip += "\n";
				tooltip += componentName;
			}
		}
		glyphs.push_back({
			editor->icons.Get(EditorIcon::Component),
			std::move(tooltip)
		});
	}

	return glyphs;
}

} // namespace

void Hierarchy::Draw(const EditorDrawContext ctx){
	(void)ctx;

	ImGuiWindowClass windowClass;
	windowClass.DockNodeFlagsOverrideSet =
		ImGuiDockNodeFlags_NoWindowMenuButton;
	ImGui::SetNextWindowClass(&windowClass);

	bool* showSceneHierarchy =
		&m_editor->GetUI<MenuBar>()->showSceneHierarchy;
	if(!showSceneHierarchy || !*showSceneHierarchy) return;

	if(!ImGui::Begin("Hierarchy", showSceneHierarchy)){
		ImGui::End();
		return;
	}

	const MImGui::Theme& modernTheme = MImGui::GetTheme();

	for(auto& scenePair : m_editor->sceneManager->GetActiveScenes()){
		SceneContext* context = scenePair.second->GetSceneContext();
		EntityRegistry* entityRegistry = context->entity;
		ComponentRegistry* componentRegistry = context->component;
		const auto entities = entityRegistry->GetAllAlive();

		ImGui::PushID(scenePair.first.c_str());
		ImGuiStorage* sceneStorage = ImGui::GetStateStorage();
		const ImGuiID sceneOpenID = ImGui::GetID("HierarchySceneOpenState");
		bool sceneOpen = sceneStorage->GetBool(sceneOpenID, true);

		const MImGui::HierarchySceneHeaderResult sceneHeader =
			MImGui::HierarchySceneHeader(
				"##SceneHeader",
				scenePair.second->SceneName.c_str(),
				m_editor->icons.Get(EditorIcon::Hierarchy),
				static_cast<int>(entities.size()),
				sceneOpen
			);
		sceneOpen = sceneHeader.open;
		sceneStorage->SetBool(sceneOpenID, sceneOpen);

		if(ImGui::BeginPopupContextItem("##SceneContext")){
			if(ImGui::MenuItem("Save scene as...")){
				const std::string oldSavePath = scenePair.second->ScenePath;
				scenePair.second->ScenePath.clear();
				scenePair.second->Save();
				scenePair.second->ScenePath = oldSavePath;
			}
			if(ImGui::MenuItem("Delete Scene")){
				scenePair.second->isDestroy = true;
				selectedEntity = 0;
			}
			ImGui::EndPopup();
		}

		auto onPrefabUndone = [this]() {
			if(sceneContext && !sceneContext->entity->IsAlive(selectedEntity)){
				selectedEntity = 0;
			}
			if(sceneContext && !sceneContext->entity->IsAlive(pendingRenameEntity)){
				pendingRenameEntity = 0;
			}
		};

		if(ImGui::BeginDragDropTarget()){
			if(const ImGuiPayload* payload =
				ImGui::AcceptDragDropPayload("ASSET_PATH")){
				if(payload->DataSize > 0 && context->prefab){
					std::string path(
						static_cast<const char*>(payload->Data),
						payload->DataSize - 1
					);
					if(std::filesystem::path(path).extension() == ".prefab"){
						auto command = std::make_unique<PrefabInstantiateCommand>(
							context,
							path,
							false,
							[this](EntityRef reference){
								if(reference){
									selectedEntity = reference.GetEntityID();
									sceneContext = reference.GetScene();
								}
							},
							onPrefabUndone
						);
						m_editor->commandManager.Execute(std::move(command));
					}
				}
			}
			ImGui::EndDragDropTarget();
		}

		if(sceneOpen){
			ImGui::Dummy(ImVec2(0.0f, 4.0f));

			const float addWidth = 74.0f;
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
					auto command = std::make_unique<EntityCreateCommand>(
						context,
						0,
						[this](Entity entity, SceneContext* createdContext){
							selectedEntity = entity;
							sceneContext = createdContext;
						}
					);
					m_editor->commandManager.Execute(std::move(command));
				}

				if(ImGui::BeginMenu("Template")){
					ConfigService* config =
						m_editor->sceneManager->GetContext()->config;
					const std::string& templateDirectory = config
						? config->appConfig.templateDir
						: APPCONFIG{}.templateDir;
					std::error_code error;
					if(std::filesystem::exists(templateDirectory, error) &&
					   !error){
						for(const auto& entry :
							std::filesystem::directory_iterator(
								templateDirectory,
								error
							)){
							if(error) break;
							if(entry.path().extension() != ".prefab") continue;

							const std::string stem = entry.path().stem().string();
							if(ImGui::MenuItem(stem.c_str()) && context->prefab){
								const std::string templatePath = entry.path().string();
								auto command =
									std::make_unique<PrefabInstantiateCommand>(
										context,
										templatePath,
										true,
										[this](EntityRef reference){
											if(reference){
												selectedEntity = reference.GetEntityID();
												sceneContext = reference.GetScene();
											}
										},
										onPrefabUndone
									);
								m_editor->commandManager.Execute(std::move(command));
							}
						}
					} else{
						ImGui::TextDisabled("(no templates found)");
					}
					ImGui::EndMenu();
				}

				if(ImGui::MenuItem("Prefab") && context->prefab){
					OPENFILENAMEA openFile = {};
					char filename[MAX_PATH] = "";
					openFile.lStructSize = sizeof(openFile);
					openFile.lpstrFilter =
						"Prefab Files (*.prefab)\0*.prefab\0All Files (*.*)\0*.*\0";
					openFile.lpstrFile = filename;
					openFile.nMaxFile = MAX_PATH;
					openFile.lpstrInitialDir = "Asset\\Prefab";
					openFile.Flags =
						OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
					openFile.lpstrDefExt = "prefab";
					if(GetOpenFileNameA(&openFile)){
						const std::string prefabPath(filename);
						auto command = std::make_unique<PrefabInstantiateCommand>(
							context,
							prefabPath,
							false,
							[this](EntityRef reference){
								if(reference){
									selectedEntity = reference.GetEntityID();
									sceneContext = reference.GetScene();
								}
							},
							onPrefabUndone
						);
						m_editor->commandManager.Execute(std::move(command));
					}
				}
				ImGui::EndPopup();
			}

			ImGui::SameLine();
			const float optionsWidth = 30.0f;
			const float remainingWidth = ImGui::GetContentRegionAvail().x;
			const bool showOptionsButton = remainingWidth >= 112.0f;
			const float searchWidth = showOptionsButton
				? (std::max)(
					64.0f,
					remainingWidth - optionsWidth -
						ImGui::GetStyle().ItemSpacing.x
				)
				: -1.0f;
			MImGui::SearchField(
				"##search",
				"Search objects...",
				searchBuffer,
				sizeof(searchBuffer),
				searchWidth
			);

			if(showOptionsButton){
				ImGui::SameLine();
				if(MImGui::Button(
					"...##HierarchyAppearance",
					ImVec2(optionsWidth, modernTheme.compactHeight),
					MImGui::ButtonKind::Ghost
				)){
					ImGui::OpenPopup("HierarchyAppearancePopup");
				}
				if(ImGui::IsItemHovered()){
					ImGui::SetTooltip("Hierarchy appearance");
				}
			}

			if(ImGui::BeginPopup("HierarchyAppearancePopup")){
				ImGui::TextUnformatted("Hierarchy Appearance");
				ImGui::TextDisabled("Visual aids can be toggled independently.");
				ImGui::Separator();
				MImGui::Toggle("Breadcrumbs", &m_showBreadcrumbs);
				MImGui::Toggle("Component Info", &m_showComponentGlyphs);
				MImGui::Toggle("Active State", &m_showActiveState);
				MImGui::Toggle("Alternating Rows", &m_alternateRows);
				ImGui::EndPopup();
			}

			ImGui::Dummy(ImVec2(0.0f, 5.0f));

			HierarchyChildMap children;
			std::vector<Entity> roots;
			children.reserve(entities.size());
			roots.reserve(entities.size());

			for(const Entity& entity : entities){
				if(!entityRegistry->IsAlive(entity)) continue;
				const auto* transform =
					componentRegistry->GetComponent<TransformComponent>(entity);
				if(transform && transform->parent != 0){
					children[transform->parent].push_back(entity);
				} else{
					roots.push_back(entity);
				}
			}

			const std::string lowerSearch = ToLower(searchBuffer);
			std::vector<Entity> visibleRoots;
			visibleRoots.reserve(roots.size());
			for(Entity entity : roots){
				const bool match =
					EntityMatchesSearch(entity, context, lowerSearch);
				const bool childMatch = !lowerSearch.empty() &&
					HasMatchingChild(entity, context, children, lowerSearch);
				if(match || childMatch) visibleRoots.push_back(entity);
			}

			m_visibleRowIndex = 0;
			const std::vector<bool> noAncestorContinuations;
			for(std::size_t index = 0; index < visibleRoots.size(); ++index){
				DrawHierarchyNode(
					visibleRoots[index],
					context,
					children,
					lowerSearch,
					0,
					index + 1 == visibleRoots.size(),
					noAncestorContinuations
				);
			}
		}

		ImGui::PopID();
		ImGui::Dummy(ImVec2(0.0f, 5.0f));
	}

	ImGui::End();
}

void Hierarchy::DrawHierarchyNode(
	Entity entity,
	SceneContext* context,
	const ChildMap& children,
	const std::string& lowerSearch,
	int depth,
	bool isLastChild,
	const std::vector<bool>& ancestorContinuations
){
	ComponentRegistry* registry = context->component;
	auto* name = registry->GetComponent<NameComponent>(entity);
	const std::string displayName = name ? name->name : "Entity";
	const auto childIterator = children.find(entity);
	const bool hasChildren =
		childIterator != children.end() && !childIterator->second.empty();
	const bool selected =
		selectedEntity == entity && sceneContext == context;
	const bool inRenameMode = pendingRenameEntity != 0 && selected;
	const bool isPrefab =
		registry->GetComponent<PrefabComponent>(entity) != nullptr;
	const bool active =
		!registry->HasComponent<DisabledComponent>(entity);

	ImGui::PushID(context);
	ImGui::PushID((void*)(intptr_t)entity);

	ImGuiStorage* storage = ImGui::GetStateStorage();
	const ImGuiID openStateID = ImGui::GetID("HierarchyOpenState");
	bool opened = storage->GetBool(openStateID, true);
	if(!lowerSearch.empty() &&
	   HasMatchingChild(entity, context, children, lowerSearch)){
		opened = true;
	}

	const std::vector<MImGui::HierarchyAccessoryGlyph> glyphs =
		m_showComponentGlyphs
			? BuildHierarchyGlyphs(m_editor, registry, entity)
			: std::vector<MImGui::HierarchyAccessoryGlyph>{};
	const bool alternate = m_alternateRows &&
		((m_visibleRowIndex & 1u) != 0u);
	++m_visibleRowIndex;

	const MImGui::DecoratedHierarchyRowResult row =
		MImGui::DecoratedHierarchyRow(
			"##EntityRow",
			inRenameMode ? "" : displayName.c_str(),
			selected,
			hasChildren,
			opened,
			active,
			m_showActiveState,
			isPrefab,
			depth,
			isLastChild,
			ancestorContinuations,
			glyphs,
			m_showBreadcrumbs,
			alternate
		);
	opened = row.open;
	storage->SetBool(openStateID, opened);

	const ImVec2 rowMin = ImGui::GetItemRectMin();
	const ImVec2 rowMax = ImGui::GetItemRectMax();
	const ImVec2 cursorAfterRow = ImGui::GetCursorPos();
	const bool rowHovered =
		ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort);

	if(row.activeToggled){
		SetHierarchyEntityActive(m_editor, context, entity, !active);
	}
	if(row.activated){
		selectedEntity = entity;
		sceneContext = context;
	}

	if(!row.accessoryHovered && ImGui::BeginDragDropSource()){
		ImGui::SetDragDropPayload(
			"ENTITY_DRAG_DROP",
			&entity,
			sizeof(Entity)
		);
		ImGui::Text("Move %s", displayName.c_str());
		ImGui::EndDragDropSource();
	}
	if(ImGui::BeginDragDropTarget()){
		if(const ImGuiPayload* payload =
			ImGui::AcceptDragDropPayload("ENTITY_DRAG_DROP")){
			IM_ASSERT(payload->DataSize == sizeof(Entity));
			const Entity draggedEntity =
				*static_cast<const Entity*>(payload->Data);
			if(draggedEntity != entity){
				auto* draggedTransform =
					registry->GetComponent<TransformComponent>(draggedEntity);
				const Entity oldParent = draggedTransform
					? draggedTransform->parent
					: Entity(0, 0);
				auto command = std::make_unique<SetParentCommand>(
					context,
					draggedEntity,
					oldParent,
					entity
				);
				m_editor->commandManager.Execute(std::move(command));
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
				std::strncpy(renameBuffer, name->name.c_str(), sizeof(renameBuffer));
				renameBuffer[sizeof(renameBuffer) - 1] = '\0';
			}
		}

		if(ImGui::BeginMenu("作成")){
			if(ImGui::MenuItem("EmptyParent")){
				auto command = std::make_unique<EmptyParentCommand>(
					context,
					entity,
					[this, context](Entity createdEntity, SceneContext*){
						selectedEntity = createdEntity;
						sceneContext = context;
					},
					[this](){
						if(sceneContext &&
						   !sceneContext->entity->IsAlive(selectedEntity)){
							selectedEntity = 0;
						}
						if(sceneContext &&
						   !sceneContext->entity->IsAlive(pendingRenameEntity)){
							pendingRenameEntity = 0;
						}
					}
				);
				m_editor->commandManager.Execute(std::move(command));
			}
			if(ImGui::MenuItem("EmptyChild")){
				auto command = std::make_unique<EntityCreateCommand>(
					context,
					entity,
					[this, context](Entity createdEntity, SceneContext*){
						selectedEntity = createdEntity;
						sceneContext = context;
					}
				);
				m_editor->commandManager.Execute(std::move(command));
			}
			ImGui::EndMenu();
		}

		if(ImGui::MenuItem("複製")){
			auto command = std::make_unique<EntityDuplicateCommand>(
				context,
				entity,
				[this, context](Entity duplicatedEntity, SceneContext*){
					selectedEntity = duplicatedEntity;
					sceneContext = context;
				},
				[this](){
					if(sceneContext &&
					   !sceneContext->entity->IsAlive(selectedEntity)){
						selectedEntity = 0;
					}
					if(sceneContext &&
					   !sceneContext->entity->IsAlive(pendingRenameEntity)){
						pendingRenameEntity = 0;
					}
				}
			);
			m_editor->commandManager.Execute(std::move(command));
		}

		if(ImGui::BeginMenu("Prefab")){
			if(ImGui::MenuItem("Prefabとして保存") && context->prefab){
				auto* nameComponent =
					registry->GetComponent<NameComponent>(entity);
				const std::string defaultName =
					((nameComponent && !nameComponent->name.empty())
						? nameComponent->name
						: "Entity") + ".prefab";
				char saveFile[MAX_PATH] = {};
				std::strncpy(saveFile, defaultName.c_str(), MAX_PATH - 1);

				OPENFILENAMEA saveDialog = {};
				saveDialog.lStructSize = sizeof(saveDialog);
				saveDialog.lpstrFilter =
					"Prefab Files (*.prefab)\0*.prefab\0All Files (*.*)\0*.*\0";
				saveDialog.lpstrFile = saveFile;
				saveDialog.nMaxFile = MAX_PATH;
				saveDialog.Flags =
					OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
				saveDialog.lpstrDefExt = "prefab";
				if(GetSaveFileNameA(&saveDialog)){
					const std::string directory =
						std::filesystem::path(saveFile).parent_path().string();
					if(!directory.empty()){
						std::filesystem::create_directories(directory);
					}
					context->prefab->SavePrefab(
						EntityRef(entity, context),
						std::string(saveFile)
					);
				}
			}

			auto* prefabComponent =
				registry->GetComponent<PrefabComponent>(entity);
			const bool hasPrefabSource =
				prefabComponent && !prefabComponent->filePath.empty();
			if(ImGui::MenuItem(
				"Prefabを上書き",
				nullptr,
				false,
				hasPrefabSource
			)){
				if(hasPrefabSource && context->prefab){
					context->prefab->SavePrefab(
						EntityRef(entity, context),
						prefabComponent->filePath
					);
				}
			}
			ImGui::EndMenu();
		}

		if(ImGui::MenuItem("削除")){
			auto command = std::make_unique<EntityDeleteCommand>(
				context,
				entity,
				[this](){
					if(sceneContext &&
					   !sceneContext->entity->IsAlive(selectedEntity)){
						selectedEntity = 0;
					}
					if(sceneContext &&
					   !sceneContext->entity->IsAlive(pendingRenameEntity)){
						pendingRenameEntity = 0;
					}
				},
				[this](Entity restoredEntity, SceneContext* restoredContext){
					selectedEntity = restoredEntity;
					sceneContext = restoredContext;
				}
			);
			m_editor->commandManager.Execute(std::move(command));

			ImGui::EndPopup();
			ImGui::PopID();
			ImGui::PopID();
			return;
		}
		ImGui::EndPopup();
	}

	if(rowHovered && !inRenameMode && !row.accessoryHovered){
		ImGui::BeginTooltip();
		ImGui::TextUnformatted(displayName.c_str());
		ImGui::TextDisabled("Entity ID: %u", entity.GetIndex());
		if(isPrefab){
			if(const auto* prefab =
				registry->GetComponent<PrefabComponent>(entity)){
				if(!prefab->filePath.empty()){
					ImGui::TextDisabled("%s", prefab->filePath.c_str());
				}
			}
		}
		ImGui::EndTooltip();
	}

	if(inRenameMode){
		const float inputWidth = (std::max)(
			48.0f,
			row.textMaxX - row.textMinX
		);
		ImGui::SetCursorScreenPos(ImVec2(row.textMinX, rowMin.y + 1.0f));
		ImGui::SetNextItemWidth(inputWidth);
		if(!ImGui::IsAnyItemActive()){
			ImGui::SetKeyboardFocusHere();
		}

		if(ImGui::InputText(
			"##Rename",
			renameBuffer,
			sizeof(renameBuffer),
			ImGuiInputTextFlags_EnterReturnsTrue
		)){
			if(name){
				const std::string oldName = name->name;
				auto command = std::make_unique<RenameCommand>(
					context,
					entity,
					oldName,
					renameBuffer
				);
				m_editor->commandManager.Execute(std::move(command));
			}
			pendingRenameEntity = 0;
		}
		ImGui::SetCursorPos(cursorAfterRow);
	}

	if(opened && hasChildren){
		std::vector<Entity> visibleChildren;
		visibleChildren.reserve(childIterator->second.size());
		for(Entity child : childIterator->second){
			const bool match =
				EntityMatchesSearch(child, context, lowerSearch);
			const bool childMatch = !lowerSearch.empty() &&
				HasMatchingChild(child, context, children, lowerSearch);
			if(match || childMatch) visibleChildren.push_back(child);
		}

		std::vector<bool> childContinuations = ancestorContinuations;
		if(depth > 0){
			childContinuations.push_back(!isLastChild);
		}

		for(std::size_t index = 0; index < visibleChildren.size(); ++index){
			DrawHierarchyNode(
				visibleChildren[index],
				context,
				children,
				lowerSearch,
				depth + 1,
				index + 1 == visibleChildren.size(),
				childContinuations
			);
		}
	}

	ImGui::PopID();
	ImGui::PopID();
}
