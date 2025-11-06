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

		for(int i = 0; i < scene->mNumAnimations; i++){

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

void ModelData::UpdateSingleAnimation(const char* AnimationName1, int Frame1, GraphicsContext* pGraphicContext) {
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
			f = Frame1 % nodeAnim->mNumRotationKeys;
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

void ModelData::Update(float Frame, GraphicsContext* pGraphicContext) {

	if (blendedAnimations.size() == 0) {
		return;
	} else {
		if (blendedAnimations.size() == 1) {
			UpdateSingleAnimation(blendedAnimations[0].name.c_str(), (int)(Frame - blendedAnimations[0].animationStartTime), pGraphicContext);
			return;
		}
	}

	// 全アニメーションの重み合計を先に計算
	float totalWeight = 0.0f;
	for (auto& Animation : blendedAnimations) {
		totalWeight += Animation.weight;
	}
	if (totalWeight <= 0.0f) return; // 重みゼロは処理しない

	// ボーンごとの最終的な回転・位置を初期化
	for (auto& pair : m_Bone) {
		pair.second.AnimationMatrix = aiMatrix4x4();  // 初期化
	}

	// ボーンごとに処理
	for (auto& pair : m_Bone) {
		std::string boneName = pair.first;
		BONE* bone = &pair.second;

		aiQuaternion blendedRotation;
		aiVector3D blendedPosition(0, 0, 0);
		bool first = true;

		// 各アニメーションを重み付きでブレンド
		for (auto& Animation : blendedAnimations) {

			if (!m_Animation[Animation.name].Animation) {
				continue; // 無効ならスキップ
			}

			aiAnimation* animation = m_Animation[Animation.name].Animation;

			BONE* bone = &m_Bone[pair.first];

			aiNodeAnim* nodeAnim = nullptr;

			for (unsigned int n = 0; n < animation->mNumChannels; n++) {

				if (animation->mChannels[n]->mNodeName == aiString(pair.first)) {
					nodeAnim = animation->mChannels[n];
					break;
				}
			}
			aiQuaternion rotation;
			aiVector3D position;
			int f;

			if (nodeAnim) {
				f = (int)(Frame - Animation.animationStartTime) % nodeAnim->mNumRotationKeys;
				rotation = nodeAnim->mRotationKeys[f].mValue;

				f = (int)(Frame - Animation.animationStartTime) % nodeAnim->mNumPositionKeys;
				position = nodeAnim->mPositionKeys[f].mValue;
			}

			float weightNorm = Animation.weight / totalWeight;
			if (Animation.weight > 0.0f) {
				if (first) {
					blendedRotation = rotation;
					blendedPosition = position * weightNorm;
					first = false;
				} else {
					// aiQuaternion::Interpolate(出力, 開始, 終了, 補間率)
					// 複数の補間は段階的に行う
					aiQuaternion newRot;
					aiQuaternion::Interpolate(newRot, blendedRotation, rotation, weightNorm);
					blendedRotation = newRot;
					blendedRotation.Normalize();

					blendedPosition += position * weightNorm;
				}
			}
		}

		// 結果をマトリクスにセット
		bone->AnimationMatrix = aiMatrix4x4(aiVector3D(1, 1, 1), blendedRotation, blendedPosition);
	}

	// 再帰的にボーンマトリクスを更新
	aiMatrix4x4 rootMatrix = aiMatrix4x4(aiVector3D(1, 1, 1), aiQuaternion(DirectX::XM_PI, 0, 0), aiVector3D(0, 0, 0));
	UpdateBoneMatrix(AiScene->mRootNode, rootMatrix);

	// CPUスキニング安全版
	for(unsigned int i = 0; i < AiScene->mNumMeshes; i++){
		aiMesh* mesh = AiScene->mMeshes[i];

		D3D11_MAPPED_SUBRESOURCE mappedResource;
		pGraphicContext->GetDeviceContext()->Map(VertexBuffer[i], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		VERTEX_3D* vertex = (VERTEX_3D*)mappedResource.pData;

		for(unsigned int j = 0; j < mesh->mNumVertices; j++){
			DEFORM_VERTEX* deformVertex = &m_DeformVertex[i][j];

			aiVector3D blendedPos(0.0f, 0.0f, 0.0f);
			aiVector3D blendedNormal(0.0f, 0.0f, 0.0f);

			// 各ボーンの影響を適用
			for(int k = 0; k < 4; k++){
				float w = deformVertex->BoneWeight[k];
				if(w <= 0.0f) continue;

				const aiMatrix4x4& boneMat = m_Bone[deformVertex->BoneName[k]].Matrix;

				// 頂点座標変換
				aiVector3D p = mesh->mVertices[j];
				p *= boneMat;

				// 法線変換用（逆転置行列を使用）
				aiMatrix3x3 normalMat = aiMatrix3x3(boneMat);
				normalMat = normalMat.Inverse().Transpose();

				aiVector3D n = mesh->mNormals[j];
				n *= normalMat;

				blendedPos += p * w;
				blendedNormal += n * w;
			}

			blendedNormal.Normalize();

			vertex[j].Position = DirectX::XMFLOAT3(blendedPos.x, blendedPos.y, blendedPos.z);
			vertex[j].Normal = DirectX::XMFLOAT3(blendedNormal.x, blendedNormal.y, blendedNormal.z);

			if(mesh->HasTextureCoords(0)){
				vertex[j].TexCoord = DirectX::XMFLOAT2(
					mesh->mTextureCoords[0][j].x,
					mesh->mTextureCoords[0][j].y
				);
			} else{
				vertex[j].TexCoord = DirectX::XMFLOAT2(0.0f, 0.0f);
			}

			vertex[j].Diffuse = DirectX::XMFLOAT4(1, 1, 1, 1);
		}

		pGraphicContext->GetDeviceContext()->Unmap(VertexBuffer[i], 0);
	}

}
