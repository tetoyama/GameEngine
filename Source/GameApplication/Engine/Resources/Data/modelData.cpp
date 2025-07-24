#include "modelData.h"
#include "Backends/Assimp/material.h"
#include "Backends/Assimp/scene.h"
#include "Backends/Assimp/cimport.h"

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
			for(auto& pair : Texture){
				if(pair.second) pair.second->Release();
			}
			Texture.clear();
		}

		aiReleaseImport(AiScene);
		AiScene = nullptr;
	}

	// InputStructuredBuffers 解放
	for(auto buf : InputStructuredBuffers){
		if(buf) buf->Release();
	}
	InputStructuredBuffers.clear();

	// InputSRVs 解放
	for(auto srv : InputSRVs){
		if(srv) srv->Release();
	}
	InputSRVs.clear();

	// OutputStructuredBuffers 解放
	for(auto buf : OutputStructuredBuffers){
		if(buf) buf->Release();
	}
	OutputStructuredBuffers.clear();

	// OutputUAVs 解放
	for(auto uav : OutputUAVs){
		if(uav) uav->Release();
	}
	OutputUAVs.clear();

	// OutputSRVs 解放
	for(auto srv : OutputSRVs){
		if(srv) srv->Release();
	}
	OutputSRVs.clear();

	// OutputVertexBuffers 解放
	for(auto buf : OutputVertexBuffers){
		if(buf) buf->Release();
	}
	OutputVertexBuffers.clear();

	// OutputUAVBuffers 解放
	for(auto buf : OutputUAVBuffers){
		if(buf) buf->Release();
	}
	OutputUAVBuffers.clear();

	// アニメーション用定数バッファ
	if(AnimationConstantBuffer){
		AnimationConstantBuffer->Release();
		AnimationConstantBuffer = nullptr;
	}

	// ボーン行列SRV
	if(BoneMatricesSRV){
		BoneMatricesSRV->Release();
		BoneMatricesSRV = nullptr;
	}
}
