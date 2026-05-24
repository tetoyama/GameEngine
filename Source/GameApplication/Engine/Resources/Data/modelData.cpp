// =======================================================================
// 
// modelData.cpp
// 
// =======================================================================
#include "modelData.h"
#include "Backends/Assimp/material.h"
#include "Backends/Assimp/scene.h"
#include "Backends/Assimp/cimport.h"
#include "Backends/Assimp/postprocess.h"

#include "Graphics/graphicsContext.h"

aiQuaternion InterpolateRotation(
	float time,
	const aiNodeAnim* nodeAnim
){
	if(nodeAnim->mNumRotationKeys == 1)
		return nodeAnim->mRotationKeys[0].mValue;

	uint32_t m_Index= 0;
	while(index + 1 < nodeAnim->mNumRotationKeys &&
		  time > (float)nodeAnim->mRotationKeys[index + 1].mTime){
		index++;
	}

	uint32_t m_NextIndex= index + 1;
	if(nextIndex >= nodeAnim->mNumRotationKeys)
		return nodeAnim->mRotationKeys[index].mValue;

	float m_T1= (float)nodeAnim->mRotationKeys[index].mTime;
	float m_T2= (float)nodeAnim->mRotationKeys[nextIndex].mTime;
	float m_Factor= (time - t1) / (t2 - t1);

	aiQuaternion m_Out;
	aiQuaternion::Interpolate(
		out,
		nodeAnim->mRotationKeys[index].mValue,
		nodeAnim->mRotationKeys[nextIndex].mValue,
		factor
	);
	out.Normalize();
	return m_Out;
}
aiVector3D InterpolatePosition(
	float time,
	const aiNodeAnim* nodeAnim
){
	if(nodeAnim->mNumPositionKeys == 1)
		return nodeAnim->mPositionKeys[0].mValue;

	uint32_t m_Index= 0;
	while(index + 1 < nodeAnim->mNumPositionKeys &&
		  time > (float)nodeAnim->mPositionKeys[index + 1].mTime){
		index++;
	}

	uint32_t m_NextIndex= index + 1;
	if(nextIndex >= nodeAnim->mNumPositionKeys)
		return nodeAnim->mPositionKeys[index].mValue;

	float m_T1= (float)nodeAnim->mPositionKeys[index].mTime;
	float m_T2= (float)nodeAnim->mPositionKeys[nextIndex].mTime;
	float m_Factor= (time - t1) / (t2 - t1);

	return
		nodeAnim->mPositionKeys[index].mValue * (1.0f - factor) +
		nodeAnim->mPositionKeys[nextIndex].mValue * factor;
}

void ModelData::Release(){
	// ----------------------------
	// classic buffers
	// ----------------------------
	for(auto*& vb : VertexBuffer){
		if(vb){
			vb->Release(); vb = nullptr;
		}
	}
	for(auto*& ib : IndexBuffer){
		if(ib){
			ib->Release(); ib = nullptr;
		}
	}
	VertexBuffer.clear();
	IndexBuffer.clear();

	// ----------------------------
	// textures
	// ----------------------------
	if(SetTexture){
		for(auto& pair : m_Texture){
			if(pair.second){
				pair.second->Release();
			}
		}
		m_Texture.clear();
	}

	// ----------------------------
	// animations (imported scenes only)
	// ----------------------------
	for(auto& pair : m_Animation){
		if(pair.second.isImported && pair.second.Scene){
			aiReleaseImport(pair.second.Scene);
			pair.second.Scene = nullptr;
		}
	}
	m_Animation.clear();

	// ----------------------------
	// deform data
	// ----------------------------
	delete[] m_DeformVertex;
	m_DeformVertex = nullptr;

	// ============================================================
	// GPU skinning resources
	// ============================================================

	for(auto*& b : m_SkinInputBuffer){
		if(b){
			b->Release(); b = nullptr;
		}
	}
	for(auto*& s : m_SkinInputSRV){
		if(s){
			s->Release(); s = nullptr;
		}
	}
	for(auto*& b : m_SkinOutputUAVBuffer){
		if(b){
			b->Release(); b = nullptr;
		}
	}
	for(auto*& u : m_SkinOutputUAV){
		if(u){
			u->Release(); u = nullptr;
		}
	}
	for(auto*& vb : m_SkinOutputVB){
		if(vb){
			vb->Release(); vb = nullptr;
		}
	}

	m_SkinInputBuffer.clear();
	m_SkinInputSRV.clear();
	m_SkinOutputUAVBuffer.clear();
	m_SkinOutputUAV.clear();
	m_SkinOutputVB.clear();

	// ----------------------------
	// constant buffers
	// ----------------------------
	if(m_BoneCB){
		m_BoneCB->Release();
		m_BoneCB = nullptr;
	}
	if(m_InfoCB){
		m_InfoCB->Release();
		m_InfoCB = nullptr;
	}

	// ----------------------------
	// Assimp scene
	// ----------------------------
	if(AiScene){
		aiReleaseImport(AiScene);
		AiScene = nullptr;
	}
}

