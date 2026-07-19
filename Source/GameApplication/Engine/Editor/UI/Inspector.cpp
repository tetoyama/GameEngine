// =======================================================================
//
// Inspector.cpp
//
// =======================================================================
#include "Inspector.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <ImGui/imgui_internal.h>

#include "Editor/editorService.h"
#include "Editor/UI/MenuBar.h"
#include "Editor/UI/ModernImGui/ModernImGui.h"
#include "Editor/Command/EntityCommand.h"
#include "Editor/Command/ComponentCommand.h"
#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Scene/Component/entityNameComponent.h"
#include "Scene/Component/EntityStateComponents.h"
#include "Scene/Component/CullingComponent.h"
#include "Scene/Component/PrefabComponent.h"
#include "Hierarchy.h"

namespace {

template<typename T>
void SetEntityHeaderTag(
	EditorService* editor,
	SceneContext* context,
	Entity entity,
	const char* componentName,
	bool shouldExist
){
	if(!editor || !context || !context->component || !context->entity ||
		!context->entity->IsAlive(entity)){
		return;
	}

	ComponentRegistry* registry = context->component;
	const bool exists = registry->HasComponent<T>(entity);
	if(exists == shouldExist) return;

	if(shouldExist){
		auto command = std::make_unique<ComponentAddCommand>(
			context,
			entity,
			componentName,
			[registry](Entity target){
				registry->AddComponent<T>(target);
			}
		);
		editor->commandManager.Execute(std::move(command));
		return;
	}

	const ComponentTypeID typeID =
		registry->GetComponentIDByName(componentName);
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

} // namespace

void Inspector::Draw(const EditorDrawContext ctx){
	(void)ctx;

	ImGuiWindowClass windowClass;
	windowClass.DockNodeFlagsOverrideSet =
		ImGuiDockNodeFlags_NoWindowMenuButton;
	ImGui::SetNextWindowClass(&windowClass);

	bool* showInspector = &m_editor->GetUI<MenuBar>()->showInspector;
	Hierarchy* hierarchy = m_editor->GetUI<Hierarchy>();
	Entity selectedEntity = hierarchy->selectedEntity;
	SceneContext* context = hierarchy->sceneContext;

	if(!showInspector || !*showInspector) return;

	ImGui::Begin("Inspector", showInspector);

	if(!selectedEntity || !context){
		ImGui::TextUnformatted("No object selected.");
		ImGui::End();
		return;
	}

	if(!context->entity->IsAlive(selectedEntity)){
		hierarchy->selectedEntity = {};
		ImGui::End();
		return;
	}

	ComponentRegistry* registry = context->component;
	const MImGui::Theme& modernTheme = MImGui::GetTheme();

	ImGui::AlignTextToFramePadding();
	ImGui::TextDisabled("ID: %u", selectedEntity.GetIndex());
	ImGui::SameLine();

	if(NameComponent* name =
		registry->GetComponent<NameComponent>(selectedEntity)){
		char nameBuffer[256]{};
		std::strncpy(nameBuffer, name->name.c_str(), sizeof(nameBuffer) - 1);

		if(ImGui::InputText(
			"##Name",
			nameBuffer,
			sizeof(nameBuffer),
			ImGuiInputTextFlags_EnterReturnsTrue
		)){
			auto command = std::make_unique<RenameCommand>(
				context,
				selectedEntity,
				name->name,
				nameBuffer
			);
			m_editor->commandManager.Execute(std::move(command));
		}

		if(registry->GetComponent<PrefabComponent>(selectedEntity)){
			ImGui::SameLine();
			ImGui::TextColored(
				ImVec4(0.4f, 0.8f, 1.0f, 1.0f),
				"Prefab"
			);
		}
	}

	ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 72.0f);
	if(MImGui::DangerButton("Delete##EntityDelete", ImVec2(72.0f, modernTheme.compactHeight))){
		auto command = std::make_unique<EntityDeleteCommand>(
			context,
			selectedEntity,
			[hierarchy, selectedEntity](){
				if(hierarchy && hierarchy->selectedEntity == selectedEntity){
					hierarchy->selectedEntity = {};
				}
			},
			[hierarchy](Entity entity, SceneContext* restoredContext){
				if(hierarchy){
					hierarchy->selectedEntity = entity;
					hierarchy->sceneContext = restoredContext;
				}
			}
		);
		m_editor->commandManager.Execute(std::move(command));
	}

	ImGui::Separator();
	ImGui::PushID("EntityStateHeader");

