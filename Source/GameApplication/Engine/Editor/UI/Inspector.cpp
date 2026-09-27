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
#include "Editor/UI/ModernImGui/EditorIconWidgets.h"
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
	NameComponent* name = registry->GetComponent<NameComponent>(selectedEntity);
	const bool isPrefab =
		registry->GetComponent<PrefabComponent>(selectedEntity) != nullptr;

	ImGui::PushID("EntityHeader");
	bool deleteEntity = false;

	// Identity is presented as one stable material surface. The icon is used
	// once as an anchor; repeated rows below rely on labels and structure.
	ImGui::PushStyleColor(ImGuiCol_ChildBg, MImGui::WithAlpha(modernTheme.panel, 0.96f));
	ImGui::PushStyleColor(ImGuiCol_Border, MImGui::EffectiveOutline());
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, modernTheme.cornerRadius);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
	if(ImGui::BeginChild(
		"EntityIdentitySurface",
		ImVec2(0.0f, 62.0f),
		true,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
	)){
		const ImVec2 baseCursor = ImGui::GetCursorPos();
		const ImVec2 iconScreen = ImGui::GetCursorScreenPos();
		const float iconTileSize = 36.0f;
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->AddRectFilled(
			iconScreen,
			ImVec2(iconScreen.x + iconTileSize, iconScreen.y + iconTileSize),
			ImGui::GetColorU32(modernTheme.raised),
			10.0f
		);
		drawList->AddRect(
			iconScreen,
			ImVec2(iconScreen.x + iconTileSize, iconScreen.y + iconTileSize),
			ImGui::GetColorU32(MImGui::EffectiveOutline()),
			10.0f,
			0,
			MImGui::EffectiveStrokeWidth()
		);
		MImGui::DrawMaterialEdge(
			drawList,
			iconScreen,
			ImVec2(iconScreen.x + iconTileSize, iconScreen.y + iconTileSize),
			10.0f,
			0.75f
		);
		MImGui::DrawEditorIcon(
			m_editor->icons.Get(EditorIcon::Entity),
			ImVec2(iconScreen.x + 8.0f, iconScreen.y + 8.0f),
			20.0f,
			0.94f
		);
		ImGui::Dummy(ImVec2(iconTileSize, iconTileSize));

		const float contentX = baseCursor.x + iconTileSize + 10.0f;
		const float actionWidth = 30.0f;
		const float actionGap = ImGui::GetStyle().ItemSpacing.x;
		const float contentRight = ImGui::GetWindowContentRegionMax().x;
		const float nameWidth = (std::max)(
			96.0f,
			contentRight - contentX - actionWidth - actionGap
		);

		ImGui::SetCursorPos(ImVec2(contentX, baseCursor.y));
		if(name){
			char nameBuffer[256]{};
			std::strncpy(nameBuffer, name->name.c_str(), sizeof(nameBuffer) - 1);

			if(MImGui::TextField(
				"##Name",
				nameBuffer,
				sizeof(nameBuffer),
				ImGuiInputTextFlags_EnterReturnsTrue,
				nameWidth
			)){
				auto command = std::make_unique<RenameCommand>(
					context,
					selectedEntity,
					name->name,
					nameBuffer
				);
				m_editor->commandManager.Execute(std::move(command));
			}
		}else{
			ImGui::Dummy(ImVec2(nameWidth, modernTheme.compactHeight));
		}

		ImGui::SameLine();
		if(MImGui::Button(
			"...##EntityActions",
			ImVec2(actionWidth, modernTheme.compactHeight),
			MImGui::ButtonKind::Ghost
		)){
			ImGui::OpenPopup("EntityActionsPopup");
		}

		if(ImGui::BeginPopup("EntityActionsPopup")){
			ImGui::TextDisabled("Scene Entity · ID %u", selectedEntity.GetIndex());
			ImGui::Separator();
			ImGui::PushStyleColor(ImGuiCol_Text, modernTheme.dangerHover);
			deleteEntity = ImGui::MenuItem("Delete Entity");
			ImGui::PopStyleColor();
			ImGui::EndPopup();
		}

		ImGui::SetCursorPos(ImVec2(contentX, baseCursor.y + modernTheme.compactHeight + 4.0f));
		ImGui::TextDisabled("Scene Entity · ID %u", selectedEntity.GetIndex());
		if(isPrefab){
			ImGui::SameLine();
			MImGui::Badge("Prefab");
		}
	}
	ImGui::EndChild();
	ImGui::PopStyleVar(3);
	ImGui::PopStyleColor(2);
	ImGui::PopID();

	if(deleteEntity){
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
		ImGui::End();
		return;
	}

	ImGui::Dummy(ImVec2(0.0f, 8.0f));
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

	ImGui::Dummy(ImVec2(0.0f, 6.0f));
	ImGui::BeginChild(
		"Child",
		ImVec2(0.0f, 0.0f),
		false,
		0
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

		const float actionWidth = 32.0f;
		const float headerWidth = (std::max)(
			80.0f,
			ImGui::GetContentRegionAvail().x -
			actionWidth - ImGui::GetStyle().ItemSpacing.x
		);

		MImGui::IconSectionHeader(
			"ComponentHeader",
			componentName.c_str(),
			m_editor->icons.GetForComponent(componentName),
			&open,
			headerWidth
		);
		sectionStorage->SetBool(openStateID, open);

		ImGui::SameLine();
		if(MImGui::Button(
			"...##ComponentActions",
			ImVec2(actionWidth, modernTheme.compactHeight),
			MImGui::ButtonKind::Ghost
		)){
			ImGui::OpenPopup("ComponentActionsPopup");
		}

		if(ImGui::BeginPopup("ComponentActionsPopup")){
			ImGui::TextDisabled("%s", componentName.c_str());
			ImGui::Separator();
			ImGui::PushStyleColor(ImGuiCol_Text, modernTheme.dangerHover);
			if(ImGui::MenuItem("Remove Component")){
				componentsToRemove.push_back(component);
			}
			ImGui::PopStyleColor();
			ImGui::EndPopup();
		}
		ImGui::PopID();

		if(open){
			ImGui::Indent(8.0f);
			ImGui::Dummy(ImVec2(0.0f, 4.0f));
			registry->InspectComponent(component, context);
			ImGui::Unindent(8.0f);
		}
		ImGui::Dummy(ImVec2(0.0f, 8.0f));
	}

	for(ComponentView component : componentsToRemove){
		auto command = std::make_unique<ComponentRemoveCommand>(
			context,
			selectedEntity,
			component
		);
		m_editor->commandManager.Execute(std::move(command));
	}

	ImGui::Dummy(ImVec2(0.0f, 2.0f));
	const float addComponentWidth = ImGui::GetContentRegionAvail().x;
	if(MImGui::IconButton(
		"AddComponent",
		"Add Component",
		m_editor->icons.Get(EditorIcon::Add),
		ImVec2(addComponentWidth, modernTheme.controlHeight),
		MImGui::ButtonKind::Secondary
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
