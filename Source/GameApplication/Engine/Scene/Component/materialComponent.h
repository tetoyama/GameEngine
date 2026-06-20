// =======================================================================
//
// materialComponent.h
//
// =======================================================================
#pragma once

#include "Interface/IComponent.h"
#include "Shader/Common.hlsl"
#include "Shader/CommonDefine.h"

// Shader選択とPBRパラメータだけを保持するComponent。
// YAML・Inspector実装はMaterialComponentOperationsへ分離する。
class MaterialComponent: public IComponent {
public:
	int ShaderID = 0;
	MATERIAL Material{};

	MaterialComponent(){
		Material.BaseColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
	}

	YAML::Node encode() override;
	bool decode(SceneContext* context, const YAML::Node& node) override;
	void inspector(SceneContext* context) override;
};

#include "Operations/MaterialComponentOperations.h"
