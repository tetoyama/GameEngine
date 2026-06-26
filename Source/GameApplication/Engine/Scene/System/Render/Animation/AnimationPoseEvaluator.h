#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include <DirectXMath.h>
#include "Resources/Data/modelData.h"

namespace AnimationPoseEvaluator {
inline float Dot(const aiQuaternion& a, const aiQuaternion& b){
	return a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;
}
inline aiQuaternion Rotation(float time, const aiNodeAnim* channel){
	if(!channel || channel->mNumRotationKeys==0) return aiQuaternion();
	if(channel->mNumRotationKeys==1) return channel->mRotationKeys[0].mValue;
	uint32_t index=0;
	while(index+1<channel->mNumRotationKeys && time>static_cast<float>(channel->mRotationKeys[index+1].mTime)) ++index;
	const uint32_t next=index+1;
	if(next>=channel->mNumRotationKeys) return channel->mRotationKeys[index].mValue;
	const float start=static_cast<float>(channel->mRotationKeys[index].mTime);
	const float end=static_cast<float>(channel->mRotationKeys[next].mTime);
	const float factor=end>start ? (time-start)/(end-start) : 0.0f;
	aiQuaternion result;
	aiQuaternion::Interpolate(result,channel->mRotationKeys[index].mValue,channel->mRotationKeys[next].mValue,factor);
	result.Normalize();
	return result;
}
inline aiVector3D Position(float time, const aiNodeAnim* channel){
	if(!channel || channel->mNumPositionKeys==0) return aiVector3D(0,0,0);
	if(channel->mNumPositionKeys==1) return channel->mPositionKeys[0].mValue;
	uint32_t index=0;
	while(index+1<channel->mNumPositionKeys && time>static_cast<float>(channel->mPositionKeys[index+1].mTime)) ++index;
	const uint32_t next=index+1;
	if(next>=channel->mNumPositionKeys) return channel->mPositionKeys[index].mValue;
	const float start=static_cast<float>(channel->mPositionKeys[index].mTime);
	const float end=static_cast<float>(channel->mPositionKeys[next].mTime);
	const float factor=end>start ? (time-start)/(end-start) : 0.0f;
	return channel->mPositionKeys[index].mValue*(1.0f-factor)+channel->mPositionKeys[next].mValue*factor;
}
inline void UpdateMatrices(const ModelData& model,const aiNode* node,const aiMatrix4x4& parent,std::vector<BONE>& bones){
	if(!node) return;
	aiMatrix4x4 childParent=parent;
	const auto found=model.m_BoneIndexMap.find(node->mName.C_Str());
	if(found!=model.m_BoneIndexMap.end() && found->second<bones.size()){
		BONE& bone=bones[found->second];
		if(model.enableRootMotion || node!=model.AiScene->mRootNode){
			childParent=parent*bone.AnimationMatrix;
			bone.WorldMatrix=childParent;
			bone.Matrix=childParent*bone.OffsetMatrix;
		}
	}
	for(uint32_t i=0;i<node->mNumChildren;++i) UpdateMatrices(model,node->mChildren[i],childParent,bones);
}
inline bool Evaluate(const ModelData& model,const std::vector<AnimationBlend>& blends,float frame,std::vector<BONE>& outBones){
	if(!model.AiScene || blends.empty() || model.m_Bones.empty()){
		outBones.clear();
		return false;
	}
	float totalWeight=0.0f;
	for(const auto& blend:blends) if(blend.weight>0.0f) totalWeight+=blend.weight;
	if(totalWeight<=0.0f){ outBones.clear(); return false; }

	outBones=model.m_Bones;
	const size_t count=outBones.size();
	std::vector<aiQuaternion> rotations(count,aiQuaternion(0,0,0,0));
	std::vector<aiVector3D> positions(count,aiVector3D(0,0,0));
	std::vector<bool> hasRotation(count,false);
	for(const auto& blend:blends){
		if(blend.weight<=0.0f) continue;
		const auto animationIt=model.m_Animation.find(blend.name);
		if(animationIt==model.m_Animation.end()) continue;
		const aiAnimation* animation=animationIt->second.Animation;
		if(!animation || animation->mDuration<=0.0) continue;
		const float duration=static_cast<float>(animation->mDuration);
		const float source=frame+blend.animationStartTime;
		const float time=blend.isLoop ? std::fmod(source,duration) : std::clamp(source,0.0f,duration);
		const float weight=blend.weight/totalWeight;
		for(uint32_t c=0;c<animation->mNumChannels;++c){
			const aiNodeAnim* channel=animation->mChannels[c];
			if(!channel) continue;
			const auto boneIt=model.m_BoneIndexMap.find(channel->mNodeName.C_Str());
			if(boneIt==model.m_BoneIndexMap.end() || boneIt->second>=count) continue;
			const size_t index=boneIt->second;
			aiQuaternion rotation=Rotation(time,channel);
			if(hasRotation[index] && Dot(rotations[index],rotation)<0.0f){
				rotation.w=-rotation.w; rotation.x=-rotation.x; rotation.y=-rotation.y; rotation.z=-rotation.z;
			}
			rotations[index].x+=rotation.x*weight;
			rotations[index].y+=rotation.y*weight;
			rotations[index].z+=rotation.z*weight;
			rotations[index].w+=rotation.w*weight;
			hasRotation[index]=true;
			positions[index]+=Position(time,channel)*weight;
		}
	}
	for(size_t i=0;i<count;++i){
		aiQuaternion rotation=rotations[i];
		if(hasRotation[i]) rotation.Normalize(); else rotation=aiQuaternion();
		outBones[i].AnimationMatrix=aiMatrix4x4(aiVector3D(1,1,1),rotation,positions[i]);
	}
	const aiMatrix4x4 root(aiVector3D(1,1,1),aiQuaternion(DirectX::XM_PI,0,0),aiVector3D(0,0,0));
	UpdateMatrices(model,model.AiScene->mRootNode,root,outBones);
	return true;
}

inline bool SkinCPU(const ModelData& model,const std::vector<BONE>& bones,std::vector<std::vector<VERTEX_3D>>& output){
	if(!model.AiScene || !model.m_DeformVertex || bones.empty()){
		output.clear();
		return false;
	}
	output.clear();
	output.resize(model.AiScene->mNumMeshes);
	for(uint32_t meshIndex=0;meshIndex<model.AiScene->mNumMeshes;++meshIndex){
		const aiMesh* mesh=model.AiScene->mMeshes[meshIndex];
		if(!mesh) continue;
		const auto& deformVertices=model.m_DeformVertex[meshIndex];
		if(deformVertices.size()<mesh->mNumVertices) return false;
		auto& vertices=output[meshIndex];
		vertices.resize(mesh->mNumVertices);
		for(uint32_t vertexIndex=0;vertexIndex<mesh->mNumVertices;++vertexIndex){
			const DEFORM_VERTEX& deform=deformVertices[vertexIndex];
			aiVector3D position(0,0,0),normal(0,0,0);
			for(uint32_t influence=0;influence<4;++influence){
				const float weight=deform.BoneWeight[influence];
				const uint32_t boneIndex=deform.BoneIndex[influence];
				if(weight<=0.0f || boneIndex>=bones.size()) continue;
				const aiMatrix4x4& matrix=bones[boneIndex].Matrix;
				aiVector3D p=mesh->mVertices[vertexIndex]; p*=matrix; position+=p*weight;
				if(mesh->mNormals){
					aiMatrix3x3 normalMatrix(matrix); normalMatrix=normalMatrix.Inverse().Transpose();
					aiVector3D n=mesh->mNormals[vertexIndex]; n*=normalMatrix; normal+=n*weight;
				}
			}
			if(normal.SquareLength()>0.0f) normal.Normalize();
			VERTEX_3D& vertex=vertices[vertexIndex];
			vertex.Position={position.x,position.y,position.z};
			vertex.Normal={normal.x,normal.y,normal.z};
			vertex.Tangent=mesh->mTangents ? DirectX::XMFLOAT3{mesh->mTangents[vertexIndex].x,mesh->mTangents[vertexIndex].y,mesh->mTangents[vertexIndex].z} : DirectX::XMFLOAT3{0,1,0};
			vertex.TexCoord=mesh->HasTextureCoords(0) ? DirectX::XMFLOAT2{mesh->mTextureCoords[0][vertexIndex].x,mesh->mTextureCoords[0][vertexIndex].y} : DirectX::XMFLOAT2{0,0};
			vertex.Diffuse={1,1,1,1};
		}
	}
	return true;
}
}
