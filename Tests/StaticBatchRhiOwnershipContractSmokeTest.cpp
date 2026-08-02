#include <cassert>
#include <fstream>
#include <iterator>
#include <string>

namespace {

std::string ReadTextFile(const char* path){
	std::ifstream stream(path, std::ios::binary);
	assert(stream && "contract source file must exist");
	return std::string(
		std::istreambuf_iterator<char>(stream),
		std::istreambuf_iterator<char>()
	);
}

} // namespace

int main(){
	const std::string rhiInterfaces = ReadTextFile(
		"Source/GameApplication/Service/Graphics/RHI/RHIInterfaces.h"
	);
	const std::string rhiService = ReadTextFile(
		"Source/GameApplication/Service/Graphics/RHI/RHIService.h"
	);
	const std::string uploadSystem = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/StaticBatch/StaticBatchUploadSystem.h"
	);
	const std::string geometryCache = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/StaticBatch/StaticBatchGeometryBindingCache.h"
	);
	const std::string geometryBinding = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/StaticBatch/StaticBatchD3D11GeometryBinding.h"
	);
	const std::string instanceBuffer = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/RenderSystem/RenderPacket/StaticBatchGpuInstanceBuffer.h"
	);
	const std::string pipelineResources = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/StaticBatch/StaticBatchPipelineResources.h"
	);
	const std::string shadowPipelineResources = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/StaticBatch/StaticBatchShadowPipelineResources.h"
	);

	assert(rhiInterfaces.find("using DeviceGeneration") != std::string::npos);
	assert(rhiInterfaces.find("GetLifetimeToken()") != std::string::npos);
	assert(rhiService.find("GetDeviceGeneration()") != std::string::npos);
	assert(rhiService.find("AdvanceDeviceGeneration()") != std::string::npos);

	assert(uploadSystem.find("m_boundDeviceLifetime") != std::string::npos);
	assert(uploadSystem.find("m_boundDeviceGeneration") != std::string::npos);
	assert(uploadSystem.find("ResolveBoundDevice()") != std::string::npos);
	assert(uploadSystem.find("ResolveReleaseDevice()") != std::string::npos);
	assert(uploadSystem.find("AbandonAllGpuResources()") != std::string::npos);
	assert(uploadSystem.find("BootstrapPipelines(*device)") != std::string::npos);
	assert(uploadSystem.find("GetDeviceGeneration()") != std::string::npos);
	assert(uploadSystem.find("ResolveDevice() const") == std::string::npos);

	assert(geometryCache.find("void Abandon() noexcept") != std::string::npos);
	assert(geometryBinding.find("void Abandon() noexcept") != std::string::npos);
	assert(instanceBuffer.find("void Abandon() noexcept") != std::string::npos);
	assert(pipelineResources.find("void Abandon() noexcept") != std::string::npos);
	assert(shadowPipelineResources.find("void Abandon() noexcept") !=
		std::string::npos);

	// Finalize / Stopは、現在のService DeviceとBinding Epochが一致する場合だけ
	// Native Releaseを実行する。異なる場合はAbandonへ分岐する。
	assert(uploadSystem.find(
		"if(RHI::IRHIDevice* device = ResolveReleaseDevice())"
	) != std::string::npos);
	assert(uploadSystem.find("m_geometryBindingCache.Abandon()") !=
		std::string::npos);
	return 0;
}
