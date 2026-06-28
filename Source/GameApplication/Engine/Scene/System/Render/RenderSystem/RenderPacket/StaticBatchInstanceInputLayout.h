#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "Service/Graphics/RHI/RHIDescriptors.h"
#include "Shader/common.hlsl"
#include "StaticBatchInstanceDataBuffer.h"

namespace StaticBatchInstanceInputLayout {

inline constexpr std::uint32_t DefaultVertexSlot = 0;
inline constexpr std::uint32_t DefaultInstanceSlot = 1;
inline constexpr std::uint32_t InstanceStepRate = 1;

static_assert(offsetof(VERTEX_3D, Position) == 0);
static_assert(offsetof(VERTEX_3D, Normal) == 12);
static_assert(offsetof(VERTEX_3D, Tangent) == 24);
static_assert(offsetof(VERTEX_3D, Diffuse) == 36);
static_assert(offsetof(VERTEX_3D, TexCoord) == 52);
static_assert(sizeof(VERTEX_3D) == 60);

static_assert(offsetof(StaticBatchInstanceData, worldMatrix) == 0);
static_assert(offsetof(StaticBatchInstanceData, entityIndex) == 64);
static_assert(offsetof(StaticBatchInstanceData, entityGeneration) == 68);
static_assert(offsetof(StaticBatchInstanceData, sceneContextID) == 72);
static_assert(offsetof(StaticBatchInstanceData, reserved) == 76);
static_assert(sizeof(StaticBatchInstanceData) == 80);

inline void AppendVertexElements(
	std::vector<RHI::InputElementDesc>& layout,
	std::uint32_t inputSlot = DefaultVertexSlot
){
	const auto append = [&](
		const char* semantic,
		RHI::Format format,
		std::uint32_t offset
	){
		RHI::InputElementDesc element;
		element.semanticName = semantic;
		element.semanticIndex = 0;
		element.format = format;
		element.inputSlot = inputSlot;
		element.alignedByteOffset = offset;
		element.perInstance = false;
		element.instanceStepRate = 0;
		layout.push_back(std::move(element));
	};

	append("POSITION", RHI::Format::RGB32_Float,
		static_cast<std::uint32_t>(offsetof(VERTEX_3D, Position)));
	append("NORMAL", RHI::Format::RGB32_Float,
		static_cast<std::uint32_t>(offsetof(VERTEX_3D, Normal)));
	append("TANGENT", RHI::Format::RGB32_Float,
		static_cast<std::uint32_t>(offsetof(VERTEX_3D, Tangent)));
	append("COLOR", RHI::Format::RGBA32_Float,
		static_cast<std::uint32_t>(offsetof(VERTEX_3D, Diffuse)));
	append("TEXCOORD", RHI::Format::RG32_Float,
		static_cast<std::uint32_t>(offsetof(VERTEX_3D, TexCoord)));
}

inline void AppendInstanceElements(
	std::vector<RHI::InputElementDesc>& layout,
	std::uint32_t inputSlot = DefaultInstanceSlot
){
	for(std::uint32_t row = 0; row < 4; ++row){
		RHI::InputElementDesc element;
		element.semanticName = "INSTANCEWORLD";
		element.semanticIndex = row;
		element.format = RHI::Format::RGBA32_Float;
		element.inputSlot = inputSlot;
		element.alignedByteOffset = row * 16;
		element.perInstance = true;
		element.instanceStepRate = InstanceStepRate;
		layout.push_back(std::move(element));
	}

	RHI::InputElementDesc objectInfo;
	objectInfo.semanticName = "INSTANCEOBJECT";
	objectInfo.semanticIndex = 0;
	objectInfo.format = RHI::Format::RGBA32_UInt;
	objectInfo.inputSlot = inputSlot;
	objectInfo.alignedByteOffset = static_cast<std::uint32_t>(
		offsetof(StaticBatchInstanceData, entityIndex)
	);
	objectInfo.perInstance = true;
	objectInfo.instanceStepRate = InstanceStepRate;
	layout.push_back(std::move(objectInfo));
}

inline std::vector<RHI::InputElementDesc> BuildInstanceOnly(
	std::uint32_t inputSlot = DefaultInstanceSlot
){
	std::vector<RHI::InputElementDesc> layout;
	layout.reserve(5);
	AppendInstanceElements(layout, inputSlot);
	return layout;
}

inline std::vector<RHI::InputElementDesc> BuildFull(
	std::uint32_t vertexSlot = DefaultVertexSlot,
	std::uint32_t instanceSlot = DefaultInstanceSlot
){
	std::vector<RHI::InputElementDesc> layout;
	layout.reserve(10);
	AppendVertexElements(layout, vertexSlot);
	AppendInstanceElements(layout, instanceSlot);
	return layout;
}

} // namespace StaticBatchInstanceInputLayout
