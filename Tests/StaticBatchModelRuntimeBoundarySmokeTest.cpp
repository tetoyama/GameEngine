#include <cassert>
#include <fstream>
#include <iterator>
#include <string>

#include "Engine/Scene/System/Render/StaticBatch/StaticBatchGeometryBindingCache.h"
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
	const std::string resolver = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/StaticBatch/StaticBatchModelGeometrySourceResolver.h"
	);
	const std::string cache = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/StaticBatch/StaticBatchGeometryBindingCache.h"
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

	// Legacy Providerだけが移行期間中のModelData Native Bufferを参照する。
	assert(provider.find("class IStaticBatchModelGeometrySourceProvider") !=
		std::string::npos);
	assert(provider.find("StaticBatchLegacyModelGeometrySourceProvider") !=
		std::string::npos);
	assert(provider.find("model->VertexBuffer") != std::string::npos);
	assert(provider.find("model->IndexBuffer") != std::string::npos);

	// Animation設定のあるModelはGroup単位で拒否し、
	// Boneを持つSubMeshはGeometry Source Providerで拒否する。
	assert(resolver.find("renderer->blendedAnimations.empty()") !=
		std::string::npos);
	assert(provider.find("mesh->HasBones()") != std::string::npos);
}

} // namespace

int main(){
	ValidateModelRuntimeBoundary();
	return 0;
}
