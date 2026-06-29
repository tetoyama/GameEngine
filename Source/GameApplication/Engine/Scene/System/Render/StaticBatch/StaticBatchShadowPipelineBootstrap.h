#pragma once

#include <cstdint>
#include <string>

#include "System/Render/StaticBatch/StaticBatchPipelineResources.h"
#include "System/Render/StaticBatch/StaticBatchShaderByteCodeLoader.h"
#include "System/Render/StaticBatch/StaticBatchShadowPipelineResources.h"

enum class StaticBatchShadowPipelineBootstrapResult : std::uint8_t {
	NotAttempted,
	Success,
	AlreadyInitialized,
	InvalidDevice,
	MissingSharedVertexShader,
	ShaderByteCodeLoadFailed,
	PipelineCreationFailed
};

namespace StaticBatchShadowPipelineBootstrap {

inline constexpr const char* DefaultVertexShaderPath =
	"Asset\\Shader\\StaticBatchVS.cso";
inline constexpr const char* DefaultPixelShaderPath =
	"Asset\\Shader\\shadowPS.cso";

inline StaticBatchShadowPipelineBootstrapResult Initialize(
	RHI::IRHIDevice* device,
	const StaticBatchPipelineResources& sharedResources,
	StaticBatchShadowPipelineResources& shadowResources,
	const std::string& vertexShaderPath = DefaultVertexShaderPath,
	const std::string& pixelShaderPath = DefaultPixelShaderPath
){
	if(!device){
		return StaticBatchShadowPipelineBootstrapResult::InvalidDevice;
	}
	if(shadowResources.IsReady() || shadowResources.IsAllocated()){
		return StaticBatchShadowPipelineBootstrapResult::AlreadyInitialized;
	}
	if(!sharedResources.IsReady() || !sharedResources.VertexShader()){
		return StaticBatchShadowPipelineBootstrapResult::MissingSharedVertexShader;
	}

	StaticBatchShaderByteCodeLoader byteCode;
	if(!byteCode.Load(vertexShaderPath, pixelShaderPath)){
		return StaticBatchShadowPipelineBootstrapResult::ShaderByteCodeLoadFailed;
	}
	if(!shadowResources.Create(
		*device,
		sharedResources.VertexShader(),
		byteCode.PixelShader()
	)){
		return StaticBatchShadowPipelineBootstrapResult::PipelineCreationFailed;
	}
	return StaticBatchShadowPipelineBootstrapResult::Success;
}

} // namespace StaticBatchShadowPipelineBootstrap
