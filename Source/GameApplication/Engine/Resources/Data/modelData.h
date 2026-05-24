// =======================================================================
// 
// modelData.h
// 
// =======================================================================
#pragma once
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

// GPUスキニング用の入力頂点構造体
struct SKINNING_INPUT_VERTEX {
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT3 normal;
	DirectX::XMFLOAT2 texCoord;
	uint32_t boneIndex[4];
	float boneWeight[4];
	DirectX::XMFLOAT4 diffuse; // 追加
};

//変形後頂点構造体
struct DEFORM_VERTEX {
	aiVector3D position;
	aiVector3D normal;
	uint32_t boneIndex[4];
	float boneWeight[4];
};

//ボーン構造体
struct BONE {
	aiMatrix4x4 matrix;
	aiMatrix4x4 animationMatrix;
	aiMatrix4x4 offsetMatrix;
	aiMatrix4x4 worldMatrix;  // ボーンのワールド変換行列（OffsetMatrixを乗算する前）
};

// アニメーションブレンド情報を保持する構造体
struct AnimationBlend {
	std::string name;
	float weight = 0.0f;
	float animationStartTime = 0.0f;
	bool isLoop = true;
};

// アニメーションリソースを保持する構造体
struct AnimationData {
	std::string filePath;
	const aiScene* Scene = nullptr;
	aiAnimation* Animation = nullptr;
	bool isImported = true;


	void Release() {
		if (isImported && Scene) {
			aiReleaseImport(Scene);
			Scene = nullptr;
			Animation = nullptr;
		}
	}
};

// 3Dモデルのリソースデータを保持する構造体
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

	// ----------------------------
	// basic model info
	// ----------------------------
	std::string filePath= "";
	bool isBlender = false;
	bool setTexture= false;

	const aiScene* AiScene = nullptr;

	// ----------------------------
	// classic rendering buffers
	// ----------------------------
	std::vector<ID3D11Buffer*> VertexBuffer;
	std::vector<ID3D11Buffer*> IndexBuffer;

	std::unordered_map<std::string, ID3D11ShaderResourceView*> m_Texture;

	// ----------------------------
	// skeleton
	// ----------------------------
	std::vector<BONE> bones;
	std::unordered_map<std::string, uint32_t> boneIndexMap;

	bool enableRootMotion = false;

	// ----------------------------
	// animation
	// ----------------------------
	std::unordered_map<std::string, AnimationData> animation;

	// per mesh deform data (CPU or upload source)
	std::vector<DEFORM_VERTEX>* m_DeformVertex = nullptr;

	// ============================================================
	// GPU skinning resources (per mesh)
	// ============================================================

	// Input structured buffer (static)
	std::vector<ID3D11Buffer*>              m_SkinInputBuffer;
	std::vector<ID3D11ShaderResourceView*>  m_SkinInputSRV;

	// Output structured buffer (CS write only)
	std::vector<ID3D11Buffer*>              m_SkinOutputUAVBuffer;
	std::vector<ID3D11UnorderedAccessView*> m_SkinOutputUAV;

	// Output vertex buffer (Draw only)
	std::vector<ID3D11Buffer*>             m_SkinOutputVB;

	// ============================================================
	// constant buffers (shared)
	// ============================================================

	// bone matrices (MAX_BONES * float4x4)
	ID3D11Buffer* m_BoneCB = nullptr;

	// optional info CB (vertex count etc.)
	ID3D11Buffer* m_InfoCB = nullptr;

	// ============================================================
	// helpers
	// ============================================================

	void CreateSkinningBuffers(GraphicsContext* ctx);
	void UpdateAndDispatchSkinning(GraphicsContext* ctx, std::vector<ID3D11Buffer*>& dynamicVertexBuffers);

	// skeleton helpers
	void CreateBone(aiNode* Node);
	void UpdateBoneMatrix(aiNode* Node, aiMatrix4x4 Parent);

	// animation
	void LoadAnimation(const char* FileName, const char* Name);
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

	// CPU fallback (debug / compare)
	void CPU_Skinning(
		const std::vector<DEFORM_VERTEX>& deformVertices,
		const aiMesh* mesh,
		VERTEX_3D* outVertex
	) const;

};
