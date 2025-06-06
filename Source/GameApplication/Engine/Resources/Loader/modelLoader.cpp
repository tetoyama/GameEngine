#include "modelLoader.h"

#include "Backends/DirectX11/DirectXTex.h"

#include "Backends/Assimp/material.h"
#include "Backends/Assimp/scene.h"
#include "Backends/Assimp/cimport.h"
#include "Backends/Assimp/postprocess.h"
#include "Backends/Assimp/matrix4x4.h"

#include "Engine/Graphics/graphicsContext.h"

#include "Engine/Resources/Data/modelData.h"

#pragma comment (lib, "assimp-vc143-mt.lib")

ModelData* ModelLoader::LoadModel(const std::string& modelPath){

	if(m_Models.count(modelPath)){
		return m_Models[modelPath].get();
	}

	std::shared_ptr<ModelData> model = std::make_shared<ModelData>();

	model->SetTexture = true;

	model->AiScene = aiImportFile(modelPath.c_str(), aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_ConvertToLeftHanded /* | aiProcess_GenBoundingBoxes */);
	assert(model->AiScene);

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

				//mayaのみ対応
				vertex[v].Position = DirectX::XMFLOAT3(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z);

				vertex[v].TexCoord = DirectX::XMFLOAT2(mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y);
				vertex[v].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

				vertex[v].Normal = DirectX::XMFLOAT3(mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z);
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

	if(model->AiScene->mNumTextures > 0){
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
			CreateShaderResourceView(m_GraphicContext->GetDevice(), image.GetImages(), image.GetImageCount(), metadata, &texture);
			assert(texture);

			model->Texture[aitexture->mFilename.data] = texture;
		}
	} else{
		model->SetTexture = false;
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
