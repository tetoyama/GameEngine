#include "model.h"

#pragma comment (lib, "assimp-vc143-mt.lib")

#include "Backends/DirectX11/DirectXTex.h"
#include "Engine/Graphics/graphicsContext.h"

#include "Backends/Assimp/cimport.h"
#include "Backends/Assimp/postprocess.h"
#include "Backends/Assimp/matrix4x4.h"

using namespace DirectX;

static GraphicsContext* g_GraphicContext = nullptr;

void InitModel(GraphicsContext* set){
	g_GraphicContext = set;
}

MODEL* LoadModel(const char* FileName, bool MayaBuild, bool SetTexture){
	MODEL* model = new MODEL;

	model->SetTexture = true;

	const std::string modelPath(FileName);

	model->AiScene = aiImportFile(FileName, aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_ConvertToLeftHanded /* | aiProcess_GenBoundingBoxes */);
	assert(model->AiScene);

	model->VertexBuffer = new ID3D11Buffer * [model->AiScene->mNumMeshes];
	model->IndexBuffer = new ID3D11Buffer * [model->AiScene->mNumMeshes];

	XMFLOAT3 Min, Max;

	for(unsigned int m = 0; m < model->AiScene->mNumMeshes; m++){

		aiMesh* mesh = model->AiScene->mMeshes[m];


		{	// 頂点バッファ生成
			VERTEX_3D* vertex = new VERTEX_3D[mesh->mNumVertices];

			for(unsigned int v = 0; v < mesh->mNumVertices; v++){

				if(v == 0 && m == 0){
					Min = Max = XMFLOAT3(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z);
				} else{

					Min.x = (std::min)(mesh->mVertices[v].x, Min.x);
					Min.y = (std::min)(mesh->mVertices[v].y, Min.y);
					Min.z = (std::min)(mesh->mVertices[v].z, Min.z);

					Max.x = (std::max)(mesh->mVertices[v].x, Max.x);
					Max.y = (std::max)(mesh->mVertices[v].y, Max.y);
					Max.z = (std::max)(mesh->mVertices[v].z, Max.z);

				}

				vertex[v].Position = XMFLOAT3(mesh->mVertices[v].x, -mesh->mVertices[v].z, mesh->mVertices[v].y);
				if(MayaBuild){
					vertex[v].Position = XMFLOAT3(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z);
				}
				vertex[v].TexCoord = XMFLOAT2(mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y);
				vertex[v].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
				//vertex[v].Normal = XMFLOAT3(mesh->mNormals[v].x, -mesh->mNormals[v].z, mesh->mNormals[v].y);
				vertex[v].Normal = XMFLOAT3(mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z);
			}

			D3D11_BUFFER_DESC bd = {};
			bd.Usage = D3D11_USAGE_DYNAMIC;
			bd.ByteWidth = sizeof(VERTEX_3D) * mesh->mNumVertices;
			bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

			D3D11_SUBRESOURCE_DATA sd = {};
			sd.pSysMem = vertex;

			g_GraphicContext->GetDevice()->CreateBuffer(&bd, &sd, &model->VertexBuffer[m]);

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

			g_GraphicContext->GetDevice()->CreateBuffer(&bd, &sd, &model->IndexBuffer[m]);

			delete[] index;
		}
	}

	if(model->AiScene->mNumTextures > 0 && SetTexture){
		//テクスチャ読み込み(組み込みされているテクスチャのみ)
		for(unsigned int i = 0; i < model->AiScene->mNumTextures; i++){

			aiTexture* aitexture = model->AiScene->mTextures[i];

			ID3D11ShaderResourceView* texture;
			TexMetadata metadata;
			ScratchImage image;
			if(aitexture->pcData == NULL){

			} else{
				LoadFromWICMemory(aitexture->pcData, aitexture->mWidth, WIC_FLAGS_NONE, &metadata, image);
			}
			CreateShaderResourceView(g_GraphicContext->GetDevice(), image.GetImages(), image.GetImageCount(), metadata, &texture);
			assert(texture);

			model->Texture[aitexture->mFilename.data] = texture;
		}
	} else{
		model->SetTexture = false;
	}
	//aiReleaseImport(model->AiScene);

	return model;
}
void ReleaseModel(MODEL* pModel){

	for(unsigned int m = 0; m < pModel->AiScene->mNumMeshes; m++){

		pModel->VertexBuffer[m]->Release();
		pModel->IndexBuffer[m]->Release();
	}
	delete[] pModel->VertexBuffer;
	delete[] pModel->IndexBuffer;

	for(std::pair<const std::string, ID3D11ShaderResourceView*> pair : pModel->Texture){

		pair.second->Release();
	}
	aiReleaseImport(pModel->AiScene);	//ModelLoadに移動?

	if(!pModel->SetTexture){

	}
	delete pModel;
}

void DrawModel(XMMATRIX World, MODEL* pModel){

	for(unsigned int m = 0; m < pModel->AiScene->mNumMeshes; m++){
		if(pModel->SetTexture){

			//テクスチャ設定
			aiString Texture;
			aiMaterial* aiMaterial = pModel->AiScene->mMaterials[pModel->AiScene->mMeshes[m]->mMaterialIndex];
			aiMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &Texture);

			if(aiString("") != Texture){
				g_GraphicContext->GetDeviceContext()->PSSetShaderResources(0, 1, &pModel->Texture[Texture.data]);
			}
		} else{
			//SetTexture(0);
		}
		// 頂点バッファ設定
		UINT stride = sizeof(VERTEX_3D);
		UINT offset = 0;
		g_GraphicContext->GetDeviceContext()->IASetVertexBuffers(0, 1, &pModel->VertexBuffer[m], &stride, &offset);

		//インデックスバッファ生成
		g_GraphicContext->GetDeviceContext()->IASetIndexBuffer(pModel->IndexBuffer[m], DXGI_FORMAT_R32_UINT, 0);

		// プリミティブトポロジ設定
		g_GraphicContext->GetDeviceContext()->IASetPrimitiveTopology(
			D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST		//渡された頂点の配列をどのように解釈するか
		);

		// マテリアル設定
		MATERIAL material;
		ZeroMemory(&material, sizeof(material));
		aiMaterial* pAiMaterial = pModel->AiScene->mMaterials[pModel->AiScene->mMeshes[m]->mMaterialIndex];
		aiColor3D diffuse;
		pAiMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
		material.Diffuse = XMFLOAT4(diffuse.r, diffuse.g, diffuse.b, 1);
		g_GraphicContext->SetMaterial(material);

		//g_GraphicContext->SetLight(World);
		g_GraphicContext->SetWorldMatrix(World);
		g_GraphicContext->SetDepthEnable(true);
		// ポリゴン描画
		g_GraphicContext->GetDeviceContext()->DrawIndexed(pModel->AiScene->mMeshes[m]->mNumFaces * 3, 0, 0);
	}
}
