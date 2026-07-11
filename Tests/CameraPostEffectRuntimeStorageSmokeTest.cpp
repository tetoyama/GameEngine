#include <cassert>
#include <fstream>
#include <iterator>
#include <string>

#include "Engine/Scene/System/Render/RenderSystem/RenderPass/PostEffect/CameraPostEffectRuntimeStorage.h"

namespace {

std::string ReadTextFile(const char* path){
	std::ifstream stream(path, std::ios::binary);
	assert(stream && "contract source file must exist");
	return std::string(
		std::istreambuf_iterator<char>(stream),
		std::istreambuf_iterator<char>()
	);
}

void ValidateComponentBoundary(){
	const std::string component = ReadTextFile(
		"Source/GameApplication/Engine/Scene/Component/cameraComponent.h"
	);
	assert(component.find("#include <d3d11.h>") == std::string::npos);
	assert(component.find("#include <wrl/client.h>") == std::string::npos);
	assert(component.find("ID3D11Texture2D") == std::string::npos);
	assert(component.find("ID3D11RenderTargetView") == std::string::npos);
	assert(component.find("ID3D11ShaderResourceView") == std::string::npos);
	assert(component.find("ComPtr<") == std::string::npos);
	assert(component.find("CreateTexture(") == std::string::npos);
	assert(component.find("ResizeTexture(") == std::string::npos);
	assert(component.find("Clear(ID3D11DeviceContext") == std::string::npos);
}

void ValidateRuntimeStorage(){
	CameraPostEffectRuntimeStorage storage;

	const CameraPostEffectRuntimeKey cameraAEffect0{1, 10, 0};
	const CameraPostEffectRuntimeKey cameraAEffect1{1, 10, 1};
	const CameraPostEffectRuntimeKey cameraBEffect0{1, 20, 0};

	const std::uint64_t cameraAFrame1 = storage.BeginCamera(1, 10);
	storage.Acquire(cameraAEffect0, cameraAFrame1);
	storage.Acquire(cameraAEffect1, cameraAFrame1);
	storage.EndCamera(cameraAFrame1);
	assert(storage.Size() == 2);
	assert(storage.Contains(cameraAEffect0));
	assert(storage.Contains(cameraAEffect1));

	// Camera AからEffect 0だけが消えた場合、そのCameraの未使用Runtimeだけを破棄する。
	const std::uint64_t cameraAFrame2 = storage.BeginCamera(1, 10);
	storage.Acquire(cameraAEffect1, cameraAFrame2);
	storage.EndCamera(cameraAFrame2);
	assert(storage.Size() == 1);
	assert(!storage.Contains(cameraAEffect0));
	assert(storage.Contains(cameraAEffect1));

	// PostEffectPassは同時に1 Cameraだけを処理するため、Camera切替時は旧Camera Runtimeを破棄する。
	const std::uint64_t cameraBFrame = storage.BeginCamera(1, 20);
	assert(storage.Size() == 0);
	assert(!storage.Contains(cameraAEffect1));
	storage.Acquire(cameraBEffect0, cameraBFrame);
	storage.EndCamera(cameraBFrame);
	assert(storage.Size() == 1);
	assert(storage.Contains(cameraBEffect0));

	storage.Reset();
	assert(storage.Size() == 0);
	assert(!storage.Contains(cameraBEffect0));
}

void ValidatePassOwnershipContract(){
	const std::string passHeader = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/RenderSystem/RenderPass/PostEffect/PostEffectPass.h"
	);
	const std::string passSource = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/RenderSystem/RenderPass/PostEffect/PostEffectPass.cpp"
	);
	const std::string runtimeSource = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/RenderSystem/RenderPass/PostEffect/CameraPostEffectRuntimeStorage.h"
	);

	assert(passHeader.find("CameraPostEffectRuntimeStorage m_cameraRuntime;") !=
		std::string::npos);
	assert(passSource.find("m_cameraRuntime.BeginCamera(") != std::string::npos);
	assert(passSource.find("m_cameraRuntime.Acquire(") != std::string::npos);
	assert(passSource.find("m_cameraRuntime.EndCamera(") != std::string::npos);
	assert(passSource.find("m_cameraRuntime.Reset();") != std::string::npos);
	assert(passSource.find("node.mipLevels = runtime.mipLevels;") !=
		std::string::npos);

	// 新Texture / RTV / SRVがすべて成功する前に既存Runtimeを置換しない。
	const std::size_t textureCreate = runtimeSource.find("device->CreateTexture2D(");
	const std::size_t rtvCreate = runtimeSource.find("device->CreateRenderTargetView(");
	const std::size_t srvCreate = runtimeSource.find("device->CreateShaderResourceView(");
	const std::size_t textureCommit = runtimeSource.find(
		"texture = std::move(newTexture);"
	);
	assert(textureCreate != std::string::npos);
	assert(rtvCreate != std::string::npos);
	assert(srvCreate != std::string::npos);
	assert(textureCommit != std::string::npos);
	assert(textureCreate < textureCommit);
	assert(rtvCreate < textureCommit);
	assert(srvCreate < textureCommit);
}

} // namespace

int main(){
	ValidateComponentBoundary();
	ValidateRuntimeStorage();
	ValidatePassOwnershipContract();
	return 0;
}
