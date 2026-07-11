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
#include <vector>

#include <d3d11.h>

#include "Graphics/graphicsContext.h"
#include "Shader/Common.hlsl"
#include "Scene/Component/meshRendererComponent.h"

namespace TerrainMeshUpload {

// staging → meshRenderer の VB/IB を作り直す。
// 成功時のみ meshCount/indexCount を更新して true を返す。
// 失敗時は片方でも中途半端に残さないよう両バッファをResetする。
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

	const UINT vertexCount = static_cast<UINT>(vertices.size());
	const UINT indexCount = static_cast<UINT>(indices.size());

	// 既存バッファを解放してから作り直す（低頻度な再生成方式）
	renderer.mesh.m_VertexBuffer.Reset();
	renderer.mesh.m_IndexBuffer.Reset();

	HRESULT hr = S_OK;

	// VertexBuffer
	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.ByteWidth = static_cast<UINT>(sizeof(VERTEX_3D) * vertexCount);

	D3D11_SUBRESOURCE_DATA sd{};
	sd.pSysMem = vertices.data();

	hr = device->CreateBuffer(&bd, &sd, renderer.mesh.m_VertexBuffer.GetAddressOf());
	if(FAILED(hr)){
		renderer.mesh.m_VertexBuffer.Reset();
		return false;
	}

	// IndexBuffer
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd.ByteWidth = static_cast<UINT>(sizeof(std::uint32_t) * indexCount);
	sd.pSysMem = indices.data();

	hr = device->CreateBuffer(&bd, &sd, renderer.mesh.m_IndexBuffer.GetAddressOf());
	if(FAILED(hr)){
		renderer.mesh.m_VertexBuffer.Reset();
		renderer.mesh.m_IndexBuffer.Reset();
		return false;
	}

	renderer.mesh.meshCount = static_cast<int>(vertexCount);
	renderer.mesh.indexCount = static_cast<int>(indexCount);
	return true;
}

} // namespace TerrainMeshUpload
