#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Resources/Data/modelData.h"
#include "Scene/Component/modelRendererComponent.h"
#include "Service/Graphics/RHI/RHIInterfaces.h"
#include "System/Render/RenderSystem/RenderPacket/RenderPacket.h"

struct ModelGeometryRuntimeMesh {
	RHI::BufferHandle vertexBuffer;
	RHI::BufferHandle indexBuffer;
	std::uint32_t vertexStride = 0;
	std::uint32_t vertexCount = 0;
	std::uint32_t indexCount = 0;
	RHI::IndexFormat indexFormat = RHI::IndexFormat::UInt32;

	bool IsReady() const noexcept {
		return static_cast<bool>(vertexBuffer) &&
			static_cast<bool>(indexBuffer) &&
			vertexStride != 0 &&
			vertexCount != 0 &&
			indexCount != 0;
	}
};

class ModelGeometryRuntime final {
public:
	ModelGeometryRuntime() = default;
	ModelGeometryRuntime(const ModelGeometryRuntime&) = delete;
	ModelGeometryRuntime& operator=(const ModelGeometryRuntime&) = delete;
	ModelGeometryRuntime(ModelGeometryRuntime&&) noexcept = default;
	ModelGeometryRuntime& operator=(ModelGeometryRuntime&&) noexcept = default;

	bool Initialize(
		RHI::IRHIDevice& device,
		const std::shared_ptr<ModelData>& model
	){
		if(!model || model->MeshGeometry.empty()) return false;

		std::vector<ModelGeometryRuntimeMesh> replacement;
		replacement.reserve(model->MeshGeometry.size());
		for(std::size_t meshIndex = 0;
			meshIndex < model->MeshGeometry.size();
			++meshIndex){
			const ModelMeshGeometryCpuData& source = model->MeshGeometry[meshIndex];
			if(!source.IsValid() ||
				source.vertices.size() >
					(static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)())) /
						sizeof(VERTEX_3D) ||
				source.indices.size() >
					(static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)())) /
						sizeof(std::uint32_t)){
				ReleaseMeshes(device, replacement);
				return false;
			}

			RHI::BufferDesc vertexDesc;
			vertexDesc.byteSize = static_cast<std::uint32_t>(
				source.vertices.size() * sizeof(VERTEX_3D)
			);
			vertexDesc.stride = static_cast<std::uint32_t>(sizeof(VERTEX_3D));
			vertexDesc.usage = RHI::ResourceUsage::Immutable;
			vertexDesc.bindFlags = RHI::BufferBindFlags::Vertex;
			vertexDesc.initialState = RHI::ResourceState::VertexBuffer;
			vertexDesc.debugName = "Model Shared Geometry Vertex Buffer";

			ModelGeometryRuntimeMesh mesh;
			mesh.vertexBuffer = device.CreateBuffer(
				vertexDesc,
				std::as_bytes(std::span<const VERTEX_3D>(source.vertices))
			);
			if(!mesh.vertexBuffer){
				ReleaseMeshes(device, replacement);
				return false;
			}

			RHI::BufferDesc indexDesc;
			indexDesc.byteSize = static_cast<std::uint32_t>(
				source.indices.size() * sizeof(std::uint32_t)
			);
			indexDesc.stride = static_cast<std::uint32_t>(sizeof(std::uint32_t));
			indexDesc.usage = RHI::ResourceUsage::Immutable;
			indexDesc.bindFlags = RHI::BufferBindFlags::Index;
			indexDesc.initialState = RHI::ResourceState::IndexBuffer;
			indexDesc.debugName = "Model Shared Geometry Index Buffer";

			mesh.indexBuffer = device.CreateBuffer(
				indexDesc,
				std::as_bytes(std::span<const std::uint32_t>(source.indices))
			);
			if(!mesh.indexBuffer){
				device.DestroyBuffer(mesh.vertexBuffer);
				ReleaseMeshes(device, replacement);
				return false;
			}

			mesh.vertexStride = vertexDesc.stride;
			mesh.vertexCount = static_cast<std::uint32_t>(source.vertices.size());
			mesh.indexCount = static_cast<std::uint32_t>(source.indices.size());
			mesh.indexFormat = RHI::IndexFormat::UInt32;
			replacement.push_back(mesh);
		}

		if(!Release(device)){
			ReleaseMeshes(device, replacement);
			return false;
		}
		m_modelIdentity = model;
		m_meshes = std::move(replacement);
		return IsReady();
	}

	bool Matches(const std::shared_ptr<ModelData>& model) const noexcept {
		const std::shared_ptr<ModelData> identity = m_modelIdentity.lock();
		if(!identity || identity != model ||
			m_meshes.size() != model->MeshGeometry.size()){
			return false;
		}
		for(std::size_t meshIndex = 0; meshIndex < m_meshes.size(); ++meshIndex){
			const ModelMeshGeometryCpuData& source = model->MeshGeometry[meshIndex];
			const ModelGeometryRuntimeMesh& runtime = m_meshes[meshIndex];
			if(!source.IsValid() || !runtime.IsReady() ||
				static_cast<std::size_t>(runtime.vertexCount) != source.vertices.size() ||
				static_cast<std::size_t>(runtime.indexCount) != source.indices.size()){
				return false;
			}
		}
		return true;
	}

	bool Release(RHI::IRHIDevice& device) noexcept {
		const bool released = ReleaseMeshes(device, m_meshes);
		if(released){
			m_meshes.clear();
			m_modelIdentity.reset();
		}
		return released;
	}

	bool IsReady() const noexcept {
		if(m_meshes.empty()) return false;
		for(const ModelGeometryRuntimeMesh& mesh : m_meshes){
			if(!mesh.IsReady()) return false;
		}
		return true;
	}

	const ModelGeometryRuntimeMesh* Mesh(std::size_t meshIndex) const noexcept {
		return meshIndex < m_meshes.size() ? &m_meshes[meshIndex] : nullptr;
	}

	std::size_t MeshCount() const noexcept { return m_meshes.size(); }

