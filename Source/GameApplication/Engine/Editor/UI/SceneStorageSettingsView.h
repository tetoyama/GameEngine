// =======================================================================
//
// SceneStorageSettingsView.h
//
// =======================================================================
#pragma once

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Backends/ImGui/imgui.h"
#include "Editor/editorService.h"
#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Scene/Component/CullingComponent.h"
#include "Scene/Component/EntityStateComponents.h"
#include "Scene/Component/modelRendererComponent.h"
#include "Scene/Component/transformComponent.h"
#include "Scene/Registry/componentRegistry.h"
#include "Scene/Registry/entityRegistry.h"
#include "Scene/Registry/systemRegistry.h"
#include "Scene/System/Render/RenderSystem/renderSystem.h"

namespace SceneStorageSettingsView {

struct StorageMetrics {
	size_t entities = 0;
	size_t transforms = 0;
	size_t renderables = 0;
	size_t cullingComponents = 0;
	size_t staticEntities = 0;
	size_t transformPages = 0;
	size_t staticTagPages = 0;
	size_t renderPackets = 0;
};

struct ViewState {
	std::string selectedSceneName;
	std::unordered_map<uint32_t, StorageMetrics> peaks;
	float peakMargin = 0.25f;
	double lastSaveTime = -1000.0;
};

inline ViewState& GetState(){
	static ViewState state;
	return state;
}

inline bool DrawUnsignedProperty(
	const char* label,
	const char* id,
	uint32_t& value,
	uint32_t minimum = 0
){
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(label);
	ImGui::TableSetColumnIndex(1);
	ImGui::SetNextItemWidth(-1.0f);

	int edited = static_cast<int>((std::min)(
		value,
		static_cast<uint32_t>(INT_MAX)
	));
	const int minValue = static_cast<int>((std::min)(
		minimum,
		static_cast<uint32_t>(INT_MAX)
	));
	if(!ImGui::DragInt(id, &edited, 1.0f, minValue, INT_MAX)) return false;
	value = static_cast<uint32_t>((std::max)(edited, minValue));
	return true;
}

inline std::shared_ptr<Scene> DrawSceneSelector(
	SceneManager& sceneManager,
	std::string& selectedSceneName
){
	const auto& activeScenes = sceneManager.GetActiveScenes();
	if(activeScenes.empty()){
		selectedSceneName.clear();
		return {};
	}

	std::vector<std::string> sceneNames;
	sceneNames.reserve(activeScenes.size());
	for(const auto& [name, scene] : activeScenes){
		if(scene) sceneNames.push_back(name);
	}
	std::sort(sceneNames.begin(), sceneNames.end());
	if(sceneNames.empty()) return {};

	if(std::find(sceneNames.begin(), sceneNames.end(), selectedSceneName) ==
		sceneNames.end()){
		selectedSceneName = sceneNames.front();
	}

	ImGui::SetNextItemWidth(-1.0f);
	if(ImGui::BeginCombo("##SceneStorageScene", selectedSceneName.c_str())){
		for(const std::string& name : sceneNames){
			const bool selected = name == selectedSceneName;
			if(ImGui::Selectable(name.c_str(), selected)){
				selectedSceneName = name;
			}
			if(selected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	auto iterator = activeScenes.find(selectedSceneName);
	return iterator != activeScenes.end() ? iterator->second : std::shared_ptr<Scene>{};
}

inline size_t CountRenderPackets(
	SystemRegistry* registry,
	uint32_t sceneContextID
){
	if(!registry || sceneContextID == 0) return 0;
	RenderSystem* renderSystem = registry->GetSystem<RenderSystem>();
	if(!renderSystem) return 0;

	size_t count = 0;
	for(const RenderPacket& packet : renderSystem->GetRenderPacketBuffer().Packets()){
		if(packet.sceneContextID == sceneContextID) ++count;
	}
	return count;
}

inline StorageMetrics ReadMetrics(
	SceneContext& context,
	SystemRegistry* systemRegistry
){
	StorageMetrics metrics;
	if(context.entity){
		metrics.entities = context.entity->GetAliveCount();
	}
	if(context.component){
		metrics.transforms =
			context.component->GetComponentStorageSize<TransformComponent>();
		metrics.renderables =
			context.component->GetComponentStorageSize<ModelRendererComponent>();
		metrics.cullingComponents =
			context.component->GetComponentStorageSize<CullingComponent>();
		metrics.staticEntities =
			context.component->GetComponentStorageSize<StaticEntityComponent>();

		metrics.transformPages =
			context.component->GetComponentStorageCapacity<TransformComponent>() /
			SceneStorageConfig::DirectPagedPageSize;
		metrics.staticTagPages =
			context.component->GetComponentStorageCapacity<StaticEntityComponent>() /
			SceneStorageConfig::DirectPagedPageSize;
	}
	metrics.renderPackets = CountRenderPackets(systemRegistry, context.contextID);
	return metrics;
}

inline void UpdatePeak(StorageMetrics& peak, const StorageMetrics& current){
	peak.entities = (std::max)(peak.entities, current.entities);
	peak.transforms = (std::max)(peak.transforms, current.transforms);
	peak.renderables = (std::max)(peak.renderables, current.renderables);
	peak.cullingComponents =
		(std::max)(peak.cullingComponents, current.cullingComponents);
	peak.staticEntities = (std::max)(peak.staticEntities, current.staticEntities);
	peak.transformPages = (std::max)(peak.transformPages, current.transformPages);
	peak.staticTagPages = (std::max)(peak.staticTagPages, current.staticTagPages);
	peak.renderPackets = (std::max)(peak.renderPackets, current.renderPackets);
}

inline void DrawMetricRow(
	const char* label,
	size_t configured,
	size_t current,
	size_t peak
){
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::TextUnformatted(label);
	ImGui::TableSetColumnIndex(1);
	ImGui::Text("%llu", static_cast<unsigned long long>(configured));
	ImGui::TableSetColumnIndex(2);
	ImGui::Text("%llu", static_cast<unsigned long long>(current));
	ImGui::TableSetColumnIndex(3);
	ImGui::Text("%llu", static_cast<unsigned long long>(peak));
}

inline uint32_t ResolvePeakValue(size_t peak, float margin){
	const double scaled = std::ceil(
		static_cast<double>((std::max)(peak, size_t{1})) *
		(1.0 + static_cast<double>((std::max)(margin, 0.0f)))
	);
	return static_cast<uint32_t>((std::min)(
		scaled,
		static_cast<double>((std::numeric_limits<uint32_t>::max)())
	));
}

inline void ApplyPeak(
	SceneStorageConfig& config,
	const StorageMetrics& peak,
	float margin
){
	config.expectedEntityCount = ResolvePeakValue(peak.entities, margin);
	config.expectedTransformCount = ResolvePeakValue(peak.transforms, margin);
	config.expectedRenderableCount = ResolvePeakValue(peak.renderables, margin);
	config.expectedCullingCount = ResolvePeakValue(peak.cullingComponents, margin);
	config.expectedStaticEntityCount = ResolvePeakValue(peak.staticEntities, margin);
	config.preallocatedTransformPages =
		SceneStorageConfig::RequiredPageCount(config.expectedTransformCount);
	config.preallocatedTagPages =
		SceneStorageConfig::RequiredPageCount(config.expectedStaticEntityCount);
	config.renderPacketReserve = ResolvePeakValue(peak.renderPackets, margin);
	config.visibleEntityReserve = ResolvePeakValue(peak.cullingComponents, margin);
	config.Normalize();
}

inline void DrawConfiguredSettings(SceneStorageConfig& config){
	bool changed = false;

	ImGui::SeparatorText("Entity Storage");
	if(ImGui::BeginTable(
		"SceneStorageEntitySettings",
		2,
		ImGuiTableFlags_SizingStretchProp |
		ImGuiTableFlags_BordersInnerV |
		ImGuiTableFlags_NoSavedSettings
	)){
		ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 220.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
		changed |= DrawUnsignedProperty(
			"Expected Entities", "##ExpectedEntities", config.expectedEntityCount, 1);
		changed |= DrawUnsignedProperty(
			"Expected Transforms", "##ExpectedTransforms", config.expectedTransformCount);
		changed |= DrawUnsignedProperty(
			"Expected Renderables", "##ExpectedRenderables", config.expectedRenderableCount);
		changed |= DrawUnsignedProperty(
			"Expected Culling Components", "##ExpectedCulling", config.expectedCullingCount);
		changed |= DrawUnsignedProperty(
			"Expected Static Entities", "##ExpectedStatic", config.expectedStaticEntityCount);
		ImGui::EndTable();
	}

	ImGui::SeparatorText("Direct Paged Storage");
	if(ImGui::BeginTable(
		"SceneStoragePagedSettings",
		2,
		ImGuiTableFlags_SizingStretchProp |
		ImGuiTableFlags_BordersInnerV |
		ImGuiTableFlags_NoSavedSettings
	)){
		ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 220.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextUnformatted("Page Size");
		ImGui::TableSetColumnIndex(1);
		ImGui::TextDisabled("%u (Engine fixed)", SceneStorageConfig::DirectPagedPageSize);
		changed |= DrawUnsignedProperty(
			"Preallocated Transform Pages",
			"##PreallocatedTransformPages",
			config.preallocatedTransformPages
		);
		changed |= DrawUnsignedProperty(
			"Preallocated Tag Pages",
			"##PreallocatedTagPages",
			config.preallocatedTagPages
		);
		ImGui::EndTable();
	}

	ImGui::SeparatorText("Rendering Reserve");
	if(ImGui::BeginTable(
		"SceneStorageRenderingSettings",
		2,
		ImGuiTableFlags_SizingStretchProp |
		ImGuiTableFlags_BordersInnerV |
		ImGuiTableFlags_NoSavedSettings
	)){
		ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 220.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
		changed |= DrawUnsignedProperty(
			"Render Packets", "##RenderPacketReserve", config.renderPacketReserve);
		changed |= DrawUnsignedProperty(
			"Visible Entities", "##VisibleEntityReserve", config.visibleEntityReserve);
		changed |= DrawUnsignedProperty(
			"Static Batches", "##StaticBatchReserve", config.staticBatchReserve);
		ImGui::EndTable();
	}

	ImGui::SeparatorText("Runtime Growth");
	changed |= ImGui::Checkbox("Allow Automatic Growth", &config.allowRuntimeGrowth);
	changed |= ImGui::Checkbox("Log Capacity Growth", &config.logCapacityGrowth);
	if(changed) config.Normalize();
}

inline void DrawMetrics(
	SceneContext& context,
	const StorageMetrics& current,
	StorageMetrics& peak,
	ViewState& state,
	SceneStorageConfig& config
){
	UpdatePeak(peak, current);

	ImGui::SeparatorText("Measured Usage");
	if(ImGui::BeginTable(
		"SceneStorageMetrics",
		4,
		ImGuiTableFlags_Borders |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_SizingStretchProp
	)){
		ImGui::TableSetupColumn("Storage", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Configured", ImGuiTableColumnFlags_WidthFixed, 100.0f);
		ImGui::TableSetupColumn("Current", ImGuiTableColumnFlags_WidthFixed, 90.0f);
		ImGui::TableSetupColumn("Peak", ImGuiTableColumnFlags_WidthFixed, 90.0f);
		ImGui::TableHeadersRow();
		DrawMetricRow("Entities", config.expectedEntityCount, current.entities, peak.entities);
		DrawMetricRow("Transforms", config.expectedTransformCount, current.transforms, peak.transforms);
		DrawMetricRow("Renderables", config.expectedRenderableCount, current.renderables, peak.renderables);
		DrawMetricRow("Culling Components", config.expectedCullingCount, current.cullingComponents, peak.cullingComponents);
		DrawMetricRow("Static Entities", config.expectedStaticEntityCount, current.staticEntities, peak.staticEntities);
		DrawMetricRow("Transform Pages", config.ResolveTransformPageCount(), current.transformPages, peak.transformPages);
		DrawMetricRow("Static Tag Pages", config.ResolveTagPageCount(), current.staticTagPages, peak.staticTagPages);
		DrawMetricRow("Render Packets", config.renderPacketReserve, current.renderPackets, peak.renderPackets);
		ImGui::EndTable();
	}

	const size_t growthEvents = context.entity
		? context.entity->GetGrowthEventCount()
		: 0;
	ImGui::Text(
		"Entity Capacity Growth Events: %llu",
		static_cast<unsigned long long>(growthEvents)
	);

	ImGui::SetNextItemWidth(180.0f);
	ImGui::SliderFloat("Peak Margin", &state.peakMargin, 0.0f, 1.0f, "%.0f%%");
	if(ImGui::IsItemHovered()){
		ImGui::SetTooltip("Margin is added when applying measured peaks to scene defaults.");
	}

	const float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
	if(ImGui::Button("Apply Measured Peak", ImVec2(buttonWidth, 0.0f))){
		ApplyPeak(config, peak, 0.0f);
	}
	ImGui::SameLine();
	if(ImGui::Button("Apply Peak + Margin", ImVec2(buttonWidth, 0.0f))){
		ApplyPeak(config, peak, state.peakMargin);
	}

	if(ImGui::Button("Reset Runtime Peaks", ImVec2(-1.0f, 0.0f))){
		peak = current;
		if(context.entity) context.entity->ResetPeakMetrics();
	}
}

inline void Draw(EditorService* editor, bool* open){
	if(!open || !*open) return;

	ImGui::SetNextWindowSize(ImVec2(820.0f, 760.0f), ImGuiCond_FirstUseEver);
	if(!ImGui::Begin("Scene Settings", open)){
		ImGui::End();
		return;
	}

	SceneManager* sceneManager = editor ? editor->sceneManager : nullptr;
	if(!sceneManager){
		ImGui::TextDisabled("SceneManager is not available.");
		ImGui::End();
		return;
	}

	ViewState& state = GetState();
	ImGui::TextUnformatted("Scene");
	std::shared_ptr<Scene> scene =
		DrawSceneSelector(*sceneManager, state.selectedSceneName);
	if(!scene){
		ImGui::TextDisabled("No active scene is available.");
		ImGui::End();
		return;
	}

	SceneContext* context = scene->GetSceneContext();
	if(!context){
		ImGui::TextDisabled("SceneContext is not available.");
		ImGui::End();
		return;
	}

	if(ImGui::BeginTabBar("SceneSettingsTabs")){
		if(ImGui::BeginTabItem("Storage")){
			SceneStorageConfig& config = scene->GetStorageConfig();
			DrawConfiguredSettings(config);

			const StorageMetrics current = ReadMetrics(
				*context,
				sceneManager->GetSystemRegistry()
			);
			StorageMetrics& peak = state.peaks[context->contextID];
			DrawMetrics(*context, current, peak, state, config);

			ImGui::Spacing();
			ImGui::Separator();
			if(ImGui::Button("Reset Storage Defaults", ImVec2(-1.0f, 0.0f))){
				config = SceneStorageConfig{};
			}
			if(ImGui::Button("Save Scene Storage Settings", ImVec2(-1.0f, 0.0f))){
				config.Normalize();
				scene->Save();
				state.lastSaveTime = ImGui::GetTime();
			}
			if(ImGui::GetTime() - state.lastSaveTime < 2.0){
				ImGui::TextDisabled("Scene storage settings saved.");
			}
			ImGui::TextDisabled(
				"Reserve changes are stored in SceneSettings.Storage and take full effect after scene reload."
			);
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	ImGui::End();
}

} // namespace SceneStorageSettingsView
