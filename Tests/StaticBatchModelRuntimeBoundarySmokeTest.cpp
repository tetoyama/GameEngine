#include <cassert>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

#include "Engine/Scene/System/Render/StaticBatch/StaticBatchGeometryBindingCache.h"
#include "Engine/Scene/System/Render/StaticBatch/StaticBatchModelGeometryRuntimeStorage.h"
#include "Engine/Scene/System/Render/StaticBatch/StaticBatchModelGeometrySourceProvider.h"
#include "Engine/Scene/System/Render/StaticBatch/StaticBatchModelGeometrySourceResolver.h"

namespace {

std::string ReadTextFile(const char* path){
	std::ifstream stream(path, std::ios::binary);
	assert(stream && "contract source file must exist");
	return std::string(
		std::istreambuf_iterator<char>(stream),
		std::istreambuf_iterator<char>()
	);
}

void ValidateModelRuntimeBoundary(){
	const std::string component = ReadTextFile(
		"Source/GameApplication/Engine/Scene/Component/modelRendererComponent.h"
	);
	const std::string modelData = ReadTextFile(
		"Source/GameApplication/Engine/Resources/Data/modelData.h"
	);
	const std::string modelLoader = ReadTextFile(
		"Source/GameApplication/Engine/Resources/Loader/modelLoader.h"
	);
	const std::string resourceKey = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/RenderSystem/RenderPacket/StaticBatchResourceKey.h"
	);
	const std::string source = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/StaticBatch/StaticBatchD3D11GeometrySource.h"
	);
	const std::string binding = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/StaticBatch/StaticBatchD3D11GeometryBinding.h"
	);
	const std::string provider = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/StaticBatch/StaticBatchModelGeometrySourceProvider.h"
	);
	const std::string runtimeStorage = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/StaticBatch/StaticBatchModelGeometryRuntimeStorage.h"
	);
	const std::string resolver = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/StaticBatch/StaticBatchModelGeometrySourceResolver.h"
	);
	const std::string cache = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/StaticBatch/StaticBatchGeometryBindingCache.h"
	);
	const std::string uploadSystem = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/StaticBatch/StaticBatchUploadSystem.h"
	);

	assert(component.find("dynamicVertexBuffers") == std::string::npos);
	assert(resolver.find("dynamicVertexBuffers") == std::string::npos);
	assert(resolver.find("UsesDynamicVertexBuffer") == std::string::npos);

	// Model Loaderは一時new[]ではなくBackend非依存CPU Geometry Snapshotを生成し、
	// 既存通常描画用D3D11 Bufferも同じSnapshotから初期化する。
	assert(modelData.find("struct ModelMeshGeometryCpuData") !=
		std::string::npos);
	assert(modelData.find("std::vector<ModelMeshGeometryCpuData> MeshGeometry;") !=
		std::string::npos);
	assert(modelLoader.find("model->MeshGeometry.resize") != std::string::npos);
	assert(modelLoader.find("geometry.vertices.resize") != std::string::npos);
	assert(modelLoader.find("geometry.indices.resize") != std::string::npos);
	assert(modelLoader.find("sd.pSysMem = geometry.vertices.data();") !=
		std::string::npos);
	assert(modelLoader.find("sd.pSysMem = geometry.indices.data();") !=
		std::string::npos);
	assert(modelLoader.find("new VERTEX_3D[") == std::string::npos);
	assert(modelLoader.find("new unsigned int[") == std::string::npos);

	// Geometry Resource KeyはNative Buffer配置ではなくCPU Snapshotを識別する。
	assert(resourceKey.find("model->MeshGeometry.size()") != std::string::npos);
	assert(resourceKey.find("geometry->vertices.size()") != std::string::npos);
	assert(resourceKey.find("geometry->indices.size()") != std::string::npos);
	assert(resourceKey.find("model->VertexBuffer.size()") == std::string::npos);
	assert(resourceKey.find("model->IndexBuffer.size()") == std::string::npos);
	assert(resourceKey.find(
		"model->MeshGeometry.size() != model->AiScene->mNumMeshes"
	) != std::string::npos);

	// Geometry SourceはCPU byte spanを第一経路とし、Native Bufferは互換Fallbackだけ。
	assert(source.find("std::span<const std::byte> vertexData") !=
		std::string::npos);
	assert(source.find("std::span<const std::byte> indexData") !=
		std::string::npos);
	assert(source.find("bool HasCpuData() const noexcept") != std::string::npos);
	assert(source.find("HasCpuData() || HasNativeBuffers()") !=
		std::string::npos);

	// Model Provider / Runtime StorageはModelData Native Bufferを参照しない。
	assert(provider.find("StaticBatchModelCpuGeometrySourceProvider") !=
		std::string::npos);
	assert(provider.find("std::as_bytes") != std::string::npos);
	assert(provider.find("model->VertexBuffer") == std::string::npos);
	assert(provider.find("model->IndexBuffer") == std::string::npos);
	assert(runtimeStorage.find("model->VertexBuffer") == std::string::npos);
	assert(runtimeStorage.find("model->IndexBuffer") == std::string::npos);
	assert(runtimeStorage.find("ComPtr<ID3D11Buffer>") == std::string::npos);
	assert(runtimeStorage.find("std::vector<std::byte> vertexData") !=
		std::string::npos);
	assert(runtimeStorage.find("std::vector<std::byte> indexData") !=
		std::string::npos);

	// RHI Geometry BindingはCPU SourceからImmutable Bufferを直接生成する。
	assert(binding.find("CreateFromCpuData") != std::string::npos);
	assert(binding.find("device.CreateBuffer(vertexDesc, source.vertexData)") !=
		std::string::npos);
	assert(binding.find("device.CreateBuffer(indexDesc, source.indexData)") !=
		std::string::npos);
	assert(binding.find("RHI::ResourceUsage::Immutable") != std::string::npos);
	assert(binding.find("WasCreatedFromCpuData") != std::string::npos);
	assert(binding.find("CreateFromNativeBuffers") != std::string::npos);

	// Resolver / CacheはModelDataのNative Buffer配置を知らず、Provider境界だけを使う。
	assert(resolver.find("model->VertexBuffer") == std::string::npos);
	assert(resolver.find("model->IndexBuffer") == std::string::npos);
	assert(resolver.find("IStaticBatchModelGeometrySourceProvider") !=
		std::string::npos);
	assert(cache.find("IStaticBatchModelGeometrySourceProvider") !=
		std::string::npos);
	assert(cache.find("sourceProvider") != std::string::npos);

	// Geometry KeyはPacket Build済みGroupからProviderへ渡し、Provider / Runtime
	// Storage内でModelDataを再走査して生成し直さない。
	assert(provider.find("expectedGeometryResourceKey") != std::string::npos);
	assert(runtimeStorage.find("expectedGeometryResourceKey") !=
		std::string::npos);
	assert(resolver.find(
		"sourceProvider.Resolve(*renderer, packet, group.key.geometryKey)"
	) != std::string::npos);
	assert(provider.find("StaticBatchResourceKey::MakeGeometryKey") ==
		std::string::npos);
	assert(runtimeStorage.find("StaticBatchResourceKey::MakeGeometryKey") ==
		std::string::npos);

	// Runtime StorageはCPU Snapshotを複製し、同期単位で未使用Entryを解放する。
	assert(runtimeStorage.find("StaticBatchRuntimeModelGeometrySourceProvider") !=
		std::string::npos);
	assert(runtimeStorage.find("StaticBatchModelGeometrySourceProviders::ModelCpuData()") !=
		std::string::npos);
	assert(runtimeStorage.find("bootstrapProvider.Resolve") != std::string::npos);
	assert(runtimeStorage.find("BeginSynchronization") != std::string::npos);
	assert(runtimeStorage.find("EndSynchronization") != std::string::npos);

	// Upload SystemがRuntime ProviderをCacheへ注入し、Scheduler競合へ公開する。
	assert(uploadSystem.find("StaticBatchRuntimeModelGeometrySourceProvider") !=
		std::string::npos);
	assert(uploadSystem.find(
		"WriteResource<StaticBatchModelGeometryRuntimeStorage>()"
	) != std::string::npos);
	assert(uploadSystem.find("m_modelGeometrySourceProvider") != std::string::npos);
	assert(uploadSystem.find("m_modelGeometrySourceProvider.Reset()") !=
		std::string::npos);

	// Animation設定のあるModelはGroup単位で拒否し、
	// Boneを持つSubMeshはCPU Providerで拒否する。
	assert(resolver.find("renderer->blendedAnimations.empty()") !=
		std::string::npos);
	assert(provider.find("mesh->HasBones()") != std::string::npos);
}

