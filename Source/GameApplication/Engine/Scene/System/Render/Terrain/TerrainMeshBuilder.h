// =======================================================================
//
// TerrainMeshBuilder.h
//
// Step 17-D: Terrainメッシュ生成の「CPU Buildタスク（AnyWorker）」側。
// AnimationPoseEvaluator相当の純CPUヘルパで、D3D11には一切触れない。
// グリッド頂点/インデックス生成とComputeNormalsAndTangentsをここへ移設し、
// 出力はstaging（vector）へ書き込む。
//
// =======================================================================
#pragma once

#include <bit>
#include <cstdint>
#include <vector>

#include <DirectXMath.h>

#include "Shader/Common.hlsl"

namespace TerrainMeshBuilder {

// ------------------------------------------------------------------
// Build Signature（AnimationInputRevision相当）
// Scale と HeightMap 内容から世代値を求め、Build後にScale/HeightMapが
// 変化していないかをUpload側で照合するために使う。
// ------------------------------------------------------------------
inline void HashByte(std::uint64_t& hash, std::uint8_t value) noexcept {
	hash ^= value;
	hash *= 1099511628211ull;
}

inline void HashUint32(std::uint64_t& hash, std::uint32_t value) noexcept {
	for(unsigned shift = 0; shift < 32; shift += 8){
		HashByte(hash, static_cast<std::uint8_t>(value >> shift));
	}
}

inline std::uint64_t ComputeSignature(
	int scale,
	const std::vector<float>& heightMap
) noexcept {
	std::uint64_t hash = 14695981039346656037ull;
	HashUint32(hash, std::bit_cast<std::uint32_t>(scale));
	HashUint32(hash, static_cast<std::uint32_t>(heightMap.size()));
	for(const float height : heightMap){
		HashUint32(hash, std::bit_cast<std::uint32_t>(height));
	}
	return hash == 0 ? 1 : hash;
}

// ------------------------------------------------------------------
// 法線・タンジェント計算（旧TerrainSystem::ComputeNormalsAndTangentsを移設）
// ------------------------------------------------------------------
inline void ComputeNormalsAndTangents(
	std::vector<VERTEX_3D>& vertices,
	const std::vector<std::uint32_t>& indices,
	bool invertNormals = false
){
	// 法線をゼロ初期化（念のため）
	for(auto& v : vertices){
		v.Normal = { 0.0f, 0.0f, 0.0f };
		v.Tangent = { 0.0f, 0.0f, 0.0f };
	}

	const size_t indexCount = indices.size();
	for(size_t i = 0; i + 2 < indexCount; i += 3){
		std::uint32_t i0 = indices[i + 0];
		std::uint32_t i1 = indices[i + 1];
		std::uint32_t i2 = indices[i + 2];

		// 頂点読み出し
		DirectX::XMVECTOR p0 = DirectX::XMLoadFloat3(&vertices[i0].Position);
		DirectX::XMVECTOR p1 = DirectX::XMLoadFloat3(&vertices[i1].Position);
		DirectX::XMVECTOR p2 = DirectX::XMLoadFloat3(&vertices[i2].Position);

		// 辺ベクトル（順序によって法線の向きが決まる）
		DirectX::XMVECTOR e1 = DirectX::XMVectorSubtract(p1, p0);
		DirectX::XMVECTOR e2 = DirectX::XMVectorSubtract(p2, p0);

		// 面法線（面積に比例する大きさ）
		DirectX::XMVECTOR faceNormal = DirectX::XMVector3Cross(e1, e2);

		if(invertNormals){
			faceNormal = DirectX::XMVectorNegate(faceNormal);
		}

		// faceNormal を各頂点に加算（面積重み付け）
		DirectX::XMFLOAT3 fn;
		DirectX::XMStoreFloat3(&fn, faceNormal);

		vertices[i0].Normal.x += fn.x;
		vertices[i0].Normal.y += fn.y;
		vertices[i0].Normal.z += fn.z;

		vertices[i1].Normal.x += fn.x;
		vertices[i1].Normal.y += fn.y;
		vertices[i1].Normal.z += fn.z;

		vertices[i2].Normal.x += fn.x;
		vertices[i2].Normal.y += fn.y;
		vertices[i2].Normal.z += fn.z;

		// 簡易タンジェント（必要なら詳細計算に置換）
		DirectX::XMFLOAT3 tan;
		DirectX::XMStoreFloat3(&tan, e1);
		vertices[i0].Tangent.x += tan.x;
		vertices[i0].Tangent.y += tan.y;
		vertices[i0].Tangent.z += tan.z;

		vertices[i1].Tangent.x += tan.x;
		vertices[i1].Tangent.y += tan.y;
		vertices[i1].Tangent.z += tan.z;

		vertices[i2].Tangent.x += tan.x;
		vertices[i2].Tangent.y += tan.y;
		vertices[i2].Tangent.z += tan.z;
	}

	// 正規化（長さが小さい場合は上方向を代入）
	for(auto& v : vertices){
		DirectX::XMVECTOR n = DirectX::XMLoadFloat3(&v.Normal);
		float len = DirectX::XMVectorGetX(DirectX::XMVector3Length(n));
		if(len < 1e-6f){
			v.Normal = { 0.0f, 1.0f, 0.0f };
		}else{
			n = DirectX::XMVector3Normalize(n);
			DirectX::XMStoreFloat3(&v.Normal, n);
		}

		DirectX::XMVECTOR t = DirectX::XMLoadFloat3(&v.Tangent);
		float tlen = DirectX::XMVectorGetX(DirectX::XMVector3Length(t));
		if(tlen < 1e-6f){
			v.Tangent = { 1.0f, 0.0f, 0.0f };
		}else{
			t = DirectX::XMVector3Normalize(t);
			DirectX::XMStoreFloat3(&v.Tangent, t);
		}
	}
}

// ------------------------------------------------------------------
// グリッドメッシュ生成（旧TerrainSystem::CreateMeshのCPU部分を移設）
// D3D11には触れず、staging用のvertices/indicesを構築する。
// scale<=0 など生成不能な場合は false を返す。
// ------------------------------------------------------------------
inline bool Build(
	int scale,
	const std::vector<float>& heightMap,
	std::vector<VERTEX_3D>& outVertices,
	std::vector<std::uint32_t>& outIndices
){
	if(scale <= 0){
		outVertices.clear();
		outIndices.clear();
		return false;
	}

	const int gridSize = scale;
	const int vertexCount = (gridSize + 1) * (gridSize + 1);
	const int indexCount = gridSize * gridSize * 6;

	outVertices.assign(vertexCount, VERTEX_3D{});
	outIndices.assign(indexCount, 0u);

	const float halfSize = gridSize * 0.5f;

	// 頂点生成
	for(int z = 0; z <= gridSize; ++z){
		for(int x = 0; x <= gridSize; ++x){
			int i = z * (gridSize + 1) + x;
			float vx = (x - halfSize) / (float)gridSize;
			float vz = (z - halfSize) / (float)gridSize;
			float vy = 0.0f;
			if((int)heightMap.size() >= vertexCount){
				// HeightMap の行方向をどちらに定義しているかでインデックスを調整すること
				vy = heightMap[x + (gridSize - z) * (gridSize + 1)];
			}
			outVertices[i].Position = { vx, vy, vz };
			outVertices[i].Normal = { 0.0f, 0.0f, 0.0f };
			outVertices[i].Tangent = { 1.0f, 0.0f, 0.0f };
			outVertices[i].TexCoord = { (float)x / gridSize, (float)z / gridSize };
			outVertices[i].Diffuse = { 1, 1, 1, 1 };
		}
	}

	// インデックス生成（winding: CCW を採用）
	int idx = 0;
	for(int z = 0; z < gridSize; ++z){
		for(int x = 0; x < gridSize; ++x){
			int tl = z * (gridSize + 1) + x;
			int tr = tl + 1;
			int bl = (z + 1) * (gridSize + 1) + x;
			int br = bl + 1;

			// 三角形 1 : tl, bl, tr
			outIndices[idx++] = static_cast<std::uint32_t>(tl);
			outIndices[idx++] = static_cast<std::uint32_t>(bl);
			outIndices[idx++] = static_cast<std::uint32_t>(tr);

			// 三角形 2 : tr, bl, br
			outIndices[idx++] = static_cast<std::uint32_t>(tr);
			outIndices[idx++] = static_cast<std::uint32_t>(bl);
			outIndices[idx++] = static_cast<std::uint32_t>(br);
		}
	}

	// 法線・タンジェント計算
	ComputeNormalsAndTangents(outVertices, outIndices, /*invertNormals*/ false);
	return true;
}

} // namespace TerrainMeshBuilder
