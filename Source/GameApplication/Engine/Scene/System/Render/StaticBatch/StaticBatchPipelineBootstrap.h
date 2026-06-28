#pragma once

#include <cstdint>
#include <string>

#include "System/Render/StaticBatch/StaticBatchPipelineResources.h"
#include "System/Render/StaticBatch/StaticBatchShaderByteCodeLoader.h"

enum class StaticBatchPipelineBootstrapResult : std::uint8_t {
	NotAttempted,
	Success,
	AlreadyInitialized,
	InvalidDevice,
	ShaderByteCodeLoadFailed,
	PipelineCreationFailed
};

namespace StaticBatchPipelineBootstrap {

inline constexpr const char* DefaultVertexShaderPath =
	"Asset\\Shader\\StaticBatchVS.cso";
inline constexpr const char* DefaultPixelShaderPath =
	"Asset\\Shader\\StaticBatchGBufferPS.cso";

inline StaticBatchPipelineBootstrapResult Initialize(
	RHI::IRHIDevice* device,
	StaticBatchPipelineResources& resources,
	const std::string& vertexShaderPath = DefaultVertexShaderPath,
	const std::string& pixelShaderPath = DefaultPixelShaderPath
){
	if(!device){
		return StaticBatchPipelineBootstrapResult::InvalidDevice;
	}
	if(resources.IsReady() || resources.IsAllocated()){
		return StaticBatchPipelineBootstrapResult::AlreadyInitialized;
	}

	StaticBatchShaderByteCodeLoader byteCode;
	if(!byteCode.Load(vertexShaderPath, pixelShaderPath)){
		return StaticBatchPipelineBootstrapResult::ShaderByteCodeLoadFailed;
	}
	if(!resources.Create(
		*device,
		byteCode.VertexShader(),
		byteCode.PixelShader()
	)){
		return StaticBatchPipelineBootstrapResult::PipelineCreationFailed;
	}
	return StaticBatchPipelineBootstrapResult::Success;
}

} // namespace StaticBatchPipelineBootstrap
