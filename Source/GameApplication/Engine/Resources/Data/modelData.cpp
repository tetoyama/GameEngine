#include "modelData.h"
#include "Backends/Assimp/material.h"
#include "Backends/Assimp/scene.h"
#include "Backends/Assimp/cimport.h"
#include "Backends/Assimp/postprocess.h"

#include "Graphics/graphicsContext.h"

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

void ModelData::CreateBone(aiNode* node) {

	BONE bone;

	m_Bone[node->mName.C_Str()] = bone;

	for (unsigned int n = 0; n < node->mNumChildren; n++) {
		CreateBone(node->mChildren[n]);
	}

}

void ModelData::UpdateBoneMatrix(aiNode* node, aiMatrix4x4 Matrix) {
	BONE* bone = &m_Bone[node->mName.C_Str()];

	aiMatrix4x4 worldMatrix = Matrix * bone->AnimationMatrix;

	bone->Matrix = worldMatrix * bone->OffsetMatrix;

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
	for(const auto& anim : anims){
		totalWeight += anim.weight;
	}
	if(totalWeight <= 0.0f) return;

	// 各ボーンの AnimationMatrix 初期化
	for(auto& pair : m_Bone){
		pair.second.AnimationMatrix = aiMatrix4x4();
	}

	for(auto& pair : m_Bone){
		const std::string& boneName = pair.first;
		BONE& bone = pair.second;

		aiQuaternion blendedRot(0, 0, 0, 0);
		aiVector3D blendedPos(0, 0, 0);

		for(const auto& anim : anims){
			if(anim.weight <= 0.0f) continue;

			auto it = m_Animation.find(anim.name);
			if(it == m_Animation.end()) continue;

			aiAnimation* animation = it->second.Animation;
			if(!animation) continue;

			aiNodeAnim* nodeAnim = nullptr;
			for(unsigned int c = 0; c < animation->mNumChannels; c++){
				if(animation->mChannels[c]->mNodeName == aiString(boneName)){
					nodeAnim = animation->mChannels[c];
					break;
				}
			}
			if(!nodeAnim) continue;

			int rotIdx = (int)frame % nodeAnim->mNumRotationKeys;
			int posIdx = (int)frame % nodeAnim->mNumPositionKeys;

			aiQuaternion rot = nodeAnim->mRotationKeys[rotIdx].mValue;
			aiVector3D pos = nodeAnim->mPositionKeys[posIdx].mValue;

			float w = anim.weight / totalWeight;

			// ---- 回転 ----
			blendedRot.x += rot.x * w;
			blendedRot.y += rot.y * w;
			blendedRot.z += rot.z * w;
			blendedRot.w += rot.w * w;

			// 位置：線形加算
			blendedPos += pos * w;
		}

		blendedRot.Normalize();

		bone.AnimationMatrix =
			aiMatrix4x4(aiVector3D(1, 1, 1), blendedRot, blendedPos);
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
				m_Bone.at(dv.BoneName[k]).Matrix;

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
