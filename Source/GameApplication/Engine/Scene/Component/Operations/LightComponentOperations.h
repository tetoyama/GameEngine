// =======================================================================
//
// LightComponentOperations.h
//
// LightComponent定義後にincludeする実装ヘッダー。
//
// =======================================================================
#pragma once

#include "Backends/ImGuiFunc.h"
#include "Backends/YAMLConverters.h"
#include "Component/cameraComponent.h"
#include "Engine/Scene/System/Render/Lighting/LocalLightShadowProjection.h"
#include "Engine/Scene/System/Render/Lighting/ShadowBiasPolicy.h"
#include "Engine/Scene/System/Render/Lighting/ShadowBiasSynchronization.h"
#include "Registry/componentRegistry.h"

#include <cstdio>

namespace LightComponentOperations {

inline bool IsLocalLight(const LIGHT& light) noexcept {
	return light.LightType == LIGHT_TYPE_POINT ||
		light.LightType == LIGHT_TYPE_SPOT;
}

inline YAML::Node Encode(const LightComponent& component){
	YAML::Node node;
	const LIGHT& light = component.light;

	node["Enable"] = light.Enable;
	node["LightType"] = light.LightType;
	node["CastShadow"] = light.CastShadow;
	node["Position"] = light.Position;
	node["Direction"] = light.Direction;
	node["Diffuse"] = light.Diffuse;
	node["Ambient"] = light.Ambient;
	node["Param"] = light.Param;
	node["ShadowBias"] = light.ShadowBias;
	node["LightView"] = light.LightView;
	node["LightProjection"] = light.LightProjection;

	YAML::Node csmNode;
	csmNode["CascadeCount"] = component.csm.cascadeCount;
	csmNode["ShadowDistance"] = component.csm.shadowDistance;
	csmNode["SplitMode"] = static_cast<std::uint32_t>(
		component.csm.splitMode
	);
	csmNode["SplitLambda"] = component.csm.splitLambda;
	csmNode["ManualSplitRatios"] = component.csm.manualSplitRatios;
	csmNode["XyMinScale"] = component.csm.xyMinScale;
	csmNode["XyScaleExponent"] = component.csm.xyScaleExponent;
	node["CSM"] = csmNode;
	return node;
}

inline bool Decode(LightComponent& component, const YAML::Node& node){
	if(!node.IsMap()){
		return false;
	}

	LIGHT& light = component.light;
	if(node["Enable"]) light.Enable = node["Enable"].as<BOOL>();
	if(node["LightType"]) light.LightType = node["LightType"].as<UINT>();
	if(node["CastShadow"]) light.CastShadow = node["CastShadow"].as<BOOL>();
	if(node["Position"]) light.Position = node["Position"].as<DirectX::XMFLOAT4>();
	if(node["Direction"]) light.Direction = node["Direction"].as<DirectX::XMFLOAT4>();
	if(node["Diffuse"]) light.Diffuse = node["Diffuse"].as<DirectX::XMFLOAT4>();
	if(node["Ambient"]) light.Ambient = node["Ambient"].as<DirectX::XMFLOAT4>();
	if(node["Param"]) light.Param = node["Param"].as<DirectX::XMFLOAT4>();
	if(node["ShadowBias"]){
		light.ShadowBias = node["ShadowBias"].as<DirectX::XMFLOAT4>();
	}else{
		light.ShadowBias = ShadowBiasPolicy::MakeLegacy(light.Param.w);
	}
	if(node["LightView"]){
		light.LightView = node["LightView"].as<DirectX::XMFLOAT4X4>();
	}
	if(node["LightProjection"]){
		light.LightProjection = node["LightProjection"].as<DirectX::XMFLOAT4X4>();
	}

	if(const YAML::Node csmNode = node["CSM"];
		csmNode && csmNode.IsMap()){
		if(csmNode["CascadeCount"]){
			component.csm.cascadeCount = csmNode["CascadeCount"].as<int>();
		}
		if(csmNode["ShadowDistance"]){
			component.csm.shadowDistance =
				csmNode["ShadowDistance"].as<float>();
		}
		if(csmNode["SplitMode"]){
			component.csm.splitMode = static_cast<CsmSplitMode>(
				csmNode["SplitMode"].as<std::uint32_t>()
			);
		}
		if(csmNode["SplitLambda"]){
			component.csm.splitLambda = csmNode["SplitLambda"].as<float>();
		}
		if(csmNode["ManualSplitRatios"]){
			component.csm.manualSplitRatios =
				csmNode["ManualSplitRatios"].as<DirectX::XMFLOAT4>();
		}
		if(csmNode["XyMinScale"]){
			component.csm.xyMinScale = csmNode["XyMinScale"].as<float>();
		}
		if(csmNode["XyScaleExponent"]){
			component.csm.xyScaleExponent =
				csmNode["XyScaleExponent"].as<float>();
		}
	}
	component.csm.Normalize();

	ShadowBiasSynchronization::Apply(light);
	if(IsLocalLight(light)){
		light.Param.x =
			LocalLightShadowProjection::ResolveFarPlane(light.Param.x);
	}

	component.dirty = true;
	return true;
}

struct CsmInspectorCameraRange {
	float nearClip = 0.1f;
	float cameraFar = 1024.0f;
	float shadowFar = 1024.0f;
	bool usesSceneCamera = false;
};

inline CsmInspectorCameraRange ResolveCsmInspectorCameraRange(
	SceneContext* context,
	const CsmDirectionalLightSettings& csm
){
	CsmInspectorCameraRange range;
	if(context && context->component){
		const auto cameraEntities =
			context->component->FindEntitiesWithComponent<CameraComponent>();
		for(Entity entity : cameraEntities){
			const CameraComponent* camera =
				context->component->GetComponent<CameraComponent>(entity);
			if(!camera || camera->FarClip <= camera->NearClip){
				continue;
			}
			range.nearClip = (std::max)(camera->NearClip, 0.1f);
			range.cameraFar = camera->FarClip;
			range.usesSceneCamera = true;
			break;
		}
	}

	range.shadowFar = csm.ResolveShadowFar(
		range.nearClip,
		range.cameraFar
	);
	return range;
}

inline void ResolveCsmInspectorSplits(
	const CsmDirectionalLightSettings& csm,
	const CsmInspectorCameraRange& range,
	float splitRatios[DIRECTIONAL_CSM_CASCADE_COUNT],
	float splitDistances[DIRECTIONAL_CSM_CASCADE_COUNT]
){
	const float depthSpan = (std::max)(
		range.shadowFar - range.nearClip,
		0.001f
	);
	float previousRatio = 0.0f;

	for(int c = 0; c < csm.cascadeCount; ++c){
		float splitDistance = range.shadowFar;
		if(c < csm.cascadeCount - 1){
			if(csm.splitMode == CsmSplitMode::Manual){
				const float ratio =
					csm.ResolveManualSplitRatio(c, csm.cascadeCount);
				splitDistance =
					range.nearClip + depthSpan * ratio;
			}else{
				const float p = static_cast<float>(c + 1) /
					static_cast<float>(csm.cascadeCount);
				const float logarithmic = range.nearClip *
					static_cast<float>(std::pow(
						range.shadowFar / range.nearClip,
						p
					));
				const float uniform =
					range.nearClip + depthSpan * p;
				splitDistance =
					csm.splitLambda * logarithmic +
					(1.0f - csm.splitLambda) * uniform;
			}
		}

		float ratio =
			(splitDistance - range.nearClip) / depthSpan;
		ratio = (std::clamp)(ratio, previousRatio, 1.0f);
		if(c == csm.cascadeCount - 1){
			ratio = 1.0f;
			splitDistance = range.shadowFar;
		}

		splitRatios[c] = ratio;
		splitDistances[c] = splitDistance;
		previousRatio = ratio;
	}

	for(int c = csm.cascadeCount;
		c < DIRECTIONAL_CSM_CASCADE_COUNT;
		++c){
		splitRatios[c] = 1.0f;
		splitDistances[c] = range.shadowFar;
	}
}

inline ImU32 ResolveCsmInspectorColor(int cascadeIndex){
	static const ImVec4 colors[DIRECTIONAL_CSM_CASCADE_COUNT] = {
		ImVec4(0.24f, 0.55f, 0.90f, 1.0f),
		ImVec4(0.30f, 0.72f, 0.56f, 1.0f),
		ImVec4(0.91f, 0.63f, 0.24f, 1.0f),
		ImVec4(0.68f, 0.48f, 0.84f, 1.0f)
	};
	return ImGui::GetColorU32(
		colors[(std::clamp)(
			cascadeIndex,
			0,
			DIRECTIONAL_CSM_CASCADE_COUNT - 1
		)]
	);
}

inline void DrawCsmSplitPreview(
	const CsmDirectionalLightSettings& csm,
	const CsmInspectorCameraRange& range,
	const float splitRatios[DIRECTIONAL_CSM_CASCADE_COUNT],
	const float splitDistances[DIRECTIONAL_CSM_CASCADE_COUNT]
){
	ImGui::TextUnformatted("Cascade Splits");

	const ImVec2 barPosition = ImGui::GetCursorScreenPos();
	const float barWidth = (std::max)(
		ImGui::GetContentRegionAvail().x,
		1.0f
	);
	const float barHeight = ImGui::GetFrameHeight();
	ImGui::InvisibleButton(
		"##CascadeSplitPreview",
		ImVec2(barWidth, barHeight)
	);

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const float rounding = ImGui::GetStyle().FrameRounding;
	const ImU32 borderColor = ImGui::GetColorU32(ImGuiCol_Border);
	const ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);

