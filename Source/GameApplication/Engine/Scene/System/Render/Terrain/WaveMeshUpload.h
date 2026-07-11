// =======================================================================
//
// WaveMeshUpload.h
//
// Step 17-E: WaveメッシュのMainThread GPU Upload側。
// Topology交換は一時Bufferへ生成し、両方成功後だけCommitする。
//
// =======================================================================
#pragma once

#include <cstdint>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

#include <d3d11.h>
#include <wrl/client.h>

#include "Graphics/graphicsContext.h"
#include "Scene/Component/meshRendererComponent.h"
#include "Shader/Common.hlsl"

namespace WaveMeshUpload {

inline bool ValidateCounts(
	const std::vector<VERTEX_3D>& vertices,
	const std::vector<std::uint32_t>& indices
) noexcept {
	if(vertices.empty() || indices.empty()) return false;
	if(vertices.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()) ||
		indices.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())){
		return false;
	}
	if(vertices.size() > static_cast<std::size_t>((std::numeric_limits<UINT>::max)()) /
		sizeof(VERTEX_3D)){
		return false;
	}
	if(indices.size() > static_cast<std::size_t>((std::numeric_limits<UINT>::max)()) /
		sizeof(std::uint32_t)){
		return false;
	}
	return true;
}

inline bool ReplaceMesh(
	GraphicsContext& graphics,
	MeshRendererComponent& renderer,
	const std::vector<VERTEX_3D>& vertices,
	const std::vector<std::uint32_t>& indices
){
	ID3D11Device* device = graphics.GetDevice();
	if(!device || !ValidateCounts(vertices, indices)) return false;

	Microsoft::WRL::ComPtr<ID3D11Buffer> newVertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> newIndexBuffer;

	D3D11_BUFFER_DESC vertexDesc{};
	vertexDesc.Usage = D3D11_USAGE_DYNAMIC;
	vertexDesc.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(VERTEX_3D));
	vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	D3D11_SUBRESOURCE_DATA vertexData{};
	vertexData.pSysMem = vertices.data();
	HRESULT result = device->CreateBuffer(
		&vertexDesc,
		&vertexData,
		newVertexBuffer.GetAddressOf()
	);
	if(FAILED(result)) return false;

	D3D11_BUFFER_DESC indexDesc{};
	indexDesc.Usage = D3D11_USAGE_DEFAULT;
	indexDesc.ByteWidth = static_cast<UINT>(indices.size() * sizeof(std::uint32_t));
	indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA indexData{};
	indexData.pSysMem = indices.data();
	result = device->CreateBuffer(
		&indexDesc,
		&indexData,
		newIndexBuffer.GetAddressOf()
	);
	if(FAILED(result)) return false;

	// Commit point: 既存の正常なWaveメッシュはここまで変更しない。
	renderer.mesh.m_VertexBuffer = std::move(newVertexBuffer);
	renderer.mesh.m_IndexBuffer = std::move(newIndexBuffer);
	renderer.mesh.meshCount = static_cast<int>(vertices.size());
	renderer.mesh.indexCount = static_cast<int>(indices.size());
	return true;
}

inline bool UploadVertices(
	GraphicsContext& graphics,
	MeshRendererComponent& renderer,
	const std::vector<VERTEX_3D>& vertices
){
	ID3D11DeviceContext* context = graphics.GetDeviceContext();
	ID3D11Buffer* vertexBuffer = renderer.mesh.m_VertexBuffer.Get();
	if(!context || !vertexBuffer || vertices.empty() ||
		renderer.mesh.meshCount <= 0 ||
		vertices.size() != static_cast<std::size_t>(renderer.mesh.meshCount)){
		return false;
	}

#ifndef NDEBUG
	D3D11_BUFFER_DESC desc{};
	vertexBuffer->GetDesc(&desc);
	const std::size_t requiredBytes = vertices.size() * sizeof(VERTEX_3D);
	if(desc.Usage != D3D11_USAGE_DYNAMIC ||
		(desc.CPUAccessFlags & D3D11_CPU_ACCESS_WRITE) == 0 ||
		desc.ByteWidth != requiredBytes){
		return false;
	}
#endif

	D3D11_MAPPED_SUBRESOURCE mapped{};
	const HRESULT result = context->Map(
		vertexBuffer,
		0,
		D3D11_MAP_WRITE_DISCARD,
		0,
		&mapped
	);
	if(FAILED(result) || !mapped.pData) return false;

	std::memcpy(
		mapped.pData,
		vertices.data(),
		vertices.size() * sizeof(VERTEX_3D)
	);
	context->Unmap(vertexBuffer, 0);
	return true;
}

} // namespace WaveMeshUpload