private:
	static bool ReleaseMeshes(
		RHI::IRHIDevice& device,
		std::vector<ModelGeometryRuntimeMesh>& meshes
	) noexcept {
		bool released = true;
		for(ModelGeometryRuntimeMesh& mesh : meshes){
			if(mesh.indexBuffer){
				if(device.DestroyBuffer(mesh.indexBuffer)){
					mesh.indexBuffer = {};
				}else{
					released = false;
				}
			}
			if(mesh.vertexBuffer){
				if(device.DestroyBuffer(mesh.vertexBuffer)){
					mesh.vertexBuffer = {};
				}else{
					released = false;
				}
			}
		}
		return released;
	}

	std::weak_ptr<ModelData> m_modelIdentity;
	std::vector<ModelGeometryRuntimeMesh> m_meshes;
};

struct ModelGeometryRuntimeStorageTelemetry {
	std::size_t currentEntryCount = 0;
	std::size_t peakEntryCount = 0;
	std::size_t synchronizationCount = 0;
	std::size_t creationCount = 0;
	std::size_t reuseCount = 0;
	std::size_t replacementCount = 0;
	std::size_t releaseCount = 0;
	std::size_t rejectedModelCount = 0;
};

// ModelData単位で共有するBackend非依存Geometry Runtime。
// Static ModelはVertex / Indexの両方、Animated Modelは共有Indexを使用し、
// Entity単位のDynamic Vertex Runtimeとは所有権を分ける。
class ModelGeometryRuntimeStorage final {
public:
	ModelGeometryRuntimeStorage() = default;
	~ModelGeometryRuntimeStorage() = default;
	ModelGeometryRuntimeStorage(const ModelGeometryRuntimeStorage&) = delete;
	ModelGeometryRuntimeStorage& operator=(const ModelGeometryRuntimeStorage&) = delete;

