#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <d3d11.h>
#include <DirectXMath.h>
#include "Shader/CommonBuffer.h"

struct aiScene;

struct Bone {
	std::string name;
	DirectX::XMFLOAT4X4 offsetMatrix;
};

struct AnimationKeyFrame {
	float time;
	std::vector<DirectX::XMFLOAT4X4> boneTransforms;
};

struct AnimationClip {
	std::string name;
	float duration;
	float ticksPerSecond;
	std::vector<AnimationKeyFrame> keyframes;
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
	std::unordered_map<std::string, ID3D11ShaderResourceView*> Texture;

	// --- スキニング関連 ---

	// CPU側 頂点配列（メッシュごと）
	std::vector<std::vector<AnimationVertex>> SkinnedVertexData;

	// GPU側 StructuredBuffer: 入力（頂点情報）→ Compute Shader用
	std::vector<ID3D11Buffer*> InputStructuredBuffers;
	std::vector<ID3D11ShaderResourceView*> InputSRVs;

	// GPU側 StructuredBuffer: 出力（スキニング結果）→ 頂点シェーダ用
	std::vector<ID3D11Buffer*> OutputStructuredBuffers;
	std::vector<ID3D11UnorderedAccessView*> OutputUAVs;
	std::vector<ID3D11ShaderResourceView*> OutputSRVs;

	// アニメーション定数バッファ（毎フレームボーン行列＋頂点数などを更新）
	ID3D11Buffer* AnimationConstantBuffer = nullptr;

	// ボーン情報
	std::vector<Bone> Bones;
	std::unordered_map<std::string, UINT> BoneNameToIndex;

	// アニメーション群
	std::unordered_map<std::string, AnimationClip> Animations;
};
