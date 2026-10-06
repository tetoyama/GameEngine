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
#include "ModelGeometryFile.h"
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
	model->AiScene = ModelGeometryImport::ImportScene(path).release();

	if(!model->AiScene){
		model.reset();
		return nullptr;
	}

	model->MeshGeometry.resize(model->AiScene->mNumMeshes);
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
		ModelMeshGeometryCpuData& geometry = model->MeshGeometry[m];

        if(!ModelGeometryImport::ExtractMesh(*mesh,isBlender,geometry)) return nullptr;
        for(const auto& p:geometry.vertices) {
            if(m==0 && &p==geometry.vertices.data()) { Min={p.Position.x,p.Position.y,p.Position.z}; Max=Min; }
            else {
                Min.x=(std::min)(p.Position.x,Min.x); Min.y=(std::min)(p.Position.y,Min.y); Min.z=(std::min)(p.Position.z,Min.z);
                Max.x=(std::max)(p.Position.x,Max.x); Max.y=(std::max)(p.Position.y,Max.y); Max.z=(std::max)(p.Position.z,Max.z);
            }
        }
        // Native buffers use exactly the CPU snapshot shared with other backends.
        {
			D3D11_BUFFER_DESC bd = {};
			bd.Usage = D3D11_USAGE_DYNAMIC;
			bd.ByteWidth = sizeof(VERTEX_3D) * mesh->mNumVertices;
			bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

			D3D11_SUBRESOURCE_DATA sd = {};
			sd.pSysMem = geometry.vertices.data();

			context->GetDevice()->CreateBuffer(&bd, &sd, &model->VertexBuffer[m]);
		}
		if (hasBones) {

			if (mesh->HasBones()) {

			}
		}


		// Backend非依存CPU Indexを生成し、既存D3D11 Bufferと将来のRHI Runtimeで共有する。
		{
			D3D11_BUFFER_DESC bd = {};

			bd.Usage = D3D11_USAGE_DEFAULT;
			bd.ByteWidth = sizeof(std::uint32_t) * mesh->mNumFaces * 3;
			bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
			bd.CPUAccessFlags = 0;

			D3D11_SUBRESOURCE_DATA sd = {};

			sd.pSysMem = geometry.indices.data();

			context->GetDevice()->CreateBuffer(&bd, &sd, &model->IndexBuffer[m]);
		}

		//変形後頂点データ初期化
		for (unsigned int v = 0; v < mesh->mNumVertices; v++) {
			DEFORM_VERTEX deformVertex;
			deformVertex.Position = mesh->mVertices[v];
			deformVertex.Normal = mesh->HasNormals() ? mesh->mNormals[v] : aiVector3D{1,1,1};

			for (unsigned int b = 0; b < 4; b++) {
				deformVertex.BoneIndex[b] = 0;
				deformVertex.BoneWeight[b] = 0.0f;
			}

			model->m_DeformVertex[m].push_back(deformVertex);
		}


		//ボーンデータ初期化
		for(unsigned int b = 0; b < mesh->mNumBones; b++){

			aiBone* bone = mesh->mBones[b];
			const std::string boneName = bone->mName.C_Str();

			// Model全体のBoneIndexを取得
			auto it = model->m_BoneIndexMap.find(boneName);
			if(it == model->m_BoneIndexMap.end()){
				continue; // Node階層に存在しないボーンは無視
			}

			const uint32_t boneIndex = it->second;

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

		model->CreateSkinningBuffers(context);
	}
	return model;
}
template<>
inline void ResourceLoader<ModelData>::SetupLoadFunc(void* contextPtr) {
	OutputDebugStringA("SetupLoadFunc ModelData called\n");
	auto context = static_cast<GraphicsContext*>(contextPtr);

	SetLoadFunction([=](const std::string& path, std::shared_ptr<void> argsPtr) -> std::shared_ptr<ModelData> {
		bool isBlender = false;

		if (argsPtr) {
			using ArgsTuple = std::tuple<std::decay_t<bool>>;
			auto tup = static_cast<ArgsTuple*>(argsPtr.get());
			isBlender = std::get<0>(*tup);
		}

		return LoadModelFromFile(path, isBlender, context);
	});
}