	float previousRatio = 0.0f;
	for(int c = 0; c < csm.cascadeCount; ++c){
		const float ratio = splitRatios[c];
		const float x0 = barPosition.x + previousRatio * barWidth;
		const float x1 = barPosition.x + ratio * barWidth;
		const ImVec2 segmentMin(x0, barPosition.y);
		const ImVec2 segmentMax(x1, barPosition.y + barHeight);

		drawList->AddRectFilled(
			segmentMin,
			segmentMax,
			ResolveCsmInspectorColor(c)
		);

		char label[8]{};
		std::snprintf(label, sizeof(label), "C%d", c);
		const ImVec2 labelSize = ImGui::CalcTextSize(label);
		if(x1 - x0 >= labelSize.x + 8.0f){
			drawList->AddText(
				ImVec2(
					(x0 + x1 - labelSize.x) * 0.5f,
					barPosition.y +
						(barHeight - labelSize.y) * 0.5f
				),
				textColor,
				label
			);
		}

		if(c < csm.cascadeCount - 1){
			drawList->AddLine(
				ImVec2(x1, barPosition.y),
				ImVec2(x1, barPosition.y + barHeight),
				borderColor,
				2.0f
			);
		}
		previousRatio = ratio;
	}

	drawList->AddRect(
		barPosition,
		ImVec2(
			barPosition.x + barWidth,
			barPosition.y + barHeight
		),
		borderColor,
		rounding
	);

