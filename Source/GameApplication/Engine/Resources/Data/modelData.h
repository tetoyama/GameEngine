// =======================================================================
// 
// modelData.h
// 
// =======================================================================
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <d3d11.h>
#include <DirectXMath.h>
#include "Shader/Common.hlsl"

#include "assimp/cimport.h"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/matrix4x4.h"

class GraphicsContext;
struct aiScene;

struct SKINNING_INPUT_VERTEX {
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 TexCoord;
	uint32_t BoneIndex[4];
	float BoneWeight[4];
	DirectX::XMFLOAT4 Diffuse;
};

struct DEFORM_VERTEX {
	aiVector3D Position;
	aiVector3D Normal;
	uint32_t BoneIndex[4];
	float BoneWeight[4];
};

struct BONE {
	aiMatrix4x4 Matrix;
	aiMatrix4x4 AnimationMatrix;
	aiMatrix4x4 OffsetMatrix;
	aiMatrix4x4 WorldMatrix;
};

struct AnimationBlend {
	std::string name;
	float weight = 0.0f;
	float animationStartTime = 0.0f;
	bool isLoop = true;
};

struct AnimationData {
	std::string FilePath;
	const aiScene* Scene = nullptr;
	aiAnimation* Animation = nullptr;
	bool isImported = true;

	void Release(){
		if(isImported && Scene){
			aiReleaseImport(Scene);
			Scene = nullptr;
			Animation = nullptr;
		}
	}
};

// Assimp Meshから抽出したBackend非依存の共有Geometry Source。
// 移行中は既存D3D11 Buffer生成にも使用し、次工程でRenderSystem/RHI側の
// Model Geometry Runtime生成元へ切り替える。
struct ModelMeshGeometryCpuData {
	std::vector<VERTEX_3D> vertices;
	std::vector<std::uint32_t> indices;

	bool IsValid() const noexcept {
		return !vertices.empty() && !indices.empty();
	}
};

struct ModelData {
public:
	ModelData(){
		OutputDebugStringA(("Created ModelData " + FilePath + "\n").c_str());
	}

	~ModelData(){
		OutputDebugStringA(("Destroyed ModelData " + FilePath + "\n").c_str());
		Release();
	}

	void Release();

	std::string FilePath = "";
	bool isBlender = false;
	bool SetTexture = false;
	const aiScene* AiScene = nullptr;

	// Backend非依存CPU Geometry。Native Bufferと寿命を分離し、RHI移行時の
	// 再生成元として保持する。
	std::vector<ModelMeshGeometryCpuData> MeshGeometry;

	// Legacy通常描画互換。生成・所有は段階的にRenderSystem/RHIへ移す。
	std::vector<ID3D11Buffer*> VertexBuffer;
	std::vector<ID3D11Buffer*> IndexBuffer;
	std::unordered_map<std::string, ID3D11ShaderResourceView*> m_Texture;

	std::vector<BONE> m_Bones;
	std::unordered_map<std::string, uint32_t> m_BoneIndexMap;
	bool enableRootMotion = false;

	std::unordered_map<std::string, AnimationData> m_Animation;
	std::vector<DEFORM_VERTEX>* m_DeformVertex = nullptr;

	std::vector<ID3D11Buffer*> m_SkinInputBuffer;
	std::vector<ID3D11ShaderResourceView*> m_SkinInputSRV;
	std::vector<ID3D11Buffer*> m_SkinOutputUAVBuffer;
	std::vector<ID3D11UnorderedAccessView*> m_SkinOutputUAV;
	std::vector<ID3D11Buffer*> m_SkinOutputVB;

	ID3D11Buffer* m_BoneCB = nullptr;
	ID3D11Buffer* m_InfoCB = nullptr;

	void CreateSkinningBuffers(GraphicsContext* ctx);
	void UpdateAndDispatchSkinning(
		GraphicsContext* ctx,
		std::vector<ID3D11Buffer*>& dynamicVertexBuffers
	);

	void CreateBone(aiNode* Node);
	void UpdateBoneMatrix(aiNode* Node, aiMatrix4x4 Parent);

	void LoadAnimation(const char* FileName, const char* Name);

	bool HasImportedAnimationSource(const std::string& filePath) const {
		for(const auto& [name, animation] : m_Animation){
			(void)name;
			if(animation.isImported && animation.FilePath == filePath){
				return true;
			}
		}
		return false;
	}

	// 共有ModelDataへ同じSource Pathを重複Importしない公開入口。
	void LoadAnimationSource(const char* fileName, const char* name){
		if(!fileName || !name || fileName[0] == '\0' || name[0] == '\0'){
			return;
		}
		if(HasImportedAnimationSource(fileName)){
			return;
		}
		LoadAnimation(fileName, name);
	}

	void RemoveAnimation(const std::string& name){
		auto it = m_Animation.find(name);
		if(it != m_Animation.end()){
			it->second.Release();
			m_Animation.erase(it);
		}
	}

	void UpdateBoneAnimation(
		const std::vector<AnimationBlend>& anims,
		float frame
	);

	void CPU_Skinning(
		const std::vector<DEFORM_VERTEX>& deformVertices,
		const aiMesh* mesh,
		VERTEX_3D* outVertex
	) const;
};
