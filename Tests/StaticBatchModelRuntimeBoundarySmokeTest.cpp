#include <cassert>
#include <fstream>
#include <iterator>
#include <string>

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
	const std::string resolver = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/StaticBatch/StaticBatchModelGeometrySourceResolver.h"
	);

	assert(component.find("dynamicVertexBuffers") == std::string::npos);
	assert(resolver.find("dynamicVertexBuffers") == std::string::npos);
	assert(resolver.find("UsesDynamicVertexBuffer") == std::string::npos);

	// Animation設定のあるModelはGroup単位で拒否し、
	// Boneを持つSubMeshはGeometry解決後にも拒否する。
	assert(resolver.find("renderer->blendedAnimations.empty()") !=
		std::string::npos);
	assert(resolver.find("mesh->HasBones()") != std::string::npos);
}

} // namespace

int main(){
	ValidateModelRuntimeBoundary();
	return 0;
}
