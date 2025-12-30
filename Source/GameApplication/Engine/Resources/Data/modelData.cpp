#include "modelData.h"
#include "Backends/Assimp/material.h"
#include "Backends/Assimp/scene.h"
#include "Backends/Assimp/cimport.h"
#include "Backends/Assimp/postprocess.h"

#include "Graphics/graphicsContext.h"

aiQuaternion InterpolateRotation(
	float time,
	const aiNodeAnim* nodeAnim
){
	if(nodeAnim->mNumRotationKeys == 1)
		return nodeAnim->mRotationKeys[0].mValue;

	uint32_t index = 0;
	while(index + 1 < nodeAnim->mNumRotationKeys &&
		  time > (float)nodeAnim->mRotationKeys[index + 1].mTime){
		index++;
	}

	uint32_t nextIndex = index + 1;
	if(nextIndex >= nodeAnim->mNumRotationKeys)
		return nodeAnim->mRotationKeys[index].mValue;

	float t1 = (float)nodeAnim->mRotationKeys[index].mTime;
	float t2 = (float)nodeAnim->mRotationKeys[nextIndex].mTime;
	float factor = (time - t1) / (t2 - t1);

	aiQuaternion out;
	aiQuaternion::Interpolate(
		out,
		nodeAnim->mRotationKeys[index].mValue,
		nodeAnim->mRotationKeys[nextIndex].mValue,
		factor
	);
	out.Normalize();
	return out;
}
aiVector3D InterpolatePosition(
	float time,
	const aiNodeAnim* nodeAnim
){
	if(nodeAnim->mNumPositionKeys == 1)
		return nodeAnim->mPositionKeys[0].mValue;

	uint32_t index = 0;
	while(index + 1 < nodeAnim->mNumPositionKeys &&
		  time > (float)nodeAnim->mPositionKeys[index + 1].mTime){
		index++;
	}

	uint32_t nextIndex = index + 1;
	if(nextIndex >= nodeAnim->mNumPositionKeys)
		return nodeAnim->mPositionKeys[index].mValue;

	float t1 = (float)nodeAnim->mPositionKeys[index].mTime;
	float t2 = (float)nodeAnim->mPositionKeys[nextIndex].mTime;
	float factor = (time - t1) / (t2 - t1);

	return
		nodeAnim->mPositionKeys[index].mValue * (1.0f - factor) +
		nodeAnim->mPositionKeys[nextIndex].mValue * factor;
}

void ModelData::Release(){
	if(AiScene){
		// メッシュバッファ解放
		for(int i = 0; i < VertexBuffer.size(); i++){
			if(VertexBuffer[i]){
				VertexBuffer[i]->Release();
				VertexBuffer[i] = nullptr;
			}
		}
		for(int i = 0; i < IndexBuffer.size(); i++){
			if(IndexBuffer[i]){
				IndexBuffer[i]->Release();
				IndexBuffer[i] = nullptr;
			}
		}
		// テクスチャ解放
		if(SetTexture){
			for(auto& pair : m_Texture){
				if(pair.second) pair.second->Release();
			}
			m_Texture.clear();
		}

		for (std::pair<std::string, AnimationData> pair : m_Animation) {
			if (pair.second.isImported) {
				aiReleaseImport(pair.second.Scene);
			}
		}

		delete[] m_DeformVertex;
		m_DeformVertex = nullptr;

		aiReleaseImport(AiScene);
		AiScene = nullptr;
	}
}

void ModelData::CreateBone(aiNode* node){
	std::string name = node->mName.C_Str();

	if(m_BoneIndexMap.find(name) == m_BoneIndexMap.end()){
		uint32_t index = (uint32_t)m_Bones.size();
		m_BoneIndexMap[name] = index;
		m_Bones.push_back(BONE{});
	}

	for(uint32_t i = 0; i < node->mNumChildren; i++){
		CreateBone(node->mChildren[i]);
	}
}


void ModelData::UpdateBoneMatrix(aiNode* node, aiMatrix4x4 Matrix) {

	uint32_t index = m_BoneIndexMap[node->mName.C_Str()];
	BONE& bone = m_Bones[index];

	aiMatrix4x4 worldMatrix = Matrix * bone.AnimationMatrix;
	bone.Matrix = worldMatrix * bone.OffsetMatrix;

	for (unsigned int i = 0; i < node->mNumChildren; i++) {

		UpdateBoneMatrix(node->mChildren[i], worldMatrix);
	}
}

