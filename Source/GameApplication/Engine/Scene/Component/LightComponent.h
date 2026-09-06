// =======================================================================
//
// LightComponent.h
//
// =======================================================================
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "Interface/IComponent.h"
#include "LightComponentDefaults.h"

enum class CsmSplitMode : std::uint32_t {
	Practical = 0,
	Manual = 1
};

// Directional CSMのCPU側設定。
// GPUのLIGHT構造体は変更せず、ShadowMapPassが最大4個のLIGHT Entryへ展開する。
struct CsmDirectionalLightSettings {
	static constexpr int MinimumCascadeCount = 1;
	static constexpr int MaximumCascadeCount =
		DIRECTIONAL_CSM_CASCADE_COUNT;
	static constexpr float MinimumSplitGap = 0.01f;

	// Runtime設定化直前のMacro検証状態。
	// UI改修や新規Scene生成でこの基準が変わらないよう、既定値を一箇所に固定する。
	static constexpr int TestedDefaultCascadeCount = 3;
	static constexpr float TestedDefaultShadowDistance = 0.0f;
	static constexpr float TestedDefaultSplitLambda = 0.85f;
	static constexpr float TestedDefaultXyMinScale = 0.50f;
	static constexpr float TestedDefaultXyScaleExponent = 1.50f;

	int cascadeCount = TestedDefaultCascadeCount;
	// 0の場合はActive CameraのFarClipを使用する。
	float shadowDistance = TestedDefaultShadowDistance;
	CsmSplitMode splitMode = CsmSplitMode::Practical;
	float splitLambda = TestedDefaultSplitLambda;
	DirectX::XMFLOAT4 manualSplitRatios = {
		0.10f,
		0.30f,
		0.60f,
		1.0f
	};
	// 最終Cascadeは常に1.0。近距離CascadeのみXY World範囲を縮小する。
	float xyMinScale = TestedDefaultXyMinScale;
	float xyScaleExponent = TestedDefaultXyScaleExponent;

	void ResetToTestedDefaults() noexcept {
		cascadeCount = TestedDefaultCascadeCount;
		shadowDistance = TestedDefaultShadowDistance;
		splitMode = CsmSplitMode::Practical;
		splitLambda = TestedDefaultSplitLambda;
		manualSplitRatios = DirectX::XMFLOAT4(
			0.10f,
			0.30f,
			0.60f,
			1.0f
		);
		xyMinScale = TestedDefaultXyMinScale;
		xyScaleExponent = TestedDefaultXyScaleExponent;
	}

	bool IsTestedDefault() const noexcept {
		constexpr float epsilon = 0.0001f;
		return cascadeCount == TestedDefaultCascadeCount &&
			std::fabs(shadowDistance - TestedDefaultShadowDistance) <= epsilon &&
			splitMode == CsmSplitMode::Practical &&
			std::fabs(splitLambda - TestedDefaultSplitLambda) <= epsilon &&
			std::fabs(xyMinScale - TestedDefaultXyMinScale) <= epsilon &&
			std::fabs(
				xyScaleExponent - TestedDefaultXyScaleExponent
			) <= epsilon;
	}

	void Normalize() noexcept {
		cascadeCount = (std::clamp)(
			cascadeCount,
			MinimumCascadeCount,
			MaximumCascadeCount
		);
		shadowDistance = (std::max)(shadowDistance, 0.0f);
		splitLambda = (std::clamp)(splitLambda, 0.0f, 1.0f);
		xyMinScale = (std::clamp)(xyMinScale, 0.10f, 1.0f);
		xyScaleExponent = (std::clamp)(
			xyScaleExponent,
			0.10f,
			8.0f
		);

		const std::uint32_t rawMode =
			static_cast<std::uint32_t>(splitMode);
		splitMode = rawMode == static_cast<std::uint32_t>(CsmSplitMode::Manual)
			? CsmSplitMode::Manual
			: CsmSplitMode::Practical;

		manualSplitRatios.x = (std::clamp)(
			manualSplitRatios.x,
			MinimumSplitGap,
			1.0f - MinimumSplitGap * 3.0f
		);
		manualSplitRatios.y = (std::clamp)(
			manualSplitRatios.y,
			manualSplitRatios.x + MinimumSplitGap,
			1.0f - MinimumSplitGap * 2.0f
		);
		manualSplitRatios.z = (std::clamp)(
			manualSplitRatios.z,
			manualSplitRatios.y + MinimumSplitGap,
			1.0f - MinimumSplitGap
		);
		manualSplitRatios.w = 1.0f;
	}

	float ResolveShadowFar(float csmNear, float cameraFar) const noexcept {
		const float safeCameraFar = (std::max)(cameraFar, csmNear + 0.1f);
		if(shadowDistance <= 0.0f){
			return safeCameraFar;
		}
		return (std::clamp)(
			shadowDistance,
			csmNear + 0.1f,
			safeCameraFar
		);
	}

	float ResolveManualSplitRatio(
		int cascadeIndex,
		int resolvedCascadeCount
	) const noexcept {
		if(cascadeIndex >= resolvedCascadeCount - 1){
			return 1.0f;
		}
		switch(cascadeIndex){
		case 0: return manualSplitRatios.x;
		case 1: return manualSplitRatios.y;
		case 2: return manualSplitRatios.z;
		default: return 1.0f;
		}
	}

	float ResolveXyScale(
		int cascadeIndex,
		int resolvedCascadeCount
	) const noexcept {
		if(resolvedCascadeCount <= 1){
			return 1.0f;
		}
		const float t = static_cast<float>(cascadeIndex) /
			static_cast<float>(resolvedCascadeCount - 1);
		return xyMinScale +
			(1.0f - xyMinScale) *
			static_cast<float>(std::pow(t, xyScaleExponent));
	}
};

// ライト設定を保持するComponent。
// YAMLとInspector実装はLightComponentOperationsへ分離する。
class LightComponent {
public:
	LIGHT light = LightComponentDefaults::Create();
	CsmDirectionalLightSettings csm{};
	bool dirty = false;

	LightComponent() = default;

	YAML::Node encode();
	bool decode(SceneContext* context, const YAML::Node& node);
	void inspector(SceneContext* context);
};

#include "Operations/LightComponentOperations.h"
