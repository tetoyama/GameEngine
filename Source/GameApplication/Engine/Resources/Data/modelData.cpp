#include "modelData.h"
#include "Backends/Assimp/material.h"
#include "Backends/Assimp/scene.h"
#include "Backends/Assimp/cimport.h"

void ModelData::Release(){
	if(AiScene){
		for(unsigned int m = 0; m < AiScene->mNumMeshes; m++){

			VertexBuffer[m]->Release();
			IndexBuffer[m]->Release();
		}
		delete[] VertexBuffer;
		delete[] IndexBuffer;

		if(SetTexture){
			for(std::pair<const std::string, ID3D11ShaderResourceView*> pair : Texture){

				pair.second->Release();
			}
		}
		aiReleaseImport(AiScene);
	}
	// InputStructuredBuffers 解放
	for(auto buf : InputStructuredBuffers){
		if(buf){
			buf->Release();
		}
	}
	InputStructuredBuffers.clear();

	// InputSRVs 解放
	for(auto srv : InputSRVs){
		if(srv){
			srv->Release();
		}
	}
	InputSRVs.clear();

	// OutputStructuredBuffers 解放
	for(auto buf : OutputStructuredBuffers){
		if(buf){
			buf->Release();
		}
	}
	OutputStructuredBuffers.clear();

	// OutputUAVs 解放
	for(auto uav : OutputUAVs){
		if(uav){
			uav->Release();
		}
	}
	OutputUAVs.clear();

	// OutputSRVs 解放
	for(auto srv : OutputSRVs){
		if(srv){
			srv->Release();
		}
	}
	OutputSRVs.clear();

	// AnimationConstantBuffer 解放
	if(AnimationConstantBuffer){
		AnimationConstantBuffer->Release();
		AnimationConstantBuffer = nullptr;
	}
}