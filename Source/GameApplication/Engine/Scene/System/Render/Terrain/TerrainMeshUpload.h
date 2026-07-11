// =======================================================================
//
// TerrainMeshUpload.h
//
// Step 17-D: Terrainメッシュ生成の「GPU Uploadタスク（MainThread）」側。
// AnimationSkinningUpload相当。stagingのCPU頂点/インデックス列から
// D3D11のVertexBuffer / IndexBufferを生成し、meshCount/indexCountを設定する。
//
// 【MainThread専用】meshRendererの確保/CreateBuffer/解放はMainThreadのみで行う。
// Terrain再構築は低頻度なので、現状同様の再生成方式（DEFAULTバッファ作り直し）でよい。
//
// =======================================================================
#pragma once

#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include <d3d11.h>
#include <wrl/client.h>

#include "Graphics/graphicsContext.h"
#include "Shader/Common.hlsl"
#include "Scene/Component/meshRendererComponent.h"

namespace TerrainMeshUpload {

// stagingから新しいVB/IBを一時生成し、両方成功した場合だけrendererへ反映する。
// 失敗時は既存の正常なメッシュを維持し、再試行可能な状態でfalseを返す。
inline bool Upload(
	GraphicsContext& graphics,
	MeshRendererComponent& renderer,
	const std::vector<VERTEX_3D>& vertices,
	const std::vector<std::uint32_t>& indices
){
	ID3D11Device* device = graphics.GetDevice();
	if(!device || vertices.empty() || indices.empty()){
		return false;
	}

	constexpr std::size_t maxByteWidth =
		static_cast<std::size_t>((std::numeric_limits<UINT>::max)());
	if(vertices.size() > maxByteWidth / sizeof(VERTEX_3D) ||
		indices.size() > maxByteWidth / sizeof(std::uint32_t) ||
		vertices.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()) ||
		indices.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())){
		return false;
	}

	const UINT vertexByteWidth =
		static_cast<UINT>(sizeof(VERTEX_3D) * vertices.size());
	const UINT indexByteWidth =
		static_cast<UINT>(sizeof(std::uint32_t) * indices.size());

	Microsoft::WRL::ComPtr<ID3D11Buffer> newVertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> newIndexBuffer;

	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.ByteWidth = vertexByteWidth;

	D3D11_SUBRESOURCE_DATA sd{};
	sd.pSysMem = vertices.data();

	HRESULT hr = device->CreateBuffer(
		&bd,
		&sd,
		newVertexBuffer.GetAddressOf()
	);
	if(FAILED(hr)){
		return false;
	}

	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd.ByteWidth = indexByteWidth;
	sd.pSysMem = indices.data();

	hr = device->CreateBuffer(
		&bd,
		&sd,
		newIndexBuffer.GetAddressOf()
	);
	if(FAILED(hr)){
		return false;
	}

	// Commit point: ここまでは既存メッシュに触れない。
	renderer.mesh.m_VertexBuffer = std::move(newVertexBuffer);
	renderer.mesh.m_IndexBuffer = std::move(newIndexBuffer);
	renderer.mesh.meshCount = static_cast<int>(vertices.size());
	renderer.mesh.indexCount = static_cast<int>(indices.size());
	return true;
}

} // namespace TerrainMeshUpload
