#include <cassert>
#include <fstream>
#include <iterator>
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

	// Legacy ProviderだけがModelData Native Bufferから初回Sourceを取り込む。
	assert(provider.find("class IStaticBatchModelGeometrySourceProvider") !=
		std::string::npos);
	assert(provider.find("StaticBatchLegacyModelGeometrySourceProvider") !=
		std::string::npos);
	assert(provider.find("model->VertexBuffer") != std::string::npos);
	assert(provider.find("model->IndexBuffer") != std::string::npos);
	assert(runtimeStorage.find("model->VertexBuffer") == std::string::npos);
	assert(runtimeStorage.find("model->IndexBuffer") == std::string::npos);

	// Runtime Storageは独立COM参照を保持し、同期単位で未使用Entryを解放する。
	assert(runtimeStorage.find("ComPtr<ID3D11Buffer>") != std::string::npos);
	assert(runtimeStorage.find("StaticBatchRuntimeModelGeometrySourceProvider") !=
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
	// Boneを持つSubMeshはBootstrap Providerで拒否する。
	assert(resolver.find("renderer->blendedAnimations.empty()") !=
		std::string::npos);
	assert(provider.find("mesh->HasBones()") != std::string::npos);
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
	ValidateEmptyRuntimeStorageLifecycle();
	return 0;
}