void ModelData::CreateBone(aiNode* node){
	std::string m_Name= node->mName.C_Str();

	if(m_BoneIndexMap.find(name) == m_BoneIndexMap.end()){
		uint32_t m_Index= (uint32_t)m_Bones.size();
		m_BoneIndexMap[name] = index;
		m_Bones.push_back(BONE{});
	}

	for(uint32_t i = 0; i < node->mNumChildren; i++){
		CreateBone(node->mChildren[i]);
	}
}


void ModelData::UpdateBoneMatrix(aiNode* node, aiMatrix4x4 Matrix) {

	uint32_t m_Index= m_BoneIndexMap[node->mName.C_Str()];
	BONE& bone = m_Bones[index];
	aiMatrix4x4 m_WorldMatrix= Matrix;
	if(enableRootMotion || node != AiScene->mRootNode){
		worldMatrix = Matrix * bone.AnimationMatrix;
		bone.WorldMatrix = worldMatrix;
		bone.Matrix = worldMatrix * bone.OffsetMatrix;
	}
	for (unsigned int i = 0; i < node->mNumChildren; i++) {

		UpdateBoneMatrix(node->mChildren[i], worldMatrix);
	}
}

void ModelData::LoadAnimation(const char* FileName, const char* Name){
	const aiScene* scene = aiImportFile(FileName, aiProcess_ConvertToLeftHanded);
	assert(scene);
	if(scene == nullptr){
		OutputDebugStringA(("Failed to load animation file: " + std::string(FileName) + "\n").c_str());
		return;
	}
	if(!scene->HasAnimations()){
		OutputDebugStringA(("No animations found in file: " + std::string(FileName) + "\n").c_str());
		return;
	}

	auto m_It= m_Animation.find(Name);
	if (it != m_Animation.end()) {
		return;
	}

	AnimationData m_AnimationData;
	animationData.FilePath = FileName;
	animationData.Scene = scene;
	animationData.isImported = true;
	animationData.Animation = scene->mAnimations[0];
	
	m_Animation[Name] = animationData;
	if(scene->mNumAnimations != 0){

		for(unsigned int i = 0; i < scene->mNumAnimations; i++){

			scene = aiImportFile(FileName, aiProcess_ConvertToLeftHanded);

			AnimationData m_AnimationData;
			animationData.FilePath = FileName;
			animationData.Scene = scene;
			animationData.isImported = true;
			animationData.Animation = scene->mAnimations[i];
			m_Animation[scene->mAnimations[i]->mName.C_Str()] = animationData;

		}
	}
}