	if(ImGui::IsItemHovered()){
		ImGui::BeginTooltip();
		float previousDistance = range.nearClip;
		for(int c = 0; c < csm.cascadeCount; ++c){
			ImGui::Text(
				"C%d  %.1f - %.1f  |  detail %.2fx",
				c,
				previousDistance,
				splitDistances[c],
				1.0f / (std::max)(
					csm.ResolveXyScale(c, csm.cascadeCount),
					0.001f
				)
			);
			previousDistance = splitDistances[c];
		}
		ImGui::EndTooltip();
	}

	ImGui::TextDisabled(
		"%.1f - %.1f units%s",
		range.nearClip,
		range.shadowFar,
		range.usesSceneCamera ? "" : " (preview)"
	);
}

inline bool InspectShadowBiasValues(LIGHT& light){
	bool changed = false;
	ShadowBiasSynchronization::Apply(light);

	if(ShadowBiasPolicy::GetMode(light.ShadowBias) ==
	   ShadowBiasPolicy::Mode::LegacyNdc){
		changed |= ImGui::UndoDragFloat(
			"Bias",
			&light.ShadowBias.x,
			0.00001f,
			0.0f,
			ShadowBiasPolicy::MaximumLegacyNdcBias,
			"%.7f"
		);
		if(ImGui::IsItemHovered()){
			ImGui::SetTooltip(
				"Reduces self-shadowing. Larger values can detach contact shadows."
			);
		}
		ImGui::TextDisabled(
			"Normal Bias is available in World Space mode."
		);
	}else{
		changed |= ImGui::UndoDragFloat(
			"Bias",
			&light.ShadowBias.x,
			0.0001f,
			0.0f,
			ShadowBiasPolicy::MaximumWorldBias,
			"%.6f"
		);
		if(ImGui::IsItemHovered()){
			ImGui::SetTooltip(
				"Moves the receiver toward the light in world units."
			);
		}

		changed |= ImGui::UndoDragFloat(
			"Normal Bias",
			&light.ShadowBias.y,
			0.0001f,
			0.0f,
			ShadowBiasPolicy::MaximumWorldBias,
			"%.6f"
		);
		if(ImGui::IsItemHovered()){
			ImGui::SetTooltip(
				"Offsets the receiver along its normal at grazing light angles."
			);
		}
	}

	ShadowBiasSynchronization::Apply(light);
	return changed;
}

