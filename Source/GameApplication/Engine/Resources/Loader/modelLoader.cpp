#include "modelLoader.h"

#include "Backends/DirectX11/DirectXTex.h"

#include "Backends/Assimp/material.h"
#include "Backends/Assimp/scene.h"
#include "Backends/Assimp/cimport.h"
#include "Backends/Assimp/postprocess.h"
#include "Backends/Assimp/matrix4x4.h"

#include "Engine/Graphics/graphicsContext.h"

#include "Engine/Resources/Data/modelData.h"

#include <filesystem>

#pragma comment (lib, "assimp-vc143-mt.lib")

ModelLoader::~ModelLoader() {
	for (auto& m : m_Models) {

		m.second->Release();
		m.second.reset();
	}
}

ModelData* ModelLoader::LoadModel(const std::string& modelPath, const bool& isBlender){

	if(m_Models.count(modelPath) && m_Models[modelPath]->isBlender == isBlender){
		return m_Models[modelPath].get();
	}
	if (m_Models[modelPath]) {
		m_Models[modelPath]->Release();
		m_Models[modelPath].reset();
	}

	OutputDebugStringA(("LoadModel: " + modelPath + "\n").c_str());

	std::shared_ptr<ModelData> model = std::make_shared<ModelData>();
	model->FilePath = modelPath;
	model->SetTexture = false;
	model->isBlender = isBlender;
	model->AiScene = aiImportFile(modelPath.c_str(), aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_ConvertToLeftHanded /* | aiProcess_GenBoundingBoxes */);
	if (!model->AiScene) {
		model->AiScene = nullptr;
		m_Models[modelPath] = model;

		return model.get();
	}

	model->VertexBuffer = new ID3D11Buffer * [model->AiScene->mNumMeshes];
	model->IndexBuffer = new ID3D11Buffer * [model->AiScene->mNumMeshes];

	DirectX::XMFLOAT3 Min, Max;

	for(unsigned int m = 0; m < model->AiScene->mNumMeshes; m++){

		aiMesh* mesh = model->AiScene->mMeshes[m];


		{	// 頂点バッファ生成
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
				if (mesh->HasPositions()) {
					if (isBlender) {
						vertex[v].Position = DirectX::XMFLOAT3(mesh->mVertices[v].x, -mesh->mVertices[v].z, mesh->mVertices[v].y);

					} else {
						vertex[v].Position = DirectX::XMFLOAT3(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z);
					}
				} else {
					vertex[v].Position = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
				}
				if (mesh->HasNormals()) {
					if (isBlender) {
						vertex[v].Normal = DirectX::XMFLOAT3(mesh->mNormals[v].x, -mesh->mNormals[v].z, mesh->mNormals[v].y);
					} else {
						vertex[v].Normal = DirectX::XMFLOAT3(mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z);
					}
				} else {
					vertex[v].Normal = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
				}
				if (mesh->HasTangentsAndBitangents()) {
					if (isBlender) {
						vertex[v].Tangent = DirectX::XMFLOAT3(mesh->mTangents[v].x, -mesh->mTangents[v].z, mesh->mTangents[v].y);
					} else {
						vertex[v].Tangent = DirectX::XMFLOAT3(mesh->mTangents[v].x, mesh->mTangents[v].y, mesh->mTangents[v].z);
					}
				} else {
					vertex[v].Tangent = DirectX::XMFLOAT3(0.0f,1.0f,0.0f);
				}
				if (mesh->HasVertexColors(v)) {
					vertex[v].Diffuse = DirectX::XMFLOAT4( mesh->mColors[v]->r , mesh->mColors[v]->g, mesh->mColors[v]->b, mesh->mColors[v]->a);
				} else {
					vertex[v].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
				}

				if (mesh->HasTextureCoords(0)) {
					vertex[v].TexCoord = DirectX::XMFLOAT2(mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y);
				} else {
					vertex[v].TexCoord = DirectX::XMFLOAT2(0.0f,0.0f);
				}


			}

			D3D11_BUFFER_DESC bd = {};
			bd.Usage = D3D11_USAGE_DYNAMIC;
			bd.ByteWidth = sizeof(VERTEX_3D) * mesh->mNumVertices;
			bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

			D3D11_SUBRESOURCE_DATA sd = {};
			sd.pSysMem = vertex;

			m_GraphicContext->GetDevice()->CreateBuffer(&bd, &sd, &model->VertexBuffer[m]);

			delete[] vertex;
		}

		{	// インデックスバッファ生成
			unsigned int* index = new unsigned int[mesh->mNumFaces * 3];
			//unsigned short* index = new unsigned short[mesh->mNumFaces * 3];

			for(unsigned int f = 0; f < mesh->mNumFaces; f++){
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

			m_GraphicContext->GetDevice()->CreateBuffer(&bd, &sd, &model->IndexBuffer[m]);

			delete[] index;
		}
	}
	if (model->AiScene->mNumTextures > 0) {
		model->SetTexture = true;

	//テクスチャ読み込み(組み込みされているテクスチャのみ)
		for (unsigned int i = 0; i < model->AiScene->mNumTextures; i++) {

			aiTexture* aitexture = model->AiScene->mTextures[i];

			ID3D11ShaderResourceView* texture;
			DirectX::TexMetadata metadata{};
			DirectX::ScratchImage image{};
			if (aitexture->pcData == NULL) {

			} else {
				LoadFromWICMemory(aitexture->pcData, aitexture->mWidth, DirectX::WIC_FLAGS::WIC_FLAGS_NONE, &metadata, image);
			}
			CreateShaderResourceView(m_GraphicContext->GetDevice(), image.GetImages(), image.GetImageCount(), metadata, &texture);
			assert(texture);

			model->Texture[aitexture->mFilename.data] = texture;
		}
	} else {
		namespace fs = std::filesystem;

		for (unsigned int i = 0; i < model->AiScene->mNumMaterials; i++) {
			aiMaterial* material = model->AiScene->mMaterials[i];

			if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
				aiString texPath;
				if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS) {
					std::string textureFilePath = texPath.C_Str();

					if (model->Texture.find(textureFilePath) == model->Texture.end()) {
						// modelPathのフォルダパスを取得
						std::string directory;
						size_t pos = modelPath.find_last_of("/\\");
						if (pos != std::string::npos) {
							directory = modelPath.substr(0, pos + 1);
						}

						std::string fullTexPath = directory + textureFilePath;

						if (fs::exists(fullTexPath)) {
							ID3D11ShaderResourceView* texture = nullptr;

							DirectX::TexMetadata metadata{};
							DirectX::ScratchImage image{};

							HRESULT hr = DirectX::LoadFromWICFile(
								std::wstring(fullTexPath.begin(), fullTexPath.end()).c_str(),
								DirectX::WIC_FLAGS_NONE,
								&metadata,
								image);

							if (SUCCEEDED(hr)) {
								hr = DirectX::CreateShaderResourceView(
									m_GraphicContext->GetDevice(),
									image.GetImages(),
									image.GetImageCount(),
									metadata,
									&texture);

								if (SUCCEEDED(hr)) {
									model->Texture[textureFilePath] = texture;
									model->SetTexture = true;

								} else {
									OutputDebugStringA(("Failed to create shader resource view for texture: " + fullTexPath + "\n").c_str());
								}
							} else {
								OutputDebugStringA(("Failed to load WIC texture file: " + fullTexPath + "\n").c_str());
							}
						} else {
							OutputDebugStringA(("Texture file not found: " + fullTexPath + "\n").c_str());
						}

					}
				}
			}

			// NormalMapテクスチャ
			if (material->GetTextureCount(aiTextureType_NORMALS) > 0) {
				aiString normalTexPath;
				if (material->GetTexture(aiTextureType_NORMALS, 0, &normalTexPath) == AI_SUCCESS) {
					std::string normalMapFile = fs::path(normalTexPath.C_Str()).filename().string();

					if (model->Texture.find(normalMapFile) == model->Texture.end()) {
						// modelPathのフォルダパスを取得
						std::string directory;
						size_t pos = modelPath.find_last_of("/\\");
						if (pos != std::string::npos) {
							directory = modelPath.substr(0, pos + 1);
						}

						std::string fullTexPath = directory + normalMapFile;

						if (fs::exists(fullTexPath)) {
							ID3D11ShaderResourceView* texture = nullptr;

							DirectX::TexMetadata metadata{};
							DirectX::ScratchImage image{};

							HRESULT hr = DirectX::LoadFromWICFile(
								std::wstring(fullTexPath.begin(), fullTexPath.end()).c_str(),
								DirectX::WIC_FLAGS_NONE,
								&metadata,
								image);

							if (SUCCEEDED(hr)) {
								hr = DirectX::CreateShaderResourceView(
									m_GraphicContext->GetDevice(),
									image.GetImages(),
									image.GetImageCount(),
									metadata,
									&texture);

								if (SUCCEEDED(hr)) {
									model->Texture[normalMapFile] = texture;
									model->SetTexture = true;

								} else {
									OutputDebugStringA(("Failed to create shader resource view for texture: " + fullTexPath + "\n").c_str());
								}
							} else {
								OutputDebugStringA(("Failed to load WIC texture file: " + fullTexPath + "\n").c_str());
							}
						} else {
							OutputDebugStringA(("Texture file not found: " + fullTexPath + "\n").c_str());
						}

					}
				}
			}
		}
	}
	//aiReleaseImport(model->AiScene);

	m_Models[modelPath] = model;

	return m_Models[modelPath].get();
}

ModelData* ModelLoader::GetModel(const std::string& modelPath){
	auto it = m_Models.find(modelPath);
	if(it != m_Models.end()){
		return it->second.get();
	}
	return nullptr;
}