void ValidateCpuSnapshotGeometryKey(){
	// ModelDataのDestructor/ReleaseはこのHeader-only契約TestではLinkしないため、
	// no-op deleterでCPU Snapshotだけを検証する。
	std::shared_ptr<ModelData> model(
		new ModelData(),
		[](ModelData*){}
	);
	aiMesh mesh{};
	mesh.mMaterialIndex = 3;
	aiMesh* meshes[] = {&mesh};
	aiScene scene{};
	scene.mNumMeshes = 1;
	scene.mMeshes = meshes;
	model->AiScene = &scene;
	model->MeshGeometry.resize(1);
	model->MeshGeometry[0].vertices.resize(3);
	model->MeshGeometry[0].indices = {0u, 1u, 2u};

	ModelRendererComponent renderer;
	renderer.model = model;
	renderer.modelFilePath = "Asset/Model/CpuSnapshot.fbx";
	renderer.modelRuntimeRevision = 4;

	RenderPacket packet;
	packet.kind = RenderPacketKind::Model;
	packet.subMeshIndex = 0;
	packet.bindings.modelRenderer = &renderer;

	assert(model->VertexBuffer.empty());
	assert(model->IndexBuffer.empty());
	const std::uint64_t firstKey =
		StaticBatchResourceKey::MakeGeometryKey(packet);
	assert(firstKey != 0);
	assert(StaticBatchResourceKey::MakeGeometryKey(packet) == firstKey);

	model->MeshGeometry[0].indices.push_back(0u);
	const std::uint64_t changedKey =
		StaticBatchResourceKey::MakeGeometryKey(packet);
	assert(changedKey != 0);
	assert(changedKey != firstKey);

	model->MeshGeometry.clear();
	assert(StaticBatchResourceKey::MakeGeometryKey(packet) == 0);
}

void ValidateEmptyRuntimeStorageLifecycle(){
	StaticBatchModelGeometryRuntimeStorage storage;
	assert(storage.EntryCount() == 0);
	storage.BeginSynchronization();
	storage.EndSynchronization();
	const StaticBatchModelGeometryRuntimeStorageTelemetry telemetry =
		storage.Telemetry();
	assert(telemetry.currentEntryCount == 0);
	assert(telemetry.synchronizationCount == 1);
	storage.Reset();
}

} // namespace

int main(){
	ValidateModelRuntimeBoundary();
	ValidateCpuSnapshotGeometryKey();
	ValidateEmptyRuntimeStorageLifecycle();
	return 0;
}
