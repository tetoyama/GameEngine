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
	int				BoneNum;
	std::string		BoneName[4];//本来はボーンインデックスで管理するべき
	float			BoneWeight[4];
};

//ボーン構造体
struct BONE {
	aiMatrix4x4 Matrix;
	aiMatrix4x4 AnimationMatrix;
	aiMatrix4x4 OffsetMatrix;
};

struct AnimationData {
	std::string FilePath;
	const aiScene* Scene;
	aiAnimation* Animation;
	bool isImported = true;
};

struct ModelData {
public:
	ModelData(){
		OutputDebugStringA("Created ModelData\n");
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
	ID3D11Buffer** VertexBuffer = nullptr;
	ID3D11Buffer** IndexBuffer = nullptr;

	// テクスチャ群
	std::unordered_map<std::string, ID3D11ShaderResourceView*> m_Texture;

	std::vector<DEFORM_VERTEX>* m_DeformVertex;//変形後頂点データ
	std::unordered_map<std::string, BONE> m_Bone;//ボーンデータ（名前で参照）

	void CreateBone(aiNode* Node);
	void UpdateBoneMatrix(aiNode* Node, aiMatrix4x4 Matrix);

	void LoadAnimation(const char* FileName, const char* Name);
	void Update(const char* AnimationName1, int Frame1, GraphicsContext* pGraphicContext);

	std::unordered_map<std::string, AnimationData> m_Animation;

};
