#pragma once

#include <cstddef>
#include <memory>
#include <string>

#include "Backends/ImGui/imgui.h"
#include "Editor/editorService.h"
#include "Editor/UI/SceneStorageSettingsView.h"
#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Scene/Registry/systemRegistry.h"
#include "Scene/System/Render/RenderSystem/renderSystem.h"

namespace SceneStorageRuntimeTelemetryView {

inline std::shared_ptr<Scene> ResolveScene(
	SceneManager& sceneManager,
	const std::string& selectedSceneName
){
	const auto& scenes = sceneManager.GetActiveScenes();
	if(!selectedSceneName.empty()){
		auto iterator = scenes.find(selectedSceneName);
		if(iterator != scenes.end() && iterator->second){
			return iterator->second;
		}
	}
	for(const auto& [name, scene] : scenes){
		(void)name;
		if(scene) return scene;
	}
	return {};
}

inline void DrawMetric(
	const char* name,
	size_t current,
	size_t peak,
	size_t capacity,
	size_t growth
){
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::TextUnformatted(name);
	ImGui::TableSetColumnIndex(1);
	ImGui::Text("%llu", static_cast<unsigned long long>(current));
	ImGui::TableSetColumnIndex(2);
	ImGui::Text("%llu", static_cast<unsigned long long>(peak));
	ImGui::TableSetColumnIndex(3);
	ImGui::Text("%llu", static_cast<unsigned long long>(capacity));
	ImGui::TableSetColumnIndex(4);
	ImGui::Text("%llu", static_cast<unsigned long long>(growth));
}

inline void Draw(
	EditorService* editor,
	bool visible,
	const std::string& selectedSceneName
){
	if(!visible || !editor || !editor->sceneManager) return;

	SceneManager& sceneManager = *editor->sceneManager;
	std::shared_ptr<Scene> scene = ResolveScene(sceneManager, selectedSceneName);
	if(!scene) return;

	SceneContext* context = scene->GetSceneContext();
	if(!context) return;

	RenderSystem* renderSystem = sceneManager.GetSystemRegistry()
		? sceneManager.GetSystemRegistry()->GetSystem<RenderSystem>()
		: nullptr;

	const size_t entityGrowth = context->entity
		? context->entity->GetGrowthEventCount()
		: 0;
	const size_t componentGrowth = context->component
		? context->component->GetTotalComponentStorageGrowthEventCount()
		: 0;
	const size_t visibilityGrowth = renderSystem
		? renderSystem->GetCullingVisibility().GrowthEventCount(context->contextID)
		: 0;
	const RenderPacketStorageTelemetry packetTelemetry = renderSystem
		? renderSystem->GetRenderPacketBuffer().Telemetry()
		: RenderPacketStorageTelemetry{};
	const size_t totalGrowth =
		entityGrowth +
		componentGrowth +
		visibilityGrowth +
		packetTelemetry.growthEventCount;

	ImGui::SetNextWindowSize(ImVec2(620.0f, 300.0f), ImGuiCond_FirstUseEver);
	if(!ImGui::Begin("Scene Storage Runtime Telemetry", nullptr)){
		ImGui::End();
		return;
	}

	ImGui::Text("Scene: %s", scene->GetName().c_str());
	ImGui::Text(
		"Total Runtime Storage Growth Events: %llu",
		static_cast<unsigned long long>(totalGrowth)
	);

	if(ImGui::BeginTable(
		"RuntimeStorageTelemetry",
		5,
		ImGuiTableFlags_Borders |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_SizingStretchProp
	)){
		ImGui::TableSetupColumn("Storage", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Current", ImGuiTableColumnFlags_WidthFixed, 80.0f);
		ImGui::TableSetupColumn("Peak", ImGuiTableColumnFlags_WidthFixed, 80.0f);
		ImGui::TableSetupColumn("Capacity", ImGuiTableColumnFlags_WidthFixed, 90.0f);
		ImGui::TableSetupColumn("Growth", ImGuiTableColumnFlags_WidthFixed, 80.0f);
		ImGui::TableHeadersRow();

		DrawMetric(
			"Entities",
			context->entity ? context->entity->GetAliveCount() : 0,
			context->entity ? context->entity->GetPeakAliveCount() : 0,
			context->entity ? context->entity->GetCapacity() : 0,
			entityGrowth
		);
		DrawMetric(
			"Component Storages",
			0,
			0,
			0,
			componentGrowth
		);
		DrawMetric(
			"Visibility / View",
			renderSystem
				? renderSystem->GetCullingVisibility().MaxVisibleCount(context->contextID)
				: 0,
			renderSystem
				? renderSystem->GetCullingVisibility().PeakVisibleCount(context->contextID)
				: 0,
			0,
			visibilityGrowth
		);
		DrawMetric(
			"Global Render Packet Buffer",
			packetTelemetry.currentSize,
			packetTelemetry.peakSize,
			packetTelemetry.capacity,
			packetTelemetry.growthEventCount
		);
		ImGui::EndTable();
	}

	if(ImGui::Button("Reset All Runtime Storage Peaks", ImVec2(-1.0f, 0.0f))){
		if(context->entity){
			context->entity->ResetPeakMetrics();
		}
		if(context->component){
			context->component->ResetAllComponentStoragePeakMetrics();
		}
		if(renderSystem){
			renderSystem->GetCullingVisibility().ResetPeakMetrics(context->contextID);
			renderSystem->GetRenderPacketBuffer().ResetPeakMetrics();
		}

		auto& state = SceneStorageSettingsView::GetState();
		state.peaks[context->contextID] =
			SceneStorageSettingsView::ReadMetrics(*context, renderSystem);
	}

	ImGui::TextDisabled(
		"Render Packet Buffer is global. Entity, Component and Visibility values are selected-scene scoped."
	);
	ImGui::End();
}

} // namespace SceneStorageRuntimeTelemetryView
