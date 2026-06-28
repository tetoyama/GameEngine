#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "Service/Graphics/RHI/RHIDescriptors.h"
#include "StaticBatchInstanceDataBuffer.h"

namespace StaticBatchInstanceInputLayout {

inline constexpr std::uint32_t DefaultInputSlot = 1;
inline constexpr std::uint32_t InstanceStepRate = 1;

static_assert(offsetof(StaticBatchInstanceData, worldMatrix) == 0);
static_assert(offsetof(StaticBatchInstanceData, entityIndex) == 64);
static_assert(offsetof(StaticBatchInstanceData, entityGeneration) == 68);
static_assert(offsetof(StaticBatchInstanceData, sceneContextID) == 72);
static_assert(offsetof(StaticBatchInstanceData, reserved) == 76);
static_assert(sizeof(StaticBatchInstanceData) == 80);

inline void Append(
	std::vector<RHI::InputElementDesc>& layout,
	std::uint32_t inputSlot = DefaultInputSlot
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

inline std::vector<RHI::InputElementDesc> Build(
	std::uint32_t inputSlot = DefaultInputSlot
){
	std::vector<RHI::InputElementDesc> layout;
	layout.reserve(5);
	Append(layout, inputSlot);
	return layout;
}

} // namespace StaticBatchInstanceInputLayout
