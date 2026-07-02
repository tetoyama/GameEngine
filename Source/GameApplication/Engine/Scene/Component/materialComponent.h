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
class MaterialComponent {
public:
	int ShaderID = 0;
	MATERIAL Material{};

	MaterialComponent(){
		Material.BaseColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
	}

	YAML::Node encode();
	bool decode(SceneContext* context, const YAML::Node& node);
	void inspector(SceneContext* context);
};

#include "Operations/MaterialComponentOperations.h"