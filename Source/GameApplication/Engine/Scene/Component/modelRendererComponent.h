// =======================================================================
//
// modelRendererComponent.h
//
// =======================================================================
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <d3d11.h>

#include "Interface/IComponent.h"
#include "Resources/Data/modelData.h"

// モデル設定、Entity固有Animation状態、動的VB参照を保持するComponent。
// ModelDataはResourceServiceで共有されるため、可変なBone PoseをResourceへ保存しない。
class ModelRendererComponent: public IComponent {
public:
	std::shared_ptr<ModelData> model;
	std::string modelFilePath;
	std::vector<std::pair<std::string, std::string>> animations;
	std::vector<AnimationBlend> blendedAnimations;
	std::vector<ID3D11Buffer*> dynamicVertexBuffers;
	bool isBlender = false;
	float animationTime = 0.0f;

	// Worker Threadで計算し、Main ThreadのGPU Uploadで消費するEntity固有状態。
	std::vector<BONE> evaluatedBones;
	std::vector<std::vector<VERTEX_3D>> cpuSkinnedVertices;
	uint64_t animationPoseRevision = 0;
	bool animationPoseReady = false;
	bool cpuSkinningReady = false;

	ModelRendererComponent() = default;
	~ModelRendererComponent() override;

	YAML::Node encode() override;
	bool decode(SceneContext* context, const YAML::Node& node) override;
	void inspector(SceneContext* context) override;

	void CreateModel(SceneContext* context);
	void ReleaseBuffers();
	void ResetAnimationRuntime();
};

#include "Operations/ModelRendererRuntime.h"
#include "Operations/ModelRendererSerialization.h"
#include "Operations/ModelRendererInspector.h"
