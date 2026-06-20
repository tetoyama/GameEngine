// =======================================================================
//
// LightComponent.h
//
// =======================================================================
#pragma once

#include "Interface/IComponent.h"
#include "Shader/Common.hlsl"
#include "Shader/CommonDefine.h"

// ライト設定を保持するComponent。
// YAMLとInspector実装はLightComponentOperationsへ分離する。
class LightComponent: public IComponent {
public:
	LIGHT light{};
	bool dirty = false;

	LightComponent(){
		light.Enable = true;
		light.LightType = LIGHT_TYPE_POINT;
		light.CastShadow = true;
		light.Ambient = float4(0.0f, 0.0f, 0.0f, 0.0f);
		light.Diffuse = float4(1.0f, 1.0f, 1.0f, 1.0f);
		light.Param = float4(10.0f, 0.0f, 0.0f, DEPTH_BIAS_CONSTANT);
	}

	YAML::Node encode() override;
	bool decode(SceneContext* context, const YAML::Node& node) override;
	void inspector(SceneContext* context) override;
};

#include "Operations/LightComponentOperations.h"
