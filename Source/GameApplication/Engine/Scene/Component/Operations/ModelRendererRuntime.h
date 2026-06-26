#pragma once
#include <cstdint>
#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Graphics/graphicsContext.h"
#include "Resources/resourceService.h"
#include "Resources/Data/modelData.h"
#include "System/Render/Animation/AnimationPoseEvaluator.h"
#include "System/Render/Animation/AnimationSkinningUpload.h"

namespace ModelRendererRuntime {
inline void ResetAnimationRuntime(ModelRendererComponent& component){
	component.evaluatedBones.clear();
	component.cpuSkinnedVertices.clear();
	component.animationPoseRevision = 0;
	component.animationPoseSourceModelRevision = 0;
	component.animationUploadFailureCount = 0;
	component.animationPoseReady = false;
	component.cpuSkinningReady = false;

	// 0は未設定値として使うため、周回時も有効世代へ進める。
	++component.modelRuntimeRevision;
	if(component.modelRuntimeRevision == 0){
		++component.modelRuntimeRevision;
	}
}

inline void ReleaseBuffers(ModelRendererComponent& component){
	for(ID3D11Buffer*& buffer : component.dynamicVertexBuffers){
		if(buffer){
			buffer->Release();
			buffer = nullptr;
		}
	}
	component.dynamicVertexBuffers.clear();
}

inline bool CreateModel(ModelRendererComponent& component, SceneContext* context){
	if(component.modelFilePath.empty() || !context || !context->manager ||
		!context->manager->resource || !context->manager->graphics){
		return false;
	}

	ReleaseBuffers(component);
	ResetAnimationRuntime(component);
	component.model.reset();
	component.model = context->manager->resource->Load<ModelData>(
		component.modelFilePath,
		component.isBlender
	);
	if(!component.model || !component.model->AiScene) return false;

	for(const auto& [animationName, animationPath] : component.animations){
		component.model->LoadAnimationSource(
			animationPath.c_str(),
			animationName.c_str()
		);
	}

	ID3D11Device* device = context->manager->graphics->GetDevice();
	if(!device) return false;

	component.dynamicVertexBuffers.assign(component.model->AiScene->mNumMeshes, nullptr);
	bool allBuffersCreated = true;
	for(uint32_t meshIndex = 0; meshIndex < component.model->AiScene->mNumMeshes; ++meshIndex){
		const aiMesh* mesh = component.model->AiScene->mMeshes[meshIndex];
		if(!mesh || mesh->mNumVertices == 0){
			allBuffersCreated = false;
			continue;
		}

		D3D11_BUFFER_DESC description{};
		description.Usage = D3D11_USAGE_DYNAMIC;
		description.ByteWidth = static_cast<UINT>(sizeof(VERTEX_3D) * mesh->mNumVertices);
		description.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		if(FAILED(device->CreateBuffer(&description, nullptr, &component.dynamicVertexBuffers[meshIndex]))){
			allBuffersCreated = false;
		}
	}
	return allBuffersCreated;
}

inline void ResetModel(ModelRendererComponent& component){
	ReleaseBuffers(component);
	ResetAnimationRuntime(component);
	component.model.reset();
	component.blendedAnimations.clear();
}
}

inline ModelRendererComponent::~ModelRendererComponent(){
	ModelRendererRuntime::ReleaseBuffers(*this);
	ModelRendererRuntime::ResetAnimationRuntime(*this);
	model.reset();
}
inline void ModelRendererComponent::ReleaseBuffers(){ ModelRendererRuntime::ReleaseBuffers(*this); }
inline void ModelRendererComponent::ResetAnimationRuntime(){ ModelRendererRuntime::ResetAnimationRuntime(*this); }
inline void ModelRendererComponent::CreateModel(SceneContext* context){ ModelRendererRuntime::CreateModel(*this, context); }
