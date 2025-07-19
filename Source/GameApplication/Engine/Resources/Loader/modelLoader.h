// LoadModelFromFile.cpp
#pragma once
#include "ResourceLoader.h"

#include <memory>
#include <string>
#include <filesystem>

#include "Engine/Resources/Data/modelData.h"
#include "Engine/Graphics/graphicsContext.h"

#include "Backends/DirectX11/DirectXTex.h"
#include "Backends/Assimp/material.h"
#include "Backends/Assimp/scene.h"
#include "Backends/Assimp/cimport.h"
#include "Backends/Assimp/postprocess.h"
#include "Backends/Assimp/matrix4x4.h"

#pragma comment (lib, "assimp-vc143-mt.lib")

#include <cassert>

inline void CreateAnimationConstantBuffer(ID3D11Device* device, ModelData* model) {
	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = sizeof(AnimationCB);
	desc.Usage = D3D11_USAGE_DYNAMIC;              // MapでCPUから書き込むので動的バッファ
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;  // CPU書き込み許可

	HRESULT hr = device->CreateBuffer(&desc, nullptr, &model->AnimationConstantBuffer);
	if (FAILED(hr)) {
		OutputDebugStringA("Failed to create AnimationConstantBuffer\n");
	}
}

inline std::shared_ptr<ModelData> LoadModelFromFile(const std::string& path, bool isBlender, GraphicsContext* context){

	auto model = std::make_shared<ModelData>();

	model->FilePath = path;
	model->SetTexture = false;
	model->isBlender = isBlender;
	model->AiScene = aiImportFile(path.c_str(), aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_ConvertToLeftHanded /* | aiProcess_GenBoundingBoxes */);


	model->VertexBuffer = new ID3D11Buffer * [model->AiScene->mNumMeshes];
	model->IndexBuffer = new ID3D11Buffer * [model->AiScene->mNumMeshes];

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
		std::vector<AnimationVertex> animVertices(vertexCount);

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
				for (UINT b = 0; b < mesh->mNumBones; ++b) {
					aiBone* bone = mesh->mBones[b];
					std::string boneName = bone->mName.C_Str();
					UINT boneIndex = 0;
					if (model->BoneNameToIndex.find(boneName) == model->BoneNameToIndex.end()) {
						boneIndex = static_cast<UINT>(model->Bones.size());
						aiMatrix4x4 offset = bone->mOffsetMatrix;
						DirectX::XMFLOAT4X4 mat(offset.a1, offset.b1, offset.c1, offset.d1,
												offset.a2, offset.b2, offset.c2, offset.d2,
												offset.a3, offset.b3, offset.c3, offset.d3,
												offset.a4, offset.b4, offset.c4, offset.d4);
						model->Bones.push_back({ boneName, mat });
						model->BoneNameToIndex[boneName] = boneIndex;
					} else {
						boneIndex = model->BoneNameToIndex[boneName];
					}
					for (UINT w = 0; w < bone->mNumWeights; ++w) {
						UINT vtxId = bone->mWeights[w].mVertexId;
						float weight = bone->mWeights[w].mWeight;
						bool assigned = false;
						for (int i = 0; i < 4; ++i) {
							if (animVertices[vtxId].BoneWeight[i] == 0.0f) {
								animVertices[vtxId].BoneIndex[i] = boneIndex;
								animVertices[vtxId].BoneWeight[i] = weight;
								assigned = true;
								break;
							}
						}
						if (!assigned) {
							// 警告: 4つ以上のボーン影響はカットされる
							std::cerr << "Warning: Vertex " << vtxId << " has more than 4 bone influences. Extra weights are ignored.\n";
						}
					}
				}
			}
		}
		// インデックスバッファ生成
		{
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

			context->GetDevice()->CreateBuffer(&bd, &sd, &model->IndexBuffer[m]);

			delete[] index;
		}

		model->SkinnedVertexData.push_back(animVertices);

		// StructuredBuffer 作成
		{
			D3D11_BUFFER_DESC desc = {};
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.ByteWidth = sizeof(AnimationVertex) * vertexCount;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
			desc.StructureByteStride = sizeof(AnimationVertex);

			D3D11_SUBRESOURCE_DATA initData = {animVertices.data()};
			ID3D11Buffer* inputBuffer = nullptr;
			HRESULT hr = context->GetDevice()->CreateBuffer(&desc, &initData, &inputBuffer);
			if(FAILED(hr) || !inputBuffer){
				std::cerr << "Failed to create input StructuredBuffer for mesh " << m << ", HRESULT=" << std::hex << hr << std::endl;
				// 必要に応じクリーンアップ
				return nullptr;
			}
			model->InputStructuredBuffers.push_back(inputBuffer);

			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = DXGI_FORMAT_UNKNOWN;
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
			srvDesc.Buffer.FirstElement = 0;
			srvDesc.Buffer.NumElements = vertexCount;
			ID3D11ShaderResourceView* srv = nullptr;
			hr = context->GetDevice()->CreateShaderResourceView(inputBuffer, &srvDesc, &srv);
			if(FAILED(hr) || !srv){
				std::cerr << "Failed to create input SRV for mesh " << m << ", HRESULT=" << std::hex << hr << std::endl;
				return nullptr;
			}
			model->InputSRVs.push_back(srv);

			// Output Buffer + UAV + SRV
			desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
			ID3D11Buffer* outputBuffer = nullptr;
			hr = context->GetDevice()->CreateBuffer(&desc, nullptr, &outputBuffer);
			if(FAILED(hr) || !outputBuffer){
				std::cerr << "Failed to create output StructuredBuffer for mesh " << m << ", HRESULT=" << std::hex << hr << std::endl;
				return nullptr;
			}
			model->OutputStructuredBuffers.push_back(outputBuffer);

			D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
			uavDesc.Format = DXGI_FORMAT_UNKNOWN;
			uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			uavDesc.Buffer.FirstElement = 0;
			uavDesc.Buffer.NumElements = vertexCount;
			ID3D11UnorderedAccessView* uav = nullptr;
			hr = context->GetDevice()->CreateUnorderedAccessView(outputBuffer, &uavDesc, &uav);
			if(FAILED(hr) || !uav){
				std::cerr << "Failed to create output UAV for mesh " << m << ", HRESULT=" << std::hex << hr << std::endl;
				return nullptr;
			}
			model->OutputUAVs.push_back(uav);

			ID3D11ShaderResourceView* outputSRV = nullptr;
			hr = context->GetDevice()->CreateShaderResourceView(outputBuffer, &srvDesc, &outputSRV);
			if(FAILED(hr) || !outputSRV){
				std::cerr << "Failed to create output SRV for mesh " << m << ", HRESULT=" << std::hex << hr << std::endl;
				return nullptr;
			}
			model->OutputSRVs.push_back(outputSRV);
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

			model->Texture[aitexture->mFilename.data] = texture;
		}
	} else{
		namespace fs = std::filesystem;

		for(unsigned int i = 0; i < model->AiScene->mNumMaterials; i++){
			aiMaterial* material = model->AiScene->mMaterials[i];

			if(material->GetTextureCount(aiTextureType_DIFFUSE) > 0){
				aiString texPath;
				if(material->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS){
					std::string textureFilePath = texPath.C_Str();

					if(model->Texture.find(textureFilePath) == model->Texture.end()){
						// modelPathのフォルダパスを取得
						std::string directory;
						size_t pos = path.find_last_of("/\\");
						if(pos != std::string::npos){
							directory = path.substr(0, pos + 1);
						}

						std::string fullTexPath = directory + textureFilePath;

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
									model->Texture[textureFilePath] = texture;
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

			// NormalMapテクスチャ
			if(material->GetTextureCount(aiTextureType_NORMALS) > 0){
				aiString normalTexPath;
				if(material->GetTexture(aiTextureType_NORMALS, 0, &normalTexPath) == AI_SUCCESS){
					std::string normalMapFile = fs::path(normalTexPath.C_Str()).filename().string();

					if(model->Texture.find(normalMapFile) == model->Texture.end()){
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
									model->Texture[normalMapFile] = texture;
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

	// アニメーション読み取り
		if (model->AiScene->HasAnimations()) {
			for (UINT i = 0; i < model->AiScene->mNumAnimations; ++i) {
				aiAnimation* aiAnim = model->AiScene->mAnimations[i];
				AnimationClip clip;
				clip.name = aiAnim->mName.C_Str();
				clip.duration = static_cast<float>(aiAnim->mDuration);
				clip.ticksPerSecond = static_cast<float>(aiAnim->mTicksPerSecond != 0.0 ? aiAnim->mTicksPerSecond : 25.0f);

				UINT maxKeyCount = 0;
				for (UINT c = 0; c < aiAnim->mNumChannels; ++c) {
					maxKeyCount = (std::max)(maxKeyCount, aiAnim->mChannels[c]->mNumPositionKeys);
				}

				for (UINT f = 0; f < maxKeyCount; ++f) {
					AnimationKeyFrame frame;
					frame.time = static_cast<float>(aiAnim->mChannels[0]->mPositionKeys[f].mTime);

					frame.boneTransforms.resize(model->Bones.size(), DirectX::XMFLOAT4X4(
						1, 0, 0, 0,
						0, 1, 0, 0,
						0, 0, 1, 0,
						0, 0, 0, 1));

					for (UINT c = 0; c < aiAnim->mNumChannels; ++c) {
						aiNodeAnim* channel = aiAnim->mChannels[c];
						std::string boneName = channel->mNodeName.C_Str();
						auto it = model->BoneNameToIndex.find(boneName);
						if (it == model->BoneNameToIndex.end()) continue;
						UINT boneIdx = it->second;

						if (f >= channel->mNumPositionKeys || f >= channel->mNumRotationKeys || f >= channel->mNumScalingKeys) {
							// キーフレーム不足はスキップ
							continue;
						}

						aiVector3D pos = channel->mPositionKeys[f].mValue;
						aiQuaternion rot = channel->mRotationKeys[f].mValue;
						aiVector3D scale = channel->mScalingKeys[f].mValue;

						aiMatrix4x4 mat;
						mat = aiMatrix4x4(scale.x, 0, 0, 0,
										  0, scale.y, 0, 0,
										  0, 0, scale.z, 0,
										  0, 0, 0, 1);
						mat *= aiMatrix4x4(rot.GetMatrix());
						mat *= aiMatrix4x4(1, 0, 0, pos.x,
										   0, 1, 0, pos.y,
										   0, 0, 1, pos.z,
										   0, 0, 0, 1);

						frame.boneTransforms[boneIdx] = DirectX::XMFLOAT4X4(
							mat.a1, mat.b1, mat.c1, mat.d1,
							mat.a2, mat.b2, mat.c2, mat.d2,
							mat.a3, mat.b3, mat.c3, mat.d3,
							mat.a4, mat.b4, mat.c4, mat.d4);
					}
					clip.keyframes.push_back(frame);
				}
				model->Animations[clip.name] = clip;
			}
		}
		// Bone行列の初期データ（単位行列）を準備
		UINT boneCount = static_cast<UINT>(model->Bones.size());
		std::vector<DirectX::XMFLOAT4X4> boneMatrixData(boneCount);
		for (UINT i = 0; i < boneCount; ++i) {
			boneMatrixData[i] = DirectX::XMFLOAT4X4(
				1, 0, 0, 0,
				0, 1, 0, 0,
				0, 0, 1, 0,
				0, 0, 0, 1
			);
		}

		// バッファの設定
		D3D11_BUFFER_DESC bd = {};
		bd.ByteWidth = sizeof(DirectX::XMFLOAT4X4) * boneCount;
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		bd.CPUAccessFlags = 0;
		bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bd.StructureByteStride = sizeof(DirectX::XMFLOAT4X4);

		// 初期データ構造体
		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = boneMatrixData.data();

		// バッファ作成
		ID3D11Buffer* boneMatrixBuffer = nullptr;
		HRESULT hr = context->GetDevice()->CreateBuffer(&bd, &initData, &boneMatrixBuffer);
		if (FAILED(hr)) {
			OutputDebugStringA("Failed to create bone matrix buffer.\n");
			return nullptr; // またはエラー処理
		}

		// SRVの作成
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = boneCount;

		hr = context->GetDevice()->CreateShaderResourceView(boneMatrixBuffer, &srvDesc, &model->BoneMatricesSRV);
		if (FAILED(hr)) {
			OutputDebugStringA("Failed to create BoneMatrices SRV.\n");
			return nullptr;
		}
		CreateAnimationConstantBuffer(context->GetDevice(), model.get());

	}
	if (!model->AnimationConstantBuffer) {
		OutputDebugStringA("Debug: AnimationConstantBuffer was NOT created.\n");
	} else {
		OutputDebugStringA("Debug: AnimationConstantBuffer successfully created.\n");
	}

	size_t meshCount = model->InputSRVs.size();
	model->OutputVertexBuffers.resize(meshCount);
	model->OutputUAVBuffers.resize(meshCount);
	model->OutputUAVs.resize(meshCount);

	return model;
}

inline void UpdateAnimationCB(GraphicsContext* context, ID3D11Buffer* cb, const AnimationCB& data){
	D3D11_MAPPED_SUBRESOURCE mapped;
	if(SUCCEEDED(context->GetDeviceContext()->Map(cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))){
		memcpy(mapped.pData, &data, sizeof(AnimationCB));
		context->GetDeviceContext()->Unmap(cb, 0);
	}
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

