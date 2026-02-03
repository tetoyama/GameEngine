#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <d3d11.h>
#include <DirectXMath.h>
#include "Shader/CommonBuffer.h"

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
	float    BoneWeight[4];
	DirectX::XMFLOAT4 Diffuse; // 追加
};

//変形後頂点構造体
struct DEFORM_VERTEX {
	aiVector3D		Position;
	aiVector3D		Normal;
	uint32_t		BoneIndex[4];
	float			BoneWeight[4];
};

//ボーン構造体
struct BONE {
	aiMatrix4x4 Matrix;
	aiMatrix4x4 AnimationMatrix;
	aiMatrix4x4 OffsetMatrix;
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


	void Release() {
		if (isImported && Scene) {
			aiReleaseImport(Scene);
			Scene = nullptr;
			Animation = nullptr;
		}
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

	// ----------------------------
	// basic model info
	// ----------------------------
	std::string FilePath = "";
	bool isBlender = false;
	bool SetTexture = false;

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
	std::vector<BONE> m_Bones;
	std::unordered_map<std::string, uint32_t> m_BoneIndexMap;

	bool enableRootMotion = false;

	// ----------------------------
	// animation
	// ----------------------------
	std::unordered_map<std::string, AnimationData> m_Animation;

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