inline bool InspectShadowBiasMode(LIGHT& light){
	const char* biasModes[] = {
		"Legacy NDC",
		"World Space"
	};
	int selectedMode = static_cast<int>(
		ShadowBiasPolicy::GetMode(light.ShadowBias)
	);
	if(!ImGui::Combo(
		"Bias Mode",
		&selectedMode,
		biasModes,
		IM_ARRAYSIZE(biasModes)
	)){
		return false;
	}

	const auto mode = selectedMode == SHADOW_BIAS_MODE_WORLD_SPACE
		? ShadowBiasPolicy::Mode::WorldSpace
		: ShadowBiasPolicy::Mode::LegacyNdc;
	ShadowBiasPolicy::SetMode(light.ShadowBias, mode, light.Param.w);
	ShadowBiasSynchronization::Apply(light);
	return true;
}

inline bool InspectCastShadow(LIGHT& light){
	bool castShadow = light.CastShadow != FALSE;
	if(!ImGui::UndoCheckbox("Cast Shadows", &castShadow)){
		return false;
	}
	light.CastShadow = castShadow ? TRUE : FALSE;
	return true;
}

inline bool InspectCsmSettings(
	LightComponent& component,
	SceneContext* context
){
	LIGHT& light = component.light;
	CsmDirectionalLightSettings& csm = component.csm;
	bool changed = false;
	csm.Normalize();

	ImGui::Separator();
	if(!ImGui::CollapsingHeader(
		"Shadows",
		ImGuiTreeNodeFlags_DefaultOpen
	)){
		return false;
	}

	changed |= InspectCastShadow(light);
	if(light.CastShadow == FALSE){
		ImGui::TextDisabled("Shadow rendering is disabled for this light.");
		return changed;
	}

	const bool useCameraFar = csm.shadowDistance <= 0.0f;
	if(useCameraFar){
		ImGui::TextUnformatted("Distance");
		ImGui::SameLine();
		ImGui::TextDisabled("Camera Far Clip");
		ImGui::SameLine();
		if(ImGui::SmallButton("Override##CsmDistance")){
			csm.shadowDistance = 256.0f;
			changed = true;
		}
	}else{
		changed |= ImGui::UndoDragFloat(
			"Distance",
			&csm.shadowDistance,
			1.0f,
			0.1f,
			100000.0f,
			"%.1f"
		);
		ImGui::SameLine();
		if(ImGui::SmallButton("Use Camera Far##CsmDistance")){
			csm.shadowDistance = 0.0f;
			changed = true;
		}
	}
	if(ImGui::IsItemHovered()){
		ImGui::SetTooltip(
			"Shorter shadow distance improves detail and can reduce shadow casters."
		);
	}

	const char* cascadeLabels[] = {
		"1 Cascade",
		"2 Cascades",
		"3 Cascades",
		"4 Cascades"
	};
	int cascadeSelection = csm.cascadeCount - 1;
	if(ImGui::Combo(
		"Cascades",
		&cascadeSelection,
		cascadeLabels,
		IM_ARRAYSIZE(cascadeLabels)
	)){
		csm.cascadeCount = cascadeSelection + 1;
		changed = true;
	}
	if(ImGui::IsItemHovered()){
		ImGui::SetTooltip(
			"Each cascade adds one complete shadow render. "
			"The tested default is 3 cascades."
		);
	}

	const char* distributionLabels[] = {
		"Automatic",
		"Manual"
	};
	int distributionSelection =
		csm.splitMode == CsmSplitMode::Manual ? 1 : 0;
	if(ImGui::Combo(
		"Distribution",
		&distributionSelection,
		distributionLabels,
		IM_ARRAYSIZE(distributionLabels)
	)){
		csm.splitMode = distributionSelection == 1
			? CsmSplitMode::Manual
			: CsmSplitMode::Practical;
		changed = true;
	}

	csm.Normalize();
	const CsmInspectorCameraRange cameraRange =
		ResolveCsmInspectorCameraRange(context, csm);
	float splitRatios[DIRECTIONAL_CSM_CASCADE_COUNT] = {};
	float splitDistances[DIRECTIONAL_CSM_CASCADE_COUNT] = {};
	ResolveCsmInspectorSplits(
		csm,
		cameraRange,
		splitRatios,
		splitDistances
	);
	DrawCsmSplitPreview(
		csm,
		cameraRange,
		splitRatios,
		splitDistances
	);

	if(csm.splitMode == CsmSplitMode::Manual && csm.cascadeCount > 1){
		float* ratios = reinterpret_cast<float*>(&csm.manualSplitRatios);
		for(int i = 0; i < csm.cascadeCount - 1; ++i){
			char label[48]{};
			std::snprintf(
				label,
				sizeof(label),
				"Cascade %d Split",
				i + 1
			);
			const float minimum = i == 0
				? CsmDirectionalLightSettings::MinimumSplitGap
				: ratios[i - 1] +
					CsmDirectionalLightSettings::MinimumSplitGap;
			const float maximum = 1.0f -
				CsmDirectionalLightSettings::MinimumSplitGap *
				static_cast<float>(csm.cascadeCount - 1 - i);
			float splitPercent = ratios[i] * 100.0f;
			if(ImGui::SliderFloat(
				label,
				&splitPercent,
				minimum * 100.0f,
				maximum * 100.0f,
				"%.1f%%"
			)){
				ratios[i] = splitPercent / 100.0f;
				changed = true;
			}
			csm.Normalize();
		}
	}

	float nearDetail = 1.0f / (std::max)(csm.xyMinScale, 0.001f);
	if(ImGui::SliderFloat(
		"Near Detail",
		&nearDetail,
		1.0f,
		10.0f,
		"%.1fx"
	)){
		csm.xyMinScale = 1.0f / (std::max)(nearDetail, 1.0f);
		changed = true;
	}
	if(ImGui::IsItemHovered()){
		ImGui::SetTooltip(
			"Concentrates the nearest cascade on a smaller world area. "
			"2x matches the tested macro configuration."
		);
	}

	ImGui::Spacing();
	changed |= InspectShadowBiasValues(light);

	ImGui::TextDisabled(
		"%d shadow renders/frame  |  %.0f%% of 4-cascade cost",
		csm.cascadeCount,
		100.0f * static_cast<float>(csm.cascadeCount) /
			static_cast<float>(CsmDirectionalLightSettings::MaximumCascadeCount)
	);

	if(ImGui::TreeNode("Advanced")){
		if(csm.splitMode == CsmSplitMode::Practical){
			changed |= ImGui::UndoSliderFloat(
				"Split Distribution",
				&csm.splitLambda,
				0.0f,
				1.0f,
				"%.3f"
			);
			if(ImGui::IsItemHovered()){
				ImGui::SetTooltip(
					"0 is uniform; 1 concentrates split distance near the camera."
				);
			}
		}

		changed |= ImGui::UndoSliderFloat(
			"Detail Falloff",
			&csm.xyScaleExponent,
			0.10f,
			8.0f,
			"%.3f"
		);
		if(ImGui::IsItemHovered()){
			ImGui::SetTooltip(
				"Controls how quickly Near Detail returns to full outer coverage."
			);
		}

		changed |= InspectShadowBiasMode(light);

		if(ImGui::Button("Reset to Tested Defaults")){
			csm.ResetToTestedDefaults();
			changed = true;
		}
		if(ImGui::IsItemHovered()){
			ImGui::SetTooltip(
				"Restores the last macro-tested state: 3 cascades, Camera Far, "
				"PSSM 0.85, Near Detail 2x and Detail Falloff 1.5."
			);
		}
		if(csm.IsTestedDefault()){
			ImGui::SameLine();
			ImGui::TextDisabled("Tested Default");
		}

		ImGui::TreePop();
	}

	csm.Normalize();
	return changed;
}

