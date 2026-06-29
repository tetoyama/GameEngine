#pragma once

#include "Backends/ImGui/imgui.h"
#include "Scene/sceneManager.h"
#include "Scene/Registry/systemRegistry.h"
#include "System/Render/RenderSystem/renderSystem.h"
#include "System/Render/RenderSystem/RenderPass/EditorView/EditorPass.h"
#include "System/Render/RenderSystem/RenderPass/GBuffer/GBufferPass.h"
#include "System/Render/RenderSystem/RenderPass/PlayerView/PlayerPass.h"
#include "System/Render/StaticBatch/StaticBatchUploadSystem.h"

namespace StaticBatchTelemetryUI {

inline void DrawMetricRow(
	const char* name,
	std::size_t value,
	const char* meaning
){
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::TextUnformatted(name);
	ImGui::TableSetColumnIndex(1);
	ImGui::Text("%zu", value);
	ImGui::TableSetColumnIndex(2);
	ImGui::TextDisabled("%s", meaning);
}

inline bool BeginMetricTable(const char* id){
	if(!ImGui::BeginTable(
		id,
		3,
		ImGuiTableFlags_BordersInnerV |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_SizingStretchProp
	)){
		return false;
	}
	ImGui::TableSetupColumn("Metric");
	ImGui::TableSetupColumn("Value");
	ImGui::TableSetupColumn("Meaning");
	ImGui::TableHeadersRow();
	return true;
}

inline void DrawGBufferTelemetry(
	const char* label,
	const GBufferPass* pass
){
	ImGui::SeparatorText(label);
	if(!pass){
		ImGui::TextDisabled("GBufferPass is not available.");
		return;
	}

	const auto& t = pass->GetStaticBatchTelemetry();
	if(BeginMetricTable(label)){
		DrawMetricRow("Candidate Groups", t.candidateGroupCount, "CPU candidate groups");
		DrawMetricRow("Visible Instances", t.visibleInstanceCount, "Instances selected for this view");
		DrawMetricRow("Culled Instances", t.culledInstanceCount, "Instances omitted from this view");
		DrawMetricRow("Compacted Mixed Groups", t.compactedMixedGroupCount, "Visible-only rebuilt groups");
		DrawMetricRow("Submitted Groups", t.submittedGroupCount, "Instanced groups submitted");
		DrawMetricRow("Submitted Instances", t.submittedInstanceCount, "Instances submitted");
		DrawMetricRow("Ordinary Draws Replaced", t.ReplacedOrdinaryDrawCallCount(), "Ordinary GBuffer draws removed");
		DrawMetricRow("Static Draw Calls", t.StaticDrawCallCount(), "Static GBuffer draw calls");
		DrawMetricRow("Estimated Draw Reduction", t.EstimatedDrawCallReduction(), "Replaced minus static draws");
		DrawMetricRow("Selection Failures", t.visibilitySelectionFailureCount, "View compaction failures");
		DrawMetricRow("Single Instance", t.singleInstanceFallbackCount, "Groups too small to instance");
		DrawMetricRow("Invalid Packet Range", t.packetRangeFallbackCount, "Packet or uint32 range failures");
		DrawMetricRow("Layer Fallback", t.layerFallbackCount, "Hidden or invalid layers");
		DrawMetricRow("Culled Groups", t.culledGroupCount, "Fully invisible groups");
		DrawMetricRow("Geometry Fallback", t.geometryFallbackCount, "Geometry unavailable");
		DrawMetricRow("Material Fallback", t.materialFallbackCount, "Material equivalence unavailable");
		DrawMetricRow("Draw Failures", t.drawFailureCount, "Draw creation failures");
		DrawMetricRow("Queue Failures", t.queueFailureCount, "Queue submission failures");
		DrawMetricRow("Target Failures", t.targetBindingFailureCount, "GBuffer target binding failures");
		ImGui::EndTable();
	}

	ImGui::Text(
		"Pipeline: %s | View Upload: %s | Revision: %llu",
		t.pipelineReady ? "Ready" : "Unavailable",
		t.instanceUploadReady ? "Ready" : "Unavailable",
		static_cast<unsigned long long>(t.sourceRevision)
	);
}

inline void DrawShadowTelemetry(const StaticBatchUploadSystem* upload){
	ImGui::SeparatorText("Shadow Submission (Player + Editor)");
	if(!upload){
		ImGui::TextDisabled("StaticBatchUploadSystem is not available.");
		return;
	}

	const auto& t = upload->GetShadowSubmissionTelemetry();
	if(BeginMetricTable("StaticBatchShadowSubmissionTelemetry")){
		DrawMetricRow("Light Tile Attempts", t.submissionAttemptCount, "Attempts since telemetry reset");
		DrawMetricRow("Candidate Group Visits", t.candidateGroupCount, "Groups evaluated across all light tiles");
		DrawMetricRow("Submitted Groups", t.submittedGroupCount, "Instanced shadow groups submitted");
		DrawMetricRow("Submitted Instances", t.submittedInstanceCount, "Ordinary shadow draws replaced");
		DrawMetricRow("Ordinary Draws Replaced", t.ReplacedOrdinaryDrawCount(), "Ordinary model shadow draws removed");
		DrawMetricRow("Static Draw Calls", t.StaticDrawCallCount(), "Static shadow draw calls");
		DrawMetricRow("Estimated Draw Reduction", t.EstimatedDrawCallReduction(), "Replaced minus static draws");
		DrawMetricRow("Mapping Fallback", t.mappingFallbackCount, "CPU/cache group mismatch");
		DrawMetricRow("Eligibility Fallback", t.eligibilityFallbackCount, "Eligibility contract unavailable");
		DrawMetricRow("Layer Fallback", t.layerFallbackCount, "Hidden or invalid layers");
		DrawMetricRow("Geometry Fallback", t.geometryFallbackCount, "Geometry unavailable");
		DrawMetricRow("Draw Failures", t.drawFailureCount, "Draw creation failures");
		DrawMetricRow("Queue Failures", t.queueFailureCount, "Queue submission failures");
		ImGui::EndTable();
	}
	ImGui::Text(
		"Shadow Pipeline: %s | Cumulative until telemetry reset.",
		upload->IsShadowPipelineReady() ? "Ready" : "Unavailable"
	);
}

inline void Draw(SceneManager* sceneManager){
	SystemRegistry* registry =
		sceneManager ? sceneManager->GetSystemRegistry() : nullptr;
	if(!registry){
		ImGui::TextDisabled("SystemRegistry is not available.");
		return;
	}

	StaticBatchUploadSystem* upload =
		registry->GetSystem<StaticBatchUploadSystem>();
	RenderSystem* render = registry->GetSystem<RenderSystem>();

	ImGui::SeparatorText("Source Upload / Geometry Cache");
	if(!upload){
		ImGui::TextDisabled("StaticBatchUploadSystem is not available.");
	}else{
		const auto instance = upload->GetTelemetry();
		const auto geometry = upload->GetGeometryTelemetry();
		ImGui::Text(
			"GBuffer Pipeline: %s | Shadow Pipeline: %s | Source Upload: %s",
			upload->IsPipelineReady() ? "Ready" : "Unavailable",
			upload->IsShadowPipelineReady() ? "Ready" : "Unavailable",
			upload->LastUploadSucceeded() ? "Succeeded" : "Unavailable"
		);
		ImGui::Text(
			"Source Instances: %zu / %zu | Uploads: %zu | Reallocations: %zu | Failures: %zu | Growth Denied: %zu",
			instance.currentInstanceCount,
			instance.instanceCapacity,
			instance.uploadCount,
			instance.reallocationCount,
			instance.failedUploadCount,
			instance.growthDeniedCount
		);
		ImGui::Text(
			"Geometry Bindings: %zu | Groups: %zu | Creates: %zu | Reuses: %zu | Rejects: %zu",
			geometry.currentBindingCount,
			geometry.currentResolvedGroupCount,
			geometry.createCount,
			geometry.reuseCount,
			geometry.rejectedGroupCount
		);
	}

	DrawShadowTelemetry(upload);

	const GBufferPass* playerPass =
		render && render->m_PlayerPass
			? render->m_PlayerPass->gBufferPass
			: nullptr;
	const GBufferPass* editorPass =
		render && render->m_EditorPass
			? render->m_EditorPass->gBufferPass
			: nullptr;
	DrawGBufferTelemetry("Player GBuffer", playerPass);
	DrawGBufferTelemetry("Editor GBuffer", editorPass);
}

} // namespace StaticBatchTelemetryUI
