#include "modelData.h"
#include "Backends/Assimp/material.h"
#include "Backends/Assimp/scene.h"
#include "Backends/Assimp/cimport.h"

ModelData::~ModelData() {
	if (AiScene) {
		for (unsigned int m = 0; m < AiScene->mNumMeshes; m++) {

			VertexBuffer[m]->Release();
			IndexBuffer[m]->Release();
		}
		delete[] VertexBuffer;
		delete[] IndexBuffer;

		if (SetTexture) {
			for (std::pair<const std::string, ID3D11ShaderResourceView*> pair : Texture) {

				pair.second->Release();
			}
		}
		aiReleaseImport(AiScene);
	}
}