inline bool InspectLocalLightSettings(LIGHT& light){
	bool changed = false;
	changed |= InspectCastShadow(light);

	changed |= ImGui::UndoDragFloat(
		"Range",
		&light.Param.x,
		0.1f,
		LocalLightShadowProjection::NearPlane +
			LocalLightShadowProjection::MinimumDepthSpan,
		100000.0f,
		"%.3f"
	);
	if(ImGui::IsItemHovered()){
		ImGui::SetTooltip(
			"Lighting attenuation and local shadow Far use the same range."
		);
	}

	if(light.LightType == LIGHT_TYPE_SPOT){
		changed |= ImGui::UndoDragFloat(
			"Inner Angle",
			&light.Param.y,
			0.1f,
			0.0f,
			179.0f,
			"%.2f deg"
		);
		changed |= ImGui::UndoDragFloat(
			"Outer Angle",
			&light.Param.z,
			0.1f,
			0.01f,
			179.0f,
			"%.2f deg"
		);
	}

	light.Param.x =
		LocalLightShadowProjection::ResolveFarPlane(light.Param.x);
	if(light.CastShadow != FALSE){
		ImGui::Separator();
		ImGui::TextDisabled("Shadow Bias");
		changed |= InspectShadowBiasValues(light);
		if(ImGui::TreeNode("Advanced Shadow")){
			changed |= InspectShadowBiasMode(light);
			ImGui::TreePop();
		}
	}
	return changed;
}

