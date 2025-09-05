#include "modelData.h"
#include "Backends/Assimp/material.h"
#include "Backends/Assimp/scene.h"
#include "Backends/Assimp/cimport.h"
#include "Backends/Assimp/postprocess.h"

#include "GameApplication/Engine/Graphics/graphicsContext.h"


void ModelData::Release(){
	if(AiScene){
		// メッシュバッファ解放
		if(VertexBuffer){
			for(UINT m = 0; m < AiScene->mNumMeshes; ++m){
				if(VertexBuffer[m]) VertexBuffer[m]->Release();
			}
			delete[] VertexBuffer;
			VertexBuffer = nullptr;
		}

		if(IndexBuffer){
			for(UINT m = 0; m < AiScene->mNumMeshes; ++m){
				if(IndexBuffer[m]) IndexBuffer[m]->Release();
			}
			delete[] IndexBuffer;
			IndexBuffer = nullptr;
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

void ModelData::LoadAnimation(const char* FileName, const char* Name) {
	const aiScene* scene = aiImportFile(FileName, aiProcess_ConvertToLeftHanded);
	assert(scene);

	AnimationData animationData;
	animationData.FilePath = FileName;
	animationData.Scene = scene;
	animationData.isImported = true;
	animationData.Animation = scene->mAnimations[0];

	m_Animation[Name] = animationData;
}

void ModelData::Update(const char* AnimationName1, int Frame1, GraphicsContext* pGraphicContext) {
	if (m_Animation.count(AnimationName1) == 0) {
		return;
	}
	if (!m_Animation[AnimationName1].Animation) {
		return;
	}
	//アニメーションデータからボーンマトリクス算出
	aiAnimation* animation1 = m_Animation[AnimationName1].Animation;

	for (auto pair : m_Bone) {
		BONE* bone = &m_Bone[pair.first];

		aiNodeAnim* nodeAnim = nullptr;

		for (unsigned int n = 0; n < animation1->mNumChannels; n++) {

			if (animation1->mChannels[n]->mNodeName == aiString(pair.first)) {
				nodeAnim = animation1->mChannels[n];
				break;
			}
		}
			aiQuaternion rotation;
			aiVector3D position;
			int f;

			if (nodeAnim) {
				f=  Frame1 % nodeAnim->mNumRotationKeys;
				rotation = nodeAnim->mRotationKeys[f].mValue;
				
				f = Frame1 % nodeAnim->mNumPositionKeys;
				position = nodeAnim->mPositionKeys[f].mValue;
			}
			bone->AnimationMatrix = aiMatrix4x4(aiVector3D(1.0f, 1.0f, 1.0f), rotation, position);

		
	}
	//再帰的にボーンマトリクスを更新
	aiMatrix4x4 rootMatrix = aiMatrix4x4(aiVector3D(1.0f,1.0f,1.0f),aiQuaternion(DirectX::XM_PI,0.0f,0.0f),aiVector3D(0.0f,0.0f,0.0f));
	UpdateBoneMatrix(AiScene->mRootNode, rootMatrix);


	//頂点変換（CPUスキニング）
	for (unsigned int i = 0; i < AiScene->mNumMeshes; i++) {
		aiMesh* mesh = AiScene->mMeshes[i];

		D3D11_MAPPED_SUBRESOURCE mappedResource;
		pGraphicContext->GetDeviceContext()->Map(VertexBuffer[i], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);

		VERTEX_3D* vertex = (VERTEX_3D*)mappedResource.pData;

		for (unsigned int j = 0; j < mesh->mNumVertices; j++) {

			DEFORM_VERTEX* deformVertex = &m_DeformVertex[i][j];

			aiMatrix4x4 matrix[4];

			for (int k = 0; k < 4; k++) {
				matrix[k] = m_Bone[deformVertex->BoneName[k]].Matrix;
			}


			aiMatrix4x4 outMatrix;

			outMatrix = matrix[0] * deformVertex->BoneWeight[0]
				+ matrix[1] * deformVertex->BoneWeight[1]
				+ matrix[2] * deformVertex->BoneWeight[2]
				+ matrix[3] * deformVertex->BoneWeight[3];

			deformVertex->Position = mesh->mVertices[j];
			deformVertex->Position *= outMatrix;

			//法線変換用に移動成分を削除
			outMatrix.a4 = 0.0f; outMatrix.b4 = 0.0f; outMatrix.c4 = 0.0f;

			deformVertex->Normal = mesh->mNormals[j];
			deformVertex->Normal *= outMatrix;


			//頂点バッファに書き込み
			vertex[j].Position = DirectX::XMFLOAT3(deformVertex->Position.x, deformVertex->Position.y, deformVertex->Position.z);
			vertex[j].Normal = DirectX::XMFLOAT3(deformVertex->Normal.x, deformVertex->Normal.y, deformVertex->Normal.z);

			vertex[j].TexCoord = DirectX::XMFLOAT2(mesh->mTextureCoords[0][j].x, mesh->mTextureCoords[0][j].y);
			vertex[j].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0, 1.0f);

		}

		pGraphicContext->GetDeviceContext()->Unmap(VertexBuffer[i], 0);
	}




}
