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
	SceneManager& manager,
	const std::string& selectedName
){
	const auto& scenes = manager.GetActiveScenes();
	auto selected = scenes.find(selectedName);
	if(selected != scenes.end() && selected->second){
		return selected->second;
	}
	for(const auto& [name, scene] : scenes){
		(void)name;
		if(scene) return scene;
	}
	return {};
}

inline void Draw(
	EditorService* editor,
	bool visible,
	const std::string& selectedSceneName
){
	if(!visible || !editor || !editor->sceneManager) return;

	SceneManager& manager = *editor->sceneManager;
	std::shared_ptr<Scene> scene = ResolveScene(manager, selectedSceneName);
	if(!scene) return;

	SceneContext* context = scene->GetSceneContext();
	RenderSystem* renderSystem = manager.GetSystemRegistry()
		? manager.GetSystemRegistry()->GetSystem<RenderSystem>()
		: nullptr;
	if(!context || !renderSystem) return;

	const size_t entityGrowth = context->entity
		? context->entity->GetGrowthEventCount()
		: 0;
	const size_t componentGrowth = context->component
		? context->component->GetTotalComponentStorageGrowthEventCount()
		: 0;
	const size_t visibilityGrowth =
		renderSystem->GetCullingVisibility().GrowthEventCount(context->contextID);
	const RenderPacketStorageTelemetry packet =
		renderSystem->GetRenderPacketBuffer().Telemetry();
	const StaticBatchCandidateStorageTelemetry staticBatch =
		renderSystem->GetRenderPacketBuffer().StaticBatchTelemetry();
	const size_t totalGrowth =
		entityGrowth +
		componentGrowth +
		visibilityGrowth +
		packet.growthEventCount +
		staticBatch.growthEventCount;

	ImGui::SetNextWindowSize(ImVec2(590.0f, 320.0f), ImGuiCond_FirstUseEver);
	if(ImGui::Begin("Scene Storage Runtime Telemetry", nullptr)){
		ImGui::Text("Scene: %s", scene->SceneName.c_str());
		ImGui::Text("Entity Growth: %llu",
			static_cast<unsigned long long>(entityGrowth));
		ImGui::Text("Component Storage Growth: %llu",
			static_cast<unsigned long long>(componentGrowth));
		ImGui::Text("Visibility Growth: %llu",
			static_cast<unsigned long long>(visibilityGrowth));
		ImGui::Text("Render Packet Growth: %llu",
			static_cast<unsigned long long>(packet.growthEventCount));
		ImGui::Text("Static Batch Candidate Growth: %llu",
			static_cast<unsigned long long>(staticBatch.growthEventCount));
		ImGui::Text("Total Growth: %llu",
			static_cast<unsigned long long>(totalGrowth));
		ImGui::Text(
			"Render Packet Buffer: Current %llu / Peak %llu / Capacity %llu",
			static_cast<unsigned long long>(packet.currentSize),
			static_cast<unsigned long long>(packet.peakSize),
			static_cast<unsigned long long>(packet.capacity)
		);
		ImGui::Text(
			"Static Batch Candidates: Current %llu / Peak %llu / Capacity %llu",
			static_cast<unsigned long long>(staticBatch.currentSize),
			static_cast<unsigned long long>(staticBatch.peakSize),
			static_cast<unsigned long long>(staticBatch.capacity)
		);
		ImGui::Text(
			"Static Batch Groups: Current %llu / Peak %llu / Capacity %llu",
			static_cast<unsigned long long>(staticBatch.currentGroupCount),
			static_cast<unsigned long long>(staticBatch.peakGroupCount),
			static_cast<unsigned long long>(staticBatch.groupCapacity)
		);
		if(staticBatch.overflowed){
			ImGui::TextColored(
				ImVec4(1.0f, 0.7f, 0.2f, 1.0f),
				"Static batch candidates overflowed; ordinary RenderPacket submission remains active."
			);
		}

		if(ImGui::Button("Reset All Runtime Storage Peaks", ImVec2(-1.0f, 0.0f))){
			if(context->entity) context->entity->ResetPeakMetrics();
			if(context->component){
				context->component->ResetAllComponentStoragePeakMetrics();
			}
			renderSystem->GetCullingVisibility().ResetPeakMetrics(context->contextID);
			renderSystem->GetRenderPacketBuffer().ResetPeakMetrics();
			SceneStorageSettingsView::GetState().peaks[context->contextID] =
				SceneStorageSettingsView::ReadMetrics(*context, renderSystem);
		}
	}
	ImGui::End();
}

} // namespace SceneStorageRuntimeTelemetryView
