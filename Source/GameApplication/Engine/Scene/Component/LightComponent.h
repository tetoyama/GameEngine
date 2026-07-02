// =======================================================================
//
// LightComponent.h
//
// =======================================================================
#pragma once

#include "Interface/IComponent.h"
#include "LightComponentDefaults.h"

// ライト設定を保持するComponent。
// YAMLとInspector実装はLightComponentOperationsへ分離する。
class LightComponent {
public:
	LIGHT light = LightComponentDefaults::Create();
	bool dirty = false;

	LightComponent() = default;

	YAML::Node encode();
	bool decode(SceneContext* context, const YAML::Node& node);
	void inspector(SceneContext* context);
};

#include "Operations/LightComponentOperations.h"