inline void Inspect(
	LightComponent& component,
	SceneContext* context
){
	LIGHT& light = component.light;
	bool changed = false;

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 6.0f));

	bool enabled = light.Enable != FALSE;
	if(ImGui::UndoCheckbox("Enable", &enabled)){
		light.Enable = enabled ? TRUE : FALSE;
		changed = true;
	}

	const char* lightTypes[] = {
		"None",
		"Directional",
		"Point",
		"Spot",
		"Directional CSM"
	};
	int selectedType = static_cast<int>(light.LightType);
	if(ImGui::Combo(
		"Type",
		&selectedType,
		lightTypes,
		IM_ARRAYSIZE(lightTypes)
	)){
		light.LightType = static_cast<UINT>(selectedType);
		if(IsLocalLight(light)){
			light.Param.x =
				LocalLightShadowProjection::ResolveFarPlane(light.Param.x);
		}
		changed = true;
	}

	changed |= ImGui::UndoColorEdit4(
		"Color",
		reinterpret_cast<float*>(&light.Diffuse)
	);
	changed |= ImGui::UndoColorEdit4(
		"Ambient",
		reinterpret_cast<float*>(&light.Ambient)
	);

	component.csm.Normalize();
	if(light.LightType == LIGHT_TYPE_DIRECTIONAL_CSM){
		changed |= InspectCsmSettings(component, context);
	}else if(IsLocalLight(light)){
		ImGui::Separator();
		if(ImGui::CollapsingHeader(
			"Light and Shadows",
			ImGuiTreeNodeFlags_DefaultOpen
		)){
			changed |= InspectLocalLightSettings(light);
		}
	}else{
		changed |= ImGui::UndoDragFloat3(
			"Light Param XYZ",
			reinterpret_cast<float*>(&light.Param),
			0.1f
		);
		if(light.LightType == LIGHT_TYPE_DIRECTIONAL){
			ImGui::Separator();
			if(ImGui::CollapsingHeader(
				"Shadows",
				ImGuiTreeNodeFlags_DefaultOpen
			)){
				changed |= InspectCastShadow(light);
				if(light.CastShadow != FALSE){
					changed |= InspectShadowBiasValues(light);
				}
			}
		}
	}

	ImGui::PopStyleVar();
	component.dirty |= changed;
}

} // namespace LightComponentOperations

inline YAML::Node LightComponent::encode(){
	return LightComponentOperations::Encode(*this);
}

inline bool LightComponent::decode(
	SceneContext*,
	const YAML::Node& node
){
	return LightComponentOperations::Decode(*this, node);
}

inline void LightComponent::inspector(SceneContext* context){
	LightComponentOperations::Inspect(*this, context);
}