	if(ImGui::BeginTable(
		"EntityStateGrid",
		2,
		ImGuiTableFlags_SizingStretchSame |
		ImGuiTableFlags_NoPadOuterX |
		ImGuiTableFlags_NoSavedSettings
	)){
		ImGui::TableNextColumn();
		bool enabled = !registry->HasComponent<DisabledComponent>(selectedEntity);
		if(MImGui::Toggle("Enable", &enabled)){
			SetEntityHeaderTag<DisabledComponent>(
				m_editor,
				context,
				selectedEntity,
				"DisabledComponent",
				!enabled
			);
		}

		ImGui::TableNextColumn();
		bool isStatic = registry->HasComponent<StaticEntityComponent>(selectedEntity);
		if(MImGui::Toggle("Static", &isStatic)){
			SetEntityHeaderTag<StaticEntityComponent>(
				m_editor,
				context,
				selectedEntity,
				"StaticEntityComponent",
				isStatic
			);
		}

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		bool culling = registry->HasComponent<CullingComponent>(selectedEntity);
		if(MImGui::Toggle("Culling", &culling)){
			SetEntityHeaderTag<CullingComponent>(
				m_editor,
				context,
				selectedEntity,
				"CullingComponent",
				culling
			);
		}

		ImGui::TableNextColumn();
		bool visible = !registry->HasComponent<HiddenComponent>(selectedEntity);
		if(MImGui::Toggle("Visible", &visible)){
			SetEntityHeaderTag<HiddenComponent>(
				m_editor,
				context,
				selectedEntity,
				"HiddenComponent",
				!visible
			);
		}

		ImGui::EndTable();
	}

	ImGui::PopID();

	ImGui::Dummy(ImVec2(0.0f, 4.0f));
	ImGui::BeginChild(
		"Child",
		ImVec2(0.0f, 0.0f),
		true,
		ImGuiWindowFlags_HorizontalScrollbar
	);

	const std::vector<ComponentView> components =
		registry->GetInspectorComponentViewsOfEntitySorted(selectedEntity);
	std::vector<ComponentView> componentsToRemove;
	ImGuiStorage* sectionStorage = ImGui::GetStateStorage();

	for(ComponentView component : components){
		const std::string componentName =
			registry->GetComponentName(component);
		if(componentName.empty()) continue;

		ImGui::PushID(component.data);
		const ImGuiID openStateID = ImGui::GetID("SectionOpenState");
		bool open = sectionStorage->GetBool(openStateID, true);

		const float removeWidth = 68.0f;
		const float headerWidth = (std::max)(
			80.0f,
			ImGui::GetContentRegionAvail().x - removeWidth - ImGui::GetStyle().ItemSpacing.x
		);

		MImGui::SectionHeader(componentName.c_str(), &open, headerWidth);
		sectionStorage->SetBool(openStateID, open);

		ImGui::SameLine();
		if(MImGui::Button(
			"Remove",
			ImVec2(removeWidth, modernTheme.compactHeight),
			MImGui::ButtonKind::Ghost
		)){
			componentsToRemove.push_back(component);
		}
		ImGui::PopID();

		if(open){
			ImGui::Indent(8.0f);
			ImGui::Dummy(ImVec2(0.0f, 2.0f));
			registry->InspectComponent(component, context);
			ImGui::Unindent(8.0f);
		}
		ImGui::Dummy(ImVec2(0.0f, 3.0f));
	}

	for(ComponentView component : componentsToRemove){
		auto command = std::make_unique<ComponentRemoveCommand>(
			context,
			selectedEntity,
			component
		);
		m_editor->commandManager.Execute(std::move(command));
	}

	ImGui::Separator();
	const float addComponentWidth = ImGui::GetContentRegionAvail().x;
	if(MImGui::Button(
		"+ Add Component",
		ImVec2(addComponentWidth, modernTheme.controlHeight)
	)){
		ImGui::OpenPopup("AddComponentPopup");
	}

	if(ImGui::BeginPopup("AddComponentPopup")){
		static char searchBuffer[128]{};
		MImGui::SearchField(
			"##ComponentSearch",
			"Search component...",
			searchBuffer,
			sizeof(searchBuffer),
			-1.0f
		);
		ImGui::Separator();

		const auto& addableMap = registry->GetAddableComponentList();
		std::vector<std::pair<std::string, std::function<void(Entity)>>> addable;
		addable.assign(addableMap.begin(), addableMap.end());
		std::sort(
			addable.begin(),
			addable.end(),
			[](const auto& left, const auto& right){
				return left.first < right.first;
			}
		);

		ComponentMask currentMask;
		const auto& entityMasks = registry->GetEntityMasks();
		if(auto maskIterator = entityMasks.find(selectedEntity);
			maskIterator != entityMasks.end()){
			currentMask = maskIterator->second;
		}

		std::string searchLower = searchBuffer;
		std::transform(
			searchLower.begin(),
			searchLower.end(),
			searchLower.begin(),
			[](unsigned char value){
				return static_cast<char>(std::tolower(value));
			}
		);

		for(const auto& [name, addFunction] : addable){
			const ComponentTypeID typeID =
				registry->GetComponentIDByName(name);
			if(typeID == INVALID_COMPONENT_TYPE_ID || currentMask.test(typeID)){
				continue;
			}

			std::string lowerName = name;
			std::transform(
				lowerName.begin(),
				lowerName.end(),
				lowerName.begin(),
				[](unsigned char value){
					return static_cast<char>(std::tolower(value));
				}
			);
			if(!searchLower.empty() &&
				lowerName.find(searchLower) == std::string::npos){
				continue;
			}

			if(ImGui::MenuItem(name.c_str())){
				auto command = std::make_unique<ComponentAddCommand>(
					context,
					selectedEntity,
					name,
					addFunction
				);
				m_editor->commandManager.Execute(std::move(command));
			}
		}

		ImGui::EndPopup();
	}

	ImGui::EndChild();
	ImGui::End();
}
