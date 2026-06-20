// =======================================================================
//
// modelRendererComponent.h
//
// =======================================================================
#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <d3d11.h>

#include "Interface/IComponent.h"
#include "Resources/Data/modelData.h"

// モデル設定、Animation状態、移行期間中の動的VB参照を保持するComponent。
// Runtime・YAML・Inspector実装はOperations配下へ分離する。
class ModelRendererComponent: public IComponent {
public:
	std::shared_ptr<ModelData> model;
	std::string modelFilePath;
	std::vector<std::pair<std::string, std::string>> animations;
	std::vector<AnimationBlend> blendedAnimations;
	std::vector<ID3D11Buffer*> dynamicVertexBuffers;
	bool isBlender = false;
	float animationTime = 0.0f;

	ModelRendererComponent() = default;
	~ModelRendererComponent() override;

	YAML::Node encode() override;
	bool decode(SceneContext* context, const YAML::Node& node) override;
	void inspector(SceneContext* context) override;

	void CreateModel(SceneContext* context);
	void ReleaseBuffers();
};

#include "Operations/ModelRendererRuntime.h"
#include "Operations/ModelRendererSerialization.h"
#include "Operations/ModelRendererInspector.h"
