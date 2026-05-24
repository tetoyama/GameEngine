// =======================================================================
// 
// modelLoader.h
// 
// =======================================================================
#pragma once
#include "ResourceLoader.h"

#include <memory>
#include <string>
#include <filesystem>

#include "Resources/Data/modelData.h"
#include "Graphics/graphicsContext.h"

#include "Backends/DirectX11/DirectXTex.h"
#include "Backends/Assimp/material.h"
#include "Backends/Assimp/scene.h"
#include "Backends/Assimp/cimport.h"
#include "Backends/Assimp/postprocess.h"
#include "Backends/Assimp/matrix4x4.h"

#pragma comment (lib, "assimp-vc143-mt.lib")

#include <cassert>

inline std::shared_ptr<ModelData> LoadModelFromFile(const std::string& path, bool isBlender, GraphicsContext* context){

	auto m_Model= std::make_shared<ModelData>();

	model->FilePath = path;
	model->SetTexture = false;
	model->isBlender = isBlender;
	model->AiScene = aiImportFile(path.c_str(), aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_ConvertToLeftHanded /* | aiProcess_GenBoundingBoxes */);

	if(!model->AiScene){
		model.reset();
		return m_Nullptr;
	}

	model->VertexBuffer.resize(model->AiScene->mNumMeshes);
	model->IndexBuffer.resize(model->AiScene->mNumMeshes);

	//変形後頂点配列生成
	model->m_DeformVertex = new std::vector<DEFORM_VERTEX>[model->AiScene->mNumMeshes];

	//再帰的にボーン生成
	model->CreateBone(model->AiScene->mRootNode);

	DirectX::XMFLOAT3 Min, m_Max;

	bool m_HasBones= false;
	for (unsigned int m = 0; m < model->AiScene->mNumMeshes; m++) {
		if (model->AiScene->mMeshes[m]->HasBones()) {
			hasBones = true;
			break;
		}
	}

	for(unsigned int m = 0; m < model->AiScene->mNumMeshes; m++){

		aiMesh* mesh = model->AiScene->mMeshes[m];

		UINT m_VertexCount= mesh->mNumVertices;

		// 頂点バッファ生成
		{
			VERTEX_3D* vertex = new VERTEX_3D[mesh->mNumVertices];

			for(unsigned int v = 0; v < mesh->mNumVertices; v++){

				if(v == 0 && m == 0){
					Min = Max = DirectX::XMFLOAT3(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z);
				} else{

					Min.x = (std::min)(mesh->mVertices[v].x, Min.x);
					Min.y = (std::min)(mesh->mVertices[v].y, Min.y);
					Min.z = (std::min)(mesh->mVertices[v].z, Min.z);

					Max.x = (std::max)(mesh->mVertices[v].x, Max.x);
					Max.y = (std::max)(mesh->mVertices[v].y, Max.y);
					Max.z = (std::max)(mesh->mVertices[v].z, Max.z);
				}
				if(mesh->HasPositions()){
					if(isBlender){
						vertex[v].Position = DirectX::XMFLOAT3(mesh->mVertices[v].x, -mesh->mVertices[v].z, mesh->mVertices[v].y);

					} else{
						vertex[v].Position = DirectX::XMFLOAT3(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z);
					}
				} else{
					vertex[v].Position = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
				}
				if(mesh->HasNormals()){
					if(isBlender){
						vertex[v].Normal = DirectX::XMFLOAT3(mesh->mNormals[v].x, -mesh->mNormals[v].z, mesh->mNormals[v].y);
					} else{
						vertex[v].Normal = DirectX::XMFLOAT3(mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z);
					}
				} else{
					vertex[v].Normal = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
				}
				if(mesh->HasTangentsAndBitangents()){
					if(isBlender){
						vertex[v].Tangent = DirectX::XMFLOAT3(mesh->mTangents[v].x, -mesh->mTangents[v].z, mesh->mTangents[v].y);
					} else{
						vertex[v].Tangent = DirectX::XMFLOAT3(mesh->mTangents[v].x, mesh->mTangents[v].y, mesh->mTangents[v].z);
					}
				} else{
					vertex[v].Tangent = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
				}
				if(mesh->HasVertexColors(v)){
					vertex[v].Diffuse = DirectX::XMFLOAT4(mesh->mColors[v]->r, mesh->mColors[v]->g, mesh->mColors[v]->b, mesh->mColors[v]->a);
				} else{
					vertex[v].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
				}

				if(mesh->HasTextureCoords(0)){
					vertex[v].TexCoord = DirectX::XMFLOAT2(mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y);
				} else{
					vertex[v].TexCoord = DirectX::XMFLOAT2(0.0f, 0.0f);
				}


			}

			D3D11_BUFFER_DESC m_Bd= {};
			bd.Usage = D3D11_USAGE_DYNAMIC;
			bd.ByteWidth = sizeof(VERTEX_3D) * mesh->mNumVertices;
			bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

			D3D11_SUBRESOURCE_DATA m_Sd= {};
			sd.pSysMem = vertex;

			context->GetDevice()->CreateBuffer(&bd, &sd, &model->VertexBuffer[m]);

			delete[] vertex;
		}
		if (hasBones) {

			if (mesh->HasBones()) {

			}
		}


		// インデックスバッファ生成
		{
			unsigned int* index = new unsigned int[mesh->mNumFaces * 3];
			//unsigned short* index = new unsigned short[mesh->mNumFaces * 3];

			for (unsigned int f = 0; f < mesh->mNumFaces; f++) {
				const aiFace* face = &mesh->mFaces[f];

				//assert(face->mNumIndices == 3);

				index[f * 3 + 0] = face->mIndices[0];
				index[f * 3 + 1] = face->mIndices[1];
				index[f * 3 + 2] = face->mIndices[2];
			}

			D3D11_BUFFER_DESC m_Bd= {};

			bd.Usage = D3D11_USAGE_DEFAULT;
			bd.ByteWidth = sizeof(unsigned int) * mesh->mNumFaces * 3;
			bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
			bd.CPUAccessFlags = 0;

			D3D11_SUBRESOURCE_DATA m_Sd= {};

			sd.pSysMem = index;

			context->GetDevice()->CreateBuffer(&bd, &sd, &model->IndexBuffer[m]);

			delete[] index;
		}

		//変形後頂点データ初期化
		for (unsigned int v = 0; v < mesh->mNumVertices; v++) {
			DEFORM_VERTEX m_DeformVertex;
			deformVertex.Position = mesh->mVertices[v];
			deformVertex.Normal = mesh->mNormals[v];

			for (unsigned int b = 0; b < 4; b++) {
				deformVertex.BoneIndex[b] = 0;
				deformVertex.BoneWeight[b] = 0.0f;
			}

			model->m_DeformVertex[m].push_back(deformVertex);
		}


		//ボーンデータ初期化
		for(unsigned int b = 0; b < mesh->mNumBones; b++){

			aiBone* bone = mesh->mBones[b];
			const std::string m_BoneName= bone->mName.C_Str();

			// Model全体のBoneIndexを取得
			auto m_It= model->m_BoneIndexMap.find(boneName);
			if(it == model->m_BoneIndexMap.end()){
				continue; // Node階層に存在しないボーンは無視
			}

			const uint32_t m_BoneIndex= it->second;

			// OffsetMatrix 設定
			model->m_Bones[boneIndex].OffsetMatrix = bone->mOffsetMatrix;

			// Weight 登録
			for(unsigned int w = 0; w < bone->mNumWeights; w++){

				const aiVertexWeight& vw = bone->mWeights[w];
				DEFORM_VERTEX& dv = model->m_DeformVertex[m][vw.mVertexId];

				// 空いているスロットに入れる（最大4）
				for(uint32_t i = 0; i < 4; i++){
					if(dv.BoneWeight[i] == 0.0f){
						dv.BoneIndex[i] = boneIndex;
						dv.BoneWeight[i] = vw.mWeight;
						break;
					}
				}
			}
		}

	}


	if(model->AiScene->mNumTextures > 0){
		model->SetTexture = true;

		//テクスチャ読み込み(組み込みされているテクスチャのみ)
		for(unsigned int i = 0; i < model->AiScene->mNumTextures; i++){

			aiTexture* aitexture = model->AiScene->mTextures[i];

			ID3D11ShaderResourceView* texture;
			DirectX::TexMetadata m_Metadata{};
			DirectX::ScratchImage m_Image{};
			if(aitexture->pcData == NULL){

			} else{
				LoadFromWICMemory(aitexture->pcData, aitexture->mWidth, DirectX::WIC_FLAGS::WIC_FLAGS_NONE, &metadata, image);
			}
			CreateShaderResourceView(context->GetDevice(), image.GetImages(), image.GetImageCount(), metadata, &texture);
			assert(texture);

			model->m_Texture[aitexture->mFilename.data] = texture;
		}
	} else{
		namespace m_Fs= std::filesystem;

		for(unsigned int i = 0; i < model->AiScene->mNumMaterials; i++){
			aiMaterial* material = model->AiScene->mMaterials[i];

			if(material->GetTextureCount(aiTextureType_DIFFUSE) > 0){
				aiString m_TexPath;
				if(material->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS){
					std::string m_TextureFilePath= texPath.C_Str();

					if(model->m_Texture.find(textureFilePath) == model->m_Texture.end()){
						// modelPathのフォルダパスを取得
						std::string m_Directory;
						size_t m_Pos= path.find_last_of("/\\");
						if(pos != std::string::npos){
							directory = path.substr(0, pos + 1);
						}

						std::string m_FullTexPath= directory + textureFilePath;
						std::u8string m_Utf8Path= reinterpret_cast<const char8_t*>(fullTexPath.c_str());
						std::filesystem::path m_TexPath{utf8Path};
						std::error_code m_Ec;

						if(fs::exists(texPath,ec)){
							if(ec){
								OutputDebugStringA(("fs::exists error: " + ec.message()).c_str());

							} else{
								ID3D11ShaderResourceView* texture = nullptr;

								DirectX::TexMetadata m_Metadata{};
								DirectX::ScratchImage m_Image{};

								HRESULT m_Hr= DirectX::LoadFromWICFile(
									std::wstring(fullTexPath.begin(), fullTexPath.end()).c_str(),
									DirectX::WIC_FLAGS_NONE,
									&metadata,
									image);

								if(SUCCEEDED(hr)){
									hr = DirectX::CreateShaderResourceView(
										context->GetDevice(),
										image.GetImages(),
										image.GetImageCount(),
										metadata,
										&texture);

									if(SUCCEEDED(hr)){
										model->m_Texture[textureFilePath] = texture;
										model->SetTexture = true;

									} else{
										OutputDebugStringA(("Failed to create shader resource view for texture: " + fullTexPath + "\n").c_str());
									}
								} else{
									OutputDebugStringA(("Failed to load WIC texture file: " + fullTexPath + "\n").c_str());
								}
							}
						} else{
							OutputDebugStringA(("Texture file not found: " + fullTexPath + "\n").c_str());
						}

					}
				}
			}

			// NormalMapテクスチャ
			if(material->GetTextureCount(aiTextureType_NORMALS) > 0){
				aiString m_NormalTexPath;
				if(material->GetTexture(aiTextureType_NORMALS, 0, &normalTexPath) == AI_SUCCESS){
					std::string m_NormalMapFile= fs::path(normalTexPath.C_Str()).filename().string();

					if(model->m_Texture.find(normalMapFile) == model->m_Texture.end()){
						// modelPathのフォルダパスを取得
						std::string m_Directory;
						size_t m_Pos= path.find_last_of("/\\");
						if(pos != std::string::npos){
							directory = path.substr(0, pos + 1);
						}

						std::string m_FullTexPath= directory + normalMapFile;

						if(fs::exists(fullTexPath)){
							ID3D11ShaderResourceView* texture = nullptr;

							DirectX::TexMetadata m_Metadata{};
							DirectX::ScratchImage m_Image{};

							HRESULT m_Hr= DirectX::LoadFromWICFile(
								std::wstring(fullTexPath.begin(), fullTexPath.end()).c_str(),
								DirectX::WIC_FLAGS_NONE,
								&metadata,
								image);

							if(SUCCEEDED(hr)){
								hr = DirectX::CreateShaderResourceView(
									context->GetDevice(),
									image.GetImages(),
									image.GetImageCount(),
									metadata,
									&texture);

								if(SUCCEEDED(hr)){
									model->m_Texture[normalMapFile] = texture;
									model->SetTexture = true;

								} else{
									OutputDebugStringA(("Failed to create shader resource view for texture: " + fullTexPath + "\n").c_str());
								}
							} else{
								OutputDebugStringA(("Failed to load WIC texture file: " + fullTexPath + "\n").c_str());
							}
						} else{
							OutputDebugStringA(("Texture file not found: " + fullTexPath + "\n").c_str());
						}

					}
				}
			}
		}
	}
	if (hasBones && model->AiScene->HasAnimations()) {
		for (unsigned int i = 0; i < model->AiScene->mNumAnimations; i++) {

			aiAnimation* animation = model->AiScene->mAnimations[i];
			std::string m_AnimName= animation->mName.C_Str();
			if (animName == "") {
				animName = "Anim_" + std::to_string(i);
			}

			AnimationData m_AnimationData;
			animationData.FilePath = model->FilePath;
			animationData.Scene = model->AiScene;
			animationData.isImported = false;
			animationData.Animation = animation;

			model->m_Animation[animName] = animationData;
		}

		model->CreateSkinningBuffers(context);
	}
	return m_Model;
}
template<>
inline void ResourceLoader<ModelData>::SetupLoadFunc(void* contextPtr) {
	OutputDebugStringA("SetupLoadFunc ModelData called\n");
	auto m_Context= static_cast<GraphicsContext*>(contextPtr);

	SetLoadFunction([=](const std::string& path, std::shared_ptr<void> argsPtr) -> std::shared_ptr<ModelData> {
		bool m_IsBlender= false;

		if (argsPtr) {
			using ArgsTuple = std::tuple<std::decay_t<bool>>;
			auto m_Tup= static_cast<ArgsTuple*>(argsPtr.get());
			isBlender = std::get<0>(*tup);
		}

		return LoadModelFromFile(path, isBlender, context);
	});
}

