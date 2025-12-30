// LoadModelFromFile.cpp
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

	auto model = std::make_shared<ModelData>();

	model->FilePath = path;
	model->SetTexture = false;
	model->isBlender = isBlender;
	model->AiScene = aiImportFile(path.c_str(), aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_ConvertToLeftHanded /* | aiProcess_GenBoundingBoxes */);

	if(!model->AiScene){
		model.reset();
		return nullptr;
	}

	model->VertexBuffer.resize(model->AiScene->mNumMeshes);
	model->IndexBuffer.resize(model->AiScene->mNumMeshes);

	//変形後頂点配列生成
	model->m_DeformVertex = new std::vector<DEFORM_VERTEX>[model->AiScene->mNumMeshes];

	//再帰的にボーン生成
	model->CreateBone(model->AiScene->mRootNode);

	DirectX::XMFLOAT3 Min, Max;

	bool hasBones = false;
	for (unsigned int m = 0; m < model->AiScene->mNumMeshes; m++) {
		if (model->AiScene->mMeshes[m]->HasBones()) {
			hasBones = true;
			break;
		}
	}

	for(unsigned int m = 0; m < model->AiScene->mNumMeshes; m++){

		aiMesh* mesh = model->AiScene->mMeshes[m];

		UINT vertexCount = mesh->mNumVertices;

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

			D3D11_BUFFER_DESC bd = {};
			bd.Usage = D3D11_USAGE_DYNAMIC;
			bd.ByteWidth = sizeof(VERTEX_3D) * mesh->mNumVertices;
			bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

			D3D11_SUBRESOURCE_DATA sd = {};
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

			D3D11_BUFFER_DESC bd = {};

			bd.Usage = D3D11_USAGE_DEFAULT;
			bd.ByteWidth = sizeof(unsigned int) * mesh->mNumFaces * 3;
			bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
			bd.CPUAccessFlags = 0;

			D3D11_SUBRESOURCE_DATA sd = {};

			sd.pSysMem = index;

			context->GetDevice()->CreateBuffer(&bd, &sd, &model->IndexBuffer[m]);

			delete[] index;
		}

		//変形後頂点データ初期化
		for (unsigned int v = 0; v < mesh->mNumVertices; v++) {
			DEFORM_VERTEX deformVertex;
			deformVertex.Position = mesh->mVertices[v];
			deformVertex.Normal = mesh->mNormals[v];
			deformVertex.BoneNum = 0;

			for (unsigned int b = 0; b < 4; b++) {
				deformVertex.BoneName[b] = "";
				deformVertex.BoneWeight[b] = 0.0f;
			}

			model->m_DeformVertex[m].push_back(deformVertex);
		}


		//ボーンデータ初期化
		for (unsigned int b = 0; b < mesh->mNumBones; b++) {
			aiBone* bone = mesh->mBones[b];

			model->m_Bone[bone->mName.C_Str()].OffsetMatrix = bone->mOffsetMatrix;

			//変形後頂点にボーンデータ格納
			for (unsigned int w = 0; w < bone->mNumWeights; w++) {
				aiVertexWeight weight = bone->mWeights[w];

				int num = model->m_DeformVertex[m][weight.mVertexId].BoneNum;

				model->m_DeformVertex[m][weight.mVertexId].BoneWeight[num] = weight.mWeight;
				model->m_DeformVertex[m][weight.mVertexId].BoneName[num] = bone->mName.C_Str();
				model->m_DeformVertex[m][weight.mVertexId].BoneNum++;

				assert(model->m_DeformVertex[m][weight.mVertexId].BoneNum <= 4);
			}
		}
	}


	if(model->AiScene->mNumTextures > 0){
		model->SetTexture = true;

		//テクスチャ読み込み(組み込みされているテクスチャのみ)
		for(unsigned int i = 0; i < model->AiScene->mNumTextures; i++){

			aiTexture* aitexture = model->AiScene->mTextures[i];

			ID3D11ShaderResourceView* texture;
			DirectX::TexMetadata metadata{};
			DirectX::ScratchImage image{};
			if(aitexture->pcData == NULL){

			} else{
				LoadFromWICMemory(aitexture->pcData, aitexture->mWidth, DirectX::WIC_FLAGS::WIC_FLAGS_NONE, &metadata, image);
			}
			CreateShaderResourceView(context->GetDevice(), image.GetImages(), image.GetImageCount(), metadata, &texture);
			assert(texture);

			model->m_Texture[aitexture->mFilename.data] = texture;
		}
	} else{
		namespace fs = std::filesystem;

		for(unsigned int i = 0; i < model->AiScene->mNumMaterials; i++){
			aiMaterial* material = model->AiScene->mMaterials[i];

			if(material->GetTextureCount(aiTextureType_DIFFUSE) > 0){
				aiString texPath;
				if(material->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS){
					std::string textureFilePath = texPath.C_Str();

					if(model->m_Texture.find(textureFilePath) == model->m_Texture.end()){
						// modelPathのフォルダパスを取得
						std::string directory;
						size_t pos = path.find_last_of("/\\");
						if(pos != std::string::npos){
							directory = path.substr(0, pos + 1);
						}

						std::string fullTexPath = directory + textureFilePath;
						std::u8string utf8Path = reinterpret_cast<const char8_t*>(fullTexPath.c_str());
						std::filesystem::path texPath{utf8Path};
						std::error_code ec;

						if(fs::exists(texPath,ec)){
							if(ec){
								OutputDebugStringA(("fs::exists error: " + ec.message()).c_str());

							} else{
								ID3D11ShaderResourceView* texture = nullptr;

								DirectX::TexMetadata metadata{};
								DirectX::ScratchImage image{};

								HRESULT hr = DirectX::LoadFromWICFile(
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
				aiString normalTexPath;
				if(material->GetTexture(aiTextureType_NORMALS, 0, &normalTexPath) == AI_SUCCESS){
					std::string normalMapFile = fs::path(normalTexPath.C_Str()).filename().string();

					if(model->m_Texture.find(normalMapFile) == model->m_Texture.end()){
						// modelPathのフォルダパスを取得
						std::string directory;
						size_t pos = path.find_last_of("/\\");
						if(pos != std::string::npos){
							directory = path.substr(0, pos + 1);
						}

						std::string fullTexPath = directory + normalMapFile;

						if(fs::exists(fullTexPath)){
							ID3D11ShaderResourceView* texture = nullptr;

							DirectX::TexMetadata metadata{};
							DirectX::ScratchImage image{};

							HRESULT hr = DirectX::LoadFromWICFile(
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
			std::string animName = animation->mName.C_Str();
			if (animName == "") {
				animName = "Anim_" + std::to_string(i);
			}

			AnimationData animationData;
			animationData.FilePath = model->FilePath;
			animationData.Scene = model->AiScene;
			animationData.isImported = false;
			animationData.Animation = animation;

			model->m_Animation[animName] = animationData;
		}
	}
	return model;
}

template<>
inline void ResourceLoader<ModelData>::SetupLoadFunc(void* contextPtr){
	OutputDebugStringA("SetupLoadFunc ModelData called\n");
	auto context = static_cast<GraphicsContext*>(contextPtr);
	SetLoadFunction([=](const std::string& path, void* argsPtr) -> std::shared_ptr<ModelData>{
		using ArgsTuple = std::tuple<std::decay_t<bool>>;
		bool isBlender = false;
		if(argsPtr){
			auto tup = static_cast<ArgsTuple*>(argsPtr);
			isBlender = std::get<0>(*tup);
		}
		return LoadModelFromFile(path, isBlender, context);
	});
}

