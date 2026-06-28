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
	const StaticBatchPacketCacheTelemetry staticCache =
		renderSystem->GetRenderPacketBuffer().StaticBatchCacheTelemetry();
	const size_t totalGrowth =
		entityGrowth + componentGrowth + visibilityGrowth +
		packet.growthEventCount + staticBatch.growthEventCount +
		staticCache.growthEventCount;

	ImGui::SetNextWindowSize(ImVec2(650.0f, 420.0f), ImGuiCond_FirstUseEver);
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
		ImGui::Text("Render Packet Safety Overrides: %llu",
			static_cast<unsigned long long>(packet.safetyGrowthOverrideCount));
		ImGui::Text("Static Batch Candidate Growth: %llu",
			static_cast<unsigned long long>(staticBatch.growthEventCount));
		ImGui::Text("Static Packet Cache Growth: %llu",
			static_cast<unsigned long long>(staticCache.growthEventCount));
		ImGui::Text("Total Growth: %llu",
			static_cast<unsigned long long>(totalGrowth));
		ImGui::Separator();
		ImGui::Text(
			"Render Packet Buffer: Current %llu / Peak %llu / Capacity %llu",
			static_cast<unsigned long long>(packet.currentSize),
			static_cast<unsigned long long>(packet.peakSize),
			static_cast<unsigned long long>(packet.capacity)
		);
		if(packet.lastMergeUsedSafetyGrowth){
			ImGui::TextColored(
				ImVec4(1.0f, 0.55f, 0.2f, 1.0f),
				"RenderPacket capacity exceeded while runtime growth was disabled; complete-frame safety growth was used."
			);
		}
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
		ImGui::Text(
			"Cache Ready Groups: Current %llu / Peak %llu",
			static_cast<unsigned long long>(staticBatch.currentCacheReadyGroupCount),
			static_cast<unsigned long long>(staticBatch.peakCacheReadyGroupCount)
		);
		ImGui::Text(
			"Static Packet Cache Entries: Current %llu / Peak %llu / Capacity %llu",
			static_cast<unsigned long long>(staticCache.currentEntryCount),
			static_cast<unsigned long long>(staticCache.peakEntryCount),
			static_cast<unsigned long long>(staticCache.entryCapacity)
		);
		ImGui::Text(
			"Static Packet Cache Instances: Current %llu / Peak %llu / Capacity %llu",
			static_cast<unsigned long long>(staticCache.currentInstanceCount),
			static_cast<unsigned long long>(staticCache.peakInstanceCount),
			static_cast<unsigned long long>(staticCache.instanceCapacity)
		);
		ImGui::Text(
			"Cache Rebuilds %llu / Skipped Incomplete Groups %llu",
			static_cast<unsigned long long>(staticCache.rebuildCount),
			static_cast<unsigned long long>(staticCache.skippedIncompleteGroupCount)
		);
		if(staticBatch.overflowed || staticCache.overflowed){
			ImGui::TextColored(
				ImVec4(1.0f, 0.7f, 0.2f, 1.0f),
				"Static batching overflowed; ordinary RenderPacket submission remains active."
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
