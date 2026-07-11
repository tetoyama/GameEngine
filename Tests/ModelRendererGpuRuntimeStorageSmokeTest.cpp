#include <cassert>
#include <fstream>
#include <iterator>
#include <string>

#include "Engine/Scene/System/Render/Animation/ModelRendererGpuRuntimeStorage.h"

namespace {

std::string ReadTextFile(const char* path){
	std::ifstream stream(path, std::ios::binary);
	assert(stream && "contract source file must exist");
	return std::string(
		std::istreambuf_iterator<char>(stream),
		std::istreambuf_iterator<char>()
	);
}

void ValidateStorageKeys(){
	ModelRendererGpuRuntimeStorage storage;
	const ModelRendererGpuRuntimeKey first{1, 100};
	const ModelRendererGpuRuntimeKey second{1, 200};
	const ModelRendererGpuRuntimeKey otherScene{2, 100};

	ModelRendererGpuRuntime& firstRuntime = storage.Acquire(first);
	assert(storage.Size() == 1);
	assert(storage.Find(first) == &firstRuntime);
	assert(storage.Find(second) == nullptr);

	storage.Acquire(second);
	storage.Acquire(otherScene);
	assert(storage.Size() == 3);
	assert(storage.Find(second) != nullptr);
	assert(storage.Find(otherScene) != nullptr);

	storage.Erase(second);
	assert(storage.Size() == 2);
	assert(storage.Find(second) == nullptr);

	storage.Reset();
	assert(storage.Size() == 0);
	assert(storage.Find(first) == nullptr);
	assert(storage.Find(otherScene) == nullptr);
}

void ValidateComponentBoundary(){
	const std::string component = ReadTextFile(
		"Source/GameApplication/Engine/Scene/Component/modelRendererComponent.h"
	);
	const std::string runtime = ReadTextFile(
		"Source/GameApplication/Engine/Scene/Component/Operations/ModelRendererRuntime.h"
	);

	assert(component.find("#include <d3d11.h>") == std::string::npos);
	assert(component.find("ID3D11Buffer") == std::string::npos);
	assert(component.find("dynamicVertexBuffers") == std::string::npos);
	assert(runtime.find("CreateBuffer(") == std::string::npos);
	assert(runtime.find("->Release()") == std::string::npos);
}

void ValidateRenderSystemOwnership(){
	const std::string renderSystemHeader = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/RenderSystem/renderSystem.h"
	);
	const std::string animationTasks = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/Animation/RenderSystemAnimationTasks.inl"
	);
	const std::string renderable = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/RenderSystem/Renderable/Model/RenderableModel.cpp"
	);

	assert(renderSystemHeader.find(
		"ModelRendererGpuRuntimeStorage m_modelRendererGpuRuntime;"
	) != std::string::npos);
	assert(renderSystemHeader.find("m_modelRendererGpuRuntime.Reset();") !=
		std::string::npos);
	assert(animationTasks.find("m_modelRendererGpuRuntime.Acquire(runtimeKey)") !=
		std::string::npos);
	assert(animationTasks.find("runtime.Ensure(") != std::string::npos);
	assert(animationTasks.find("runtime.RawBuffers()") != std::string::npos);
	assert(renderable.find("GetModelRendererGpuRuntime().Find(runtimeKey)") !=
		std::string::npos);
	assert(renderable.find("modelGpuRuntime->Buffer(meshIndex)") !=
		std::string::npos);
}

void ValidateTransactionalCreation(){
	const std::string storage = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/Animation/ModelRendererGpuRuntimeStorage.h"
	);
	const std::size_t create = storage.find("device->CreateBuffer(");
	const std::size_t commit = storage.find("m_buffers = std::move(newBuffers);");
	const std::size_t revisionCommit = storage.find("m_modelRevision = modelRevision;");
	assert(create != std::string::npos);
	assert(commit != std::string::npos);
	assert(revisionCommit != std::string::npos);
	assert(create < commit);
	assert(commit < revisionCommit);
}

} // namespace

int main(){
	ValidateStorageKeys();
	ValidateComponentBoundary();
	ValidateRenderSystemOwnership();
	ValidateTransactionalCreation();
	return 0;
}
