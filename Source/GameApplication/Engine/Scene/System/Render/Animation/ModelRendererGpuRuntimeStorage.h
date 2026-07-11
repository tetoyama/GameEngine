// =======================================================================
//
// ModelRendererGpuRuntimeStorage.h
//
// Step 18-A: ModelRendererComponentから動的Vertex Buffer所有権を分離する。
// RenderSystemがScene Context / Entity単位のGPU Runtimeを所有する。
//
// =======================================================================
#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <d3d11.h>
#include <wrl/client.h>

#include "Resources/Data/modelData.h"

struct ModelRendererGpuRuntimeKey {
	std::uint32_t sceneContextID = 0;
	std::uint64_t entity = 0;

	bool operator==(const ModelRendererGpuRuntimeKey&) const noexcept = default;
};

struct ModelRendererGpuRuntimeKeyHash {
	std::size_t operator()(const ModelRendererGpuRuntimeKey& key) const noexcept {
		std::size_t hash = static_cast<std::size_t>(key.sceneContextID);
		hash ^= static_cast<std::size_t>(key.entity) +
			0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
		return hash;
	}
};

class ModelRendererGpuRuntime final {
public:
	bool Ensure(
		ID3D11Device* device,
		const ModelData& model,
		std::uint64_t modelRevision
	){
		if(!device || !model.AiScene || modelRevision == 0){
			return false;
		}

		const std::size_t meshCount = model.AiScene->mNumMeshes;
		if(IsValidFor(modelRevision, meshCount)){
			return true;
		}

		std::vector<Microsoft::WRL::ComPtr<ID3D11Buffer>> newBuffers;
		newBuffers.reserve(meshCount);
		for(std::size_t meshIndex = 0; meshIndex < meshCount; ++meshIndex){
			const aiMesh* mesh = model.AiScene->mMeshes[meshIndex];
			if(!mesh || mesh->mNumVertices == 0){
				return false;
			}

			const std::uint64_t byteWidth =
				static_cast<std::uint64_t>(sizeof(VERTEX_3D)) *
				static_cast<std::uint64_t>(mesh->mNumVertices);
			const std::uint64_t maximumByteWidth =
				static_cast<std::uint64_t>((std::numeric_limits<UINT>::max)());
			if(byteWidth == 0 || byteWidth > maximumByteWidth){
				return false;
			}

			D3D11_BUFFER_DESC description{};
			description.Usage = D3D11_USAGE_DYNAMIC;
			description.ByteWidth = static_cast<UINT>(byteWidth);
			description.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

			Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
			const HRESULT result = device->CreateBuffer(
				&description,
				nullptr,
				buffer.GetAddressOf()
			);
			if(FAILED(result) || !buffer){
				return false;
			}
			newBuffers.push_back(std::move(buffer));
		}

		// Transactional commit: 全MeshのBuffer生成成功後だけ旧Runtimeを置換する。
		m_buffers = std::move(newBuffers);
		m_rawBuffers.clear();
		m_rawBuffers.reserve(m_buffers.size());
		for(const auto& buffer : m_buffers){
			m_rawBuffers.push_back(buffer.Get());
		}
		m_modelRevision = modelRevision;
		return true;
	}

	bool IsValidFor(
		std::uint64_t modelRevision,
		std::size_t meshCount
	) const noexcept {
		if(modelRevision == 0 || m_modelRevision != modelRevision ||
			m_buffers.size() != meshCount || m_rawBuffers.size() != meshCount){
			return false;
		}
		for(std::size_t index = 0; index < meshCount; ++index){
			if(!m_buffers[index] || m_rawBuffers[index] != m_buffers[index].Get()){
				return false;
			}
		}
		return true;
	}

	const std::vector<ID3D11Buffer*>& RawBuffers() const noexcept {
		return m_rawBuffers;
	}

	ID3D11Buffer* Buffer(std::size_t meshIndex) const noexcept {
		return meshIndex < m_rawBuffers.size()
			? m_rawBuffers[meshIndex]
			: nullptr;
	}

	std::uint64_t ModelRevision() const noexcept {
		return m_modelRevision;
	}

private:
	std::vector<Microsoft::WRL::ComPtr<ID3D11Buffer>> m_buffers;
	std::vector<ID3D11Buffer*> m_rawBuffers;
	std::uint64_t m_modelRevision = 0;
};

class ModelRendererGpuRuntimeStorage final {
public:
	ModelRendererGpuRuntime& Acquire(const ModelRendererGpuRuntimeKey& key){
		return m_entries[key];
	}

	ModelRendererGpuRuntime* Find(
		const ModelRendererGpuRuntimeKey& key
	) noexcept {
		auto it = m_entries.find(key);
		return it != m_entries.end() ? &it->second : nullptr;
	}

	const ModelRendererGpuRuntime* Find(
		const ModelRendererGpuRuntimeKey& key
	) const noexcept {
		auto it = m_entries.find(key);
		return it != m_entries.end() ? &it->second : nullptr;
	}

	void Erase(const ModelRendererGpuRuntimeKey& key){
		m_entries.erase(key);
	}

	void Reset() noexcept {
		m_entries.clear();
	}

	std::size_t Size() const noexcept {
		return m_entries.size();
	}

private:
	std::unordered_map<
		ModelRendererGpuRuntimeKey,
		ModelRendererGpuRuntime,
		ModelRendererGpuRuntimeKeyHash
	> m_entries;
};