static float QuaternionDot(const aiQuaternion& a, const aiQuaternion& b){
	return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

void ModelData::UpdateBoneAnimation(
	const std::vector<AnimationBlend>& anims,
	float frame
){
	if(anims.empty()) return;

	float m_TotalWeight= 0.0f;
	for(const auto& a : anims) totalWeight += a.weight;
	if(totalWeight <= 0.0f) return;

	const size_t m_BoneCount= m_Bones.size();

	std::vector<aiQuaternion> accumRot(boneCount, aiQuaternion(0, 0, 0, 0));
	std::vector<aiVector3D>  accumPos(boneCount, aiVector3D(0, 0, 0));
	std::vector<bool>        hasRot(boneCount, false);

	for(const auto& anim : anims){
		if(anim.weight <= 0.0f) continue;

		auto m_ItAnim= m_Animation.find(anim.name);
		if(itAnim == m_Animation.end()) continue;

		aiAnimation* animation = itAnim->second.Animation;
		if(!animation) continue;

		float m_Time= frame + anim.animationStartTime;
		float m_Duration= (float)animation->mDuration;

		float m_AnimTime;
		if(anim.isLoop){
			animTime = fmod(time, duration);
		} else{
			animTime = std::clamp(time, 0.0f, duration);
		}

		float m_W= anim.weight / totalWeight;

		for(unsigned int c = 0; c < animation->mNumChannels; c++){
			aiNodeAnim* nodeAnim = animation->mChannels[c];

			auto m_ItBone= m_BoneIndexMap.find(nodeAnim->mNodeName.C_Str());
			if(itBone == m_BoneIndexMap.end()) continue;

			uint32_t m_Idx= itBone->second;

			aiQuaternion m_Rot= InterpolateRotation(animTime, nodeAnim);
			aiVector3D m_Pos= InterpolatePosition(animTime, nodeAnim);

			// ---- Quaternion Lerp (符号補正あり) ----
			if(hasRot[idx]){
				if(QuaternionDot(accumRot[idx], rot) < 0.0f){
					rot = aiQuaternion(-rot.x, -rot.y, -rot.z, -rot.w);
				}
			}

			accumRot[idx].x += rot.x * w;
			accumRot[idx].y += rot.y * w;
			accumRot[idx].z += rot.z * w;
			accumRot[idx].w += rot.w * w;
			hasRot[idx] = true;

			accumPos[idx] += pos * w;
		}
	}

	// 行列化
	for(size_t i = 0; i < boneCount; i++){
		aiQuaternion m_R= accumRot[i];
		if(hasRot[i]){
			r.Normalize();
		} else{
			r = aiQuaternion(); // identity
		}

		m_Bones[i].AnimationMatrix =
			aiMatrix4x4(aiVector3D(1, 1, 1), r, accumPos[i]);
	}

	aiMatrix4x4 rootMatrix(
		aiVector3D(1, 1, 1),
		aiQuaternion(DirectX::XM_PI, 0, 0),
		aiVector3D(0, 0, 0)
	);

	UpdateBoneMatrix(AiScene->mRootNode, rootMatrix);
}



void ModelData::CPU_Skinning(
	const std::vector<DEFORM_VERTEX>& deformVertices,
	const aiMesh* mesh,
	VERTEX_3D* outVertex
) const{
	for(unsigned int j = 0; j < mesh->mNumVertices; j++){
		const DEFORM_VERTEX& dv = deformVertices[j];

		aiVector3D blendedPos(0, 0, 0);
		aiVector3D blendedNormal(0, 0, 0);

		for(int k = 0; k < 4; k++){
			float m_W= dv.BoneWeight[k];
			if(w <= 0.0f) continue;

			const aiMatrix4x4& boneMat =
				m_Bones[dv.BoneIndex[k]].Matrix;

			aiVector3D m_P= mesh->mVertices[j];
			p *= boneMat;

			aiMatrix3x3 normalMat(boneMat);
			normalMat = normalMat.Inverse().Transpose();

			aiVector3D m_N= mesh->mNormals[j];
			n *= normalMat;

			blendedPos += p * w;
			blendedNormal += n * w;
		}

		blendedNormal.Normalize();

		outVertex[j].Position =
		{blendedPos.x, blendedPos.y, blendedPos.z};
		outVertex[j].Normal =
		{blendedNormal.x, blendedNormal.y, blendedNormal.z};

		if(mesh->HasTextureCoords(0)){
			outVertex[j].TexCoord =
			{
				mesh->mTextureCoords[0][j].x,
				mesh->mTextureCoords[0][j].y
			};
		} else{
			outVertex[j].TexCoord = {0.0f, 0.0f};
		}

		outVertex[j].Diffuse = {1,1,1,1};
	}
}

void ModelData::CreateSkinningBuffers(GraphicsContext* ctx){
	ID3D11Device* dev = ctx->GetDevice();
	const size_t m_MeshCount= AiScene->mNumMeshes;

	m_SkinInputBuffer.resize(meshCount);
	m_SkinInputSRV.resize(meshCount);

	m_SkinOutputUAVBuffer.resize(meshCount);
	m_SkinOutputUAV.resize(meshCount);
	m_SkinOutputVB.resize(meshCount);

	for(size_t m = 0; m < meshCount; ++m){
		aiMesh* mesh = AiScene->mMeshes[m];
		UINT m_VertexCount= mesh->mNumVertices;

		// ============================
		// Input StructuredBuffer (SRV)
		// ============================
		std::vector<SKINNING_INPUT_VERTEX> input(vertexCount);

		for(UINT v = 0; v < vertexCount; ++v){
			input[v].Position = {mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z};
			input[v].Normal = mesh->mNormals ? DirectX::XMFLOAT3{mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z} : DirectX::XMFLOAT3{0,0,0};
			input[v].TexCoord = mesh->HasTextureCoords(0) ? DirectX::XMFLOAT2{mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y} : DirectX::XMFLOAT2{0,0};

			const DEFORM_VERTEX& dv = m_DeformVertex[m][v];
			for(int i = 0; i < 4; i++){
				input[v].BoneIndex[i] = dv.BoneIndex[i];
				input[v].BoneWeight[i] = dv.BoneWeight[i];
			}

			input[v].Diffuse = DirectX::XMFLOAT4(1, 1, 1, 1);
		}

		D3D11_BUFFER_DESC m_InBd{};
		inBD.Usage = D3D11_USAGE_DEFAULT;
		inBD.ByteWidth = sizeof(SKINNING_INPUT_VERTEX) * vertexCount;
		inBD.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		inBD.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		inBD.StructureByteStride = sizeof(SKINNING_INPUT_VERTEX);

		D3D11_SUBRESOURCE_DATA m_InSd{};
		inSD.pSysMem = input.data();

		HRESULT m_Hr= dev->CreateBuffer(&inBD, &inSD, &m_SkinInputBuffer[m]);
		assert(SUCCEEDED(hr));

		D3D11_SHADER_RESOURCE_VIEW_DESC m_SrvDesc{};
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.Buffer.NumElements = vertexCount;

		hr = dev->CreateShaderResourceView(m_SkinInputBuffer[m], &srvDesc, &m_SkinInputSRV[m]);
		assert(SUCCEEDED(hr));

		// ============================
		// Output UAV Buffer
		// ============================
		D3D11_BUFFER_DESC m_OutUavbd{};
		outUAVBD.Usage = D3D11_USAGE_DEFAULT;
		outUAVBD.ByteWidth = sizeof(VERTEX_3D) * vertexCount;
		outUAVBD.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
		outUAVBD.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		outUAVBD.StructureByteStride = sizeof(VERTEX_3D);

		hr = dev->CreateBuffer(&outUAVBD, nullptr, &m_SkinOutputUAVBuffer[m]);
		assert(SUCCEEDED(hr));

		D3D11_UNORDERED_ACCESS_VIEW_DESC m_UavDesc{};
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.Buffer.NumElements = vertexCount;

		hr = dev->CreateUnorderedAccessView(m_SkinOutputUAVBuffer[m], &uavDesc, &m_SkinOutputUAV[m]);
		assert(SUCCEEDED(hr));

		// ============================
		// VertexBuffer (描画用)
		// ============================
		D3D11_BUFFER_DESC m_VbBd{};
		vbBD.Usage = D3D11_USAGE_DYNAMIC;
		vbBD.ByteWidth = sizeof(VERTEX_3D) * vertexCount;
		vbBD.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vbBD.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		hr = dev->CreateBuffer(&vbBD, nullptr, &m_SkinOutputVB[m]);
		assert(SUCCEEDED(hr));
	}

	// Bone CB
	{
		D3D11_BUFFER_DESC m_Cbd{};
		cbd.Usage = D3D11_USAGE_DYNAMIC;
		cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		cbd.ByteWidth = sizeof(DirectX::XMMATRIX) * BONE_MAX_COUNT;
		dev->CreateBuffer(&cbd, nullptr, &m_BoneCB);
	}

	// Info CB
	{
		D3D11_BUFFER_DESC m_Ibd{};
		ibd.Usage = D3D11_USAGE_DYNAMIC;
		ibd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		ibd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		ibd.ByteWidth = 16;
		dev->CreateBuffer(&ibd, nullptr, &m_InfoCB);
	}
}

void ModelData::UpdateAndDispatchSkinning(GraphicsContext* ctx, std::vector<ID3D11Buffer*>& dynamicVertexBuffers){
	ID3D11DeviceContext* dc = ctx->GetDeviceContext();
	const UINT m_MaxBones= BONE_MAX_COUNT;

	// 1) Bone palette
	std::vector<DirectX::XMMATRIX> bonePal(MAX_BONES, DirectX::XMMatrixIdentity());
	for(size_t i = 0; i < m_Bones.size() && i < MAX_BONES; ++i){
		const aiMatrix4x4& a = m_Bones[i].Matrix;
		DirectX::XMMATRIX m(a.a1, a.a2, a.a3, a.a4,
							a.b1, a.b2, a.b3, a.b4,
							a.c1, a.c2, a.c3, a.c4,
							a.d1, a.d2, a.d3, a.d4);
		bonePal[i] = m;
	}

	// 2) Update Bone CB
	D3D11_MAPPED_SUBRESOURCE m_Mapped{};
	HRESULT m_Hr= dc->Map(m_BoneCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	assert(SUCCEEDED(hr));
	memcpy(mapped.pData, bonePal.data(), sizeof(DirectX::XMMATRIX) * MAX_BONES);
	dc->Unmap(m_BoneCB, 0);

	// 3) Dispatch per mesh
	for(size_t m = 0; m < AiScene->mNumMeshes; ++m){
		aiMesh* mesh = AiScene->mMeshes[m];
		const uint32_t m_VertexCount= mesh->mNumVertices;

		// Info CB
		D3D11_MAPPED_SUBRESOURCE m_MapInfo{};
		hr = dc->Map(m_InfoCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapInfo);
		assert(SUCCEEDED(hr));
		reinterpret_cast<uint32_t*>(mapInfo.pData)[0] = vertexCount;
		dc->Unmap(m_InfoCB, 0);

		// Bind SRV/UAV/CB
		dc->CSSetShaderResources(0, 1, &m_SkinInputSRV[m]);
		dc->CSSetUnorderedAccessViews(0, 1, &m_SkinOutputUAV[m], nullptr);
		dc->CSSetConstantBuffers(0, 1, &m_BoneCB);
		dc->CSSetConstantBuffers(1, 1, &m_InfoCB);

		// Dispatch CS
		dc->CSSetShader(ctx->GetSkinningShader(), nullptr, 0);
		uint32_t m_Group= (vertexCount + 63) / 64;
		dc->Dispatch(group, 1, 1);

		// Unbind
		ID3D11UnorderedAccessView* nullUAV = nullptr;
		dc->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
		ID3D11ShaderResourceView* nullSRV = nullptr;
		dc->CSSetShaderResources(0, 1, &nullSRV);
		dc->CSSetShader(nullptr, nullptr, 0);

		// --- GPU 内で CopyResource して描画用 VertexBuffer に転送 ---
		dc->CopyResource(dynamicVertexBuffers[m], m_SkinOutputUAVBuffer[m]);
	}
}