	void Synchronize(
		RHI::IRHIDevice& device,
		std::span<const RenderPacket> packets,
		std::uint64_t generation
	){
		m_device = &device;
		++m_synchronizationCount;

		for(const RenderPacket& packet : packets){
			if(packet.kind != RenderPacketKind::Model ||
				!packet.bindings.modelRenderer){
				continue;
			}

			const std::shared_ptr<ModelData>& model =
				packet.bindings.modelRenderer->model;
			if(!model || model->MeshGeometry.empty()){
				++m_rejectedModelCount;
				continue;
			}

			Entry& entry = m_entries[model.get()];
			entry.lastUsedGeneration = generation;
			if(entry.lastAttemptGeneration == generation) continue;
			entry.lastAttemptGeneration = generation;

			if(entry.runtime.Matches(model)){
				++m_reuseCount;
				continue;
			}

			const bool hadRuntime = entry.runtime.IsReady();
			if(!entry.runtime.Initialize(device, model)){
				++m_rejectedModelCount;
				continue;
			}
			if(hadRuntime){
				++m_replacementCount;
			}else{
				++m_creationCount;
			}
		}

		for(auto entryIt = m_entries.begin(); entryIt != m_entries.end();){
			if(entryIt->second.lastUsedGeneration == generation){
				++entryIt;
				continue;
			}
			if(entryIt->second.runtime.Release(device)){
				entryIt = m_entries.erase(entryIt);
				++m_releaseCount;
			}else{
				++entryIt;
			}
		}
		m_peakEntryCount = (std::max)(m_peakEntryCount, m_entries.size());
	}

	const ModelGeometryRuntime* Find(const ModelData* model) const noexcept {
		if(!model) return nullptr;
		const auto entryIt = m_entries.find(model);
		return entryIt != m_entries.end() && entryIt->second.runtime.IsReady()
			? &entryIt->second.runtime
			: nullptr;
	}

	ModelGeometryRuntime* Find(const ModelData* model) noexcept {
		return const_cast<ModelGeometryRuntime*>(
			std::as_const(*this).Find(model)
		);
	}

	void Reset() noexcept {
		if(m_device){
			for(auto& [model, entry] : m_entries){
				(void)model;
				entry.runtime.Release(*m_device);
			}
		}
		m_releaseCount += m_entries.size();
		m_entries.clear();
		m_device = nullptr;
	}

	void Abandon() noexcept {
		m_entries.clear();
		m_device = nullptr;
	}

	std::size_t Size() const noexcept { return m_entries.size(); }

	ModelGeometryRuntimeStorageTelemetry Telemetry() const noexcept {
		return {
			m_entries.size(),
			m_peakEntryCount,
			m_synchronizationCount,
			m_creationCount,
			m_reuseCount,
			m_replacementCount,
			m_releaseCount,
			m_rejectedModelCount
		};
	}

	void ResetMetrics() noexcept {
		m_peakEntryCount = m_entries.size();
		m_synchronizationCount = 0;
		m_creationCount = 0;
		m_reuseCount = 0;
		m_replacementCount = 0;
		m_releaseCount = 0;
		m_rejectedModelCount = 0;
	}

private:
	struct Entry {
		ModelGeometryRuntime runtime;
		std::uint64_t lastUsedGeneration = 0;
		std::uint64_t lastAttemptGeneration =
			(std::numeric_limits<std::uint64_t>::max)();
	};

	RHI::IRHIDevice* m_device = nullptr;
	std::unordered_map<const ModelData*, Entry> m_entries;
	std::size_t m_peakEntryCount = 0;
	std::size_t m_synchronizationCount = 0;
	std::size_t m_creationCount = 0;
	std::size_t m_reuseCount = 0;
	std::size_t m_replacementCount = 0;
	std::size_t m_releaseCount = 0;
	std::size_t m_rejectedModelCount = 0;
};
