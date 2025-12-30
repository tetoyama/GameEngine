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

//変形後頂点構造体
struct DEFORM_VERTEX {
	aiVector3D Position;
	aiVector3D Normal;
	int				BoneNum = 0;
	std::string		BoneName[4];//本来はボーンインデックスで管理するべき
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
		OutputDebugStringA(("Created ModelData" + FilePath + "\n").c_str());
	}

	~ModelData(){
		OutputDebugStringA(("Destroyed ModelData: " + FilePath + "\n").c_str());
		Release();
	}

	void Release();

	// ファイルパスとインポート設定
	std::string FilePath = "";
	bool isBlender = false;
	bool SetTexture = false;

	// Assimpシーン
	const aiScene* AiScene = nullptr;

	// メッシュごとの頂点・インデックスバッファ（既存描画用）
	std::vector<ID3D11Buffer*>VertexBuffer;
	std::vector<ID3D11Buffer*>IndexBuffer;

	// テクスチャ群
	std::unordered_map<std::string, ID3D11ShaderResourceView*> m_Texture;

	std::vector<DEFORM_VERTEX>* m_DeformVertex = nullptr;
	std::unordered_map<std::string, BONE> m_Bone;

	void CreateBone(aiNode* Node);
	void UpdateBoneMatrix(aiNode* Node, aiMatrix4x4 Matrix);

	void LoadAnimation(const char* FileName, const char* Name);
	void RemoveAnimation(const std::string& name) {
		auto it = m_Animation.find(name);
		if (it != m_Animation.end()) {
			it->second.Release();  // Assimpメモリを開放
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

	std::unordered_map<std::string, AnimationData> m_Animation;
};
