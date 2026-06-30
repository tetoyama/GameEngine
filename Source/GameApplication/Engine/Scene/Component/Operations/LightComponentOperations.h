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
#include "Engine/Scene/System/Render/Lighting/LocalLightShadowProjection.h"
#include "Engine/Scene/System/Render/Lighting/ShadowBiasPolicy.h"
#include "Engine/Scene/System/Render/Lighting/ShadowBiasSynchronization.h"

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

	ShadowBiasSynchronization::Apply(light);
	if(IsLocalLight(light)){
		light.Param.x =
			LocalLightShadowProjection::ResolveFarPlane(light.Param.x);
	}

	component.dirty = true;
	return true;
}

inline bool InspectShadowBias(LIGHT& light){
	bool changed = false;
	ShadowBiasSynchronization::Apply(light);

	const char* biasModes[] = {
		"Legacy NDC",
		"World Space"
	};
	int selectedMode = static_cast<int>(
		ShadowBiasPolicy::GetMode(light.ShadowBias)
	);
	if(ImGui::Combo(
		"Shadow Bias Mode",
		&selectedMode,
		biasModes,
		IM_ARRAYSIZE(biasModes)
	)){
		const auto mode = selectedMode == SHADOW_BIAS_MODE_WORLD_SPACE
			? ShadowBiasPolicy::Mode::WorldSpace
			: ShadowBiasPolicy::Mode::LegacyNdc;
		ShadowBiasPolicy::SetMode(light.ShadowBias, mode, light.Param.w);
		changed = true;
	}

	if(ShadowBiasPolicy::GetMode(light.ShadowBias) ==
	   ShadowBiasPolicy::Mode::LegacyNdc){
		changed |= ImGui::UndoDragFloat(
			"Depth Bias (NDC)",
			&light.ShadowBias.x,
			0.00001f,
			0.0f,
			ShadowBiasPolicy::MaximumLegacyNdcBias,
			"%.7f"
		);
		if(ImGui::IsItemHovered()){
			ImGui::SetTooltip(
				"Backward-compatible projected-depth bias. Existing scenes use this mode. "
				"Its world-space effect changes with projection and receiver distance."
			);
		}
		ImGui::TextDisabled(
			"Legacy compatibility mode: Normal Bias is disabled."
		);
	}else{
		changed |= ImGui::UndoDragFloat(
			"Depth Bias (World)",
			&light.ShadowBias.x,
			0.0001f,
			0.0f,
			ShadowBiasPolicy::MaximumWorldBias,
			"%.6f"
		);
		if(ImGui::IsItemHovered()){
			ImGui::SetTooltip(
				"Moves the shadow receiver toward the light before projection. "
				"The value is expressed in world units and is independent of Far."
			);
		}

		changed |= ImGui::UndoDragFloat(
			"Normal Bias (World)",
			&light.ShadowBias.y,
			0.0001f,
			0.0f,
			ShadowBiasPolicy::MaximumWorldBias,
			"%.6f"
		);
		if(ImGui::IsItemHovered()){
			ImGui::SetTooltip(
				"Offsets the receiver along its normal. The offset increases toward "
				"grazing light angles and is zero when the normal faces the light."
			);
		}
	}

	ShadowBiasSynchronization::Apply(light);
	return changed;
}

inline void Inspect(LightComponent& component){
	LIGHT& light = component.light;
	bool changed = false;

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 6.0f));

	bool enabled = light.Enable != FALSE;
	if(ImGui::UndoCheckbox("Enable", &enabled)){
		light.Enable = enabled ? TRUE : FALSE;
		changed = true;
	}

	bool castShadow = light.CastShadow != FALSE;
	if(ImGui::UndoCheckbox("CastShadow", &castShadow)){
		light.CastShadow = castShadow ? TRUE : FALSE;
		changed = true;
	}
	if(ImGui::IsItemHovered()){
		ImGui::SetTooltip(
			"Shadow generation is optional and defaults to OFF for new lights. "
			"The light remains active when CastShadow is OFF."
		);
	}

	const char* lightTypes[] = {
		"None",
		"Directional",
		"Point",
		"Spot",
		"DirectionalCSM (Cascaded)"
	};
	int selectedType = static_cast<int>(light.LightType);
	if(ImGui::Combo(
		"Light Type",
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

	if(light.CastShadow != FALSE){
		if(light.LightType == LIGHT_TYPE_POINT){
			ImGui::TextDisabled(
				"Point shadow expands to 6 atlas faces. Enable only when required."
			);
		}else if(light.LightType == LIGHT_TYPE_DIRECTIONAL_CSM){
			ImGui::TextDisabled(
				"CSM shadow expands to %d cascade entries.",
				DIRECTIONAL_CSM_CASCADE_COUNT
			);
		}
	}

	changed |= ImGui::UndoColorEdit4(
		"Diffuse",
		reinterpret_cast<float*>(&light.Diffuse)
	);
	changed |= ImGui::UndoColorEdit4(
		"Ambient",
		reinterpret_cast<float*>(&light.Ambient)
	);

	if(IsLocalLight(light)){
		changed |= ImGui::UndoDragFloat(
			"Range / Shadow Far",
			&light.Param.x,
			0.1f,
			LocalLightShadowProjection::NearPlane +
				LocalLightShadowProjection::MinimumDepthSpan,
			100000.0f,
			"%.3f"
		);
		if(ImGui::IsItemHovered()){
			ImGui::SetTooltip(
				"Point and Spot lighting attenuation reaches zero at this distance. "
				"The same value is used as the perspective shadow Far plane."
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
		ImGui::TextDisabled(
			"Shadow Clip: Near %.3f / Far %.3f / Ratio %.1f",
			LocalLightShadowProjection::NearPlane,
			LocalLightShadowProjection::ResolveFarPlane(light.Param.x),
			LocalLightShadowProjection::DepthRatio(light.Param.x)
		);
		if(LocalLightShadowProjection::DepthRatio(light.Param.x) > 10000.0f){
			ImGui::TextWrapped(
				"Warning: a very large Far/Near ratio reduces perspective shadow depth precision."
			);
		}
	}else{
		changed |= ImGui::UndoDragFloat3(
			"Light Param XYZ",
			reinterpret_cast<float*>(&light.Param),
			0.1f
		);
	}

	ImGui::Separator();
	ImGui::TextDisabled("Shadow Receiver Bias");
	changed |= InspectShadowBias(light);

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

inline void LightComponent::inspector(SceneContext*){
	LightComponentOperations::Inspect(*this);
}