void ModelData::LoadAnimation(const char* FileName, const char* Name){
	const aiScene* scene = aiImportFile(FileName, aiProcess_ConvertToLeftHanded);
	assert(scene);

	auto it = m_Animation.find(Name);
	if (it != m_Animation.end()) {
		return;
	}

	AnimationData animationData;
	animationData.FilePath = FileName;
	animationData.Scene = scene;
	animationData.isImported = true;
	animationData.Animation = scene->mAnimations[0];

	m_Animation[Name] = animationData;
	if(scene->mNumAnimations != 0){

		for(unsigned int i = 0; i < scene->mNumAnimations; i++){

			scene = aiImportFile(FileName, aiProcess_ConvertToLeftHanded);

			AnimationData animationData;
			animationData.FilePath = FileName;
			animationData.Scene = scene;
			animationData.isImported = true;
			animationData.Animation = scene->mAnimations[i];
			m_Animation[scene->mAnimations[i]->mName.C_Str()] = animationData;

		}
	}
}

void ModelData::UpdateBoneAnimation(
	const std::vector<AnimationBlend>& anims,
	float frame
){
	if(anims.empty()) return;

	float totalWeight = 0.0f;
	for(const auto& a : anims) totalWeight += a.weight;
	if(totalWeight <= 0.0f) return;

	const size_t boneCount = m_Bones.size();

	// 一時バッファ
	std::vector<aiQuaternion> accumRot(
		boneCount, aiQuaternion(0, 0, 0, 0)
	);
	std::vector<aiVector3D> accumPos(
		boneCount, aiVector3D(0, 0, 0)
	);

	for(const auto& anim : anims){
		if(anim.weight <= 0.0f) continue;

		auto itAnim = m_Animation.find(anim.name);
		if(itAnim == m_Animation.end()) continue;

		aiAnimation* animation = itAnim->second.Animation;
		if(!animation) continue;

		float animTime =
			fmod(frame + anim.animationStartTime,
				 (float)animation->mDuration);

		float w = anim.weight / totalWeight;

		for(unsigned int c = 0; c < animation->mNumChannels; c++){
			aiNodeAnim* nodeAnim = animation->mChannels[c];
			const char* boneName = nodeAnim->mNodeName.C_Str();

			auto itBone = m_BoneIndexMap.find(boneName);
			if(itBone == m_BoneIndexMap.end()) continue;

			uint32_t idx = itBone->second;

			aiQuaternion rot = InterpolateRotation(animTime, nodeAnim);
			aiVector3D  pos = InterpolatePosition(animTime, nodeAnim);

			accumRot[idx].x += rot.x * w;
			accumRot[idx].y += rot.y * w;
			accumRot[idx].z += rot.z * w;
			accumRot[idx].w += rot.w * w;

			accumPos[idx] += pos * w;
		}
	}

	// AnimationMatrix へ反映
	for(size_t i = 0; i < boneCount; i++){
		aiQuaternion r = accumRot[i];
		r.Normalize();

		m_Bones[i].AnimationMatrix =
			aiMatrix4x4(aiVector3D(1, 1, 1), r, accumPos[i]);
	}

	aiMatrix4x4 rootMatrix(
		aiVector3D(1, 1, 1),
		aiQuaternion(DirectX::XM_PI, 0, 0),
		aiVector3D(0, 0, 0)
	);

	UpdateBoneMatrix(AiScene->mRootNode, rootMatrix);
}

void ModelData::CPU_Skinning(
	const std::vector<DEFORM_VERTEX>& deformVertices,
	const aiMesh* mesh,
	VERTEX_3D* outVertex
) const{
	for(unsigned int j = 0; j < mesh->mNumVertices; j++){
		const DEFORM_VERTEX& dv = deformVertices[j];

		aiVector3D blendedPos(0, 0, 0);
		aiVector3D blendedNormal(0, 0, 0);

		for(int k = 0; k < 4; k++){
			float w = dv.BoneWeight[k];
			if(w <= 0.0f) continue;

			const aiMatrix4x4& boneMat =
				m_Bones[dv.BoneIndex[k]].Matrix;

			aiVector3D p = mesh->mVertices[j];
			p *= boneMat;

			aiMatrix3x3 normalMat(boneMat);
			normalMat = normalMat.Inverse().Transpose();

			aiVector3D n = mesh->mNormals[j];
			n *= normalMat;

			blendedPos += p * w;
			blendedNormal += n * w;
		}

		blendedNormal.Normalize();

		outVertex[j].Position =
		{blendedPos.x, blendedPos.y, blendedPos.z};
		outVertex[j].Normal =
		{blendedNormal.x, blendedNormal.y, blendedNormal.z};

		if(mesh->HasTextureCoords(0)){
			outVertex[j].TexCoord =
			{
				mesh->mTextureCoords[0][j].x,
				mesh->mTextureCoords[0][j].y
			};
		} else{
			outVertex[j].TexCoord = {0.0f, 0.0f};
		}

		outVertex[j].Diffuse = {1,1,1,1};
	}
}
