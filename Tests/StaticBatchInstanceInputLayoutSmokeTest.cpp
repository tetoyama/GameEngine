#include <cassert>
#include <cstddef>

#include "Engine/Scene/System/Render/RenderSystem/RenderPacket/StaticBatchInstanceInputLayout.h"

int main(){
	static_assert(sizeof(StaticBatchInstanceData) == 80);
	static_assert(offsetof(StaticBatchInstanceData, worldMatrix) == 0);
	static_assert(offsetof(StaticBatchInstanceData, entityIndex) == 64);

	const auto layout = StaticBatchInstanceInputLayout::Build(3);
	assert(layout.size() == 5);

	for(std::size_t row = 0; row < 4; ++row){
		const RHI::InputElementDesc& element = layout[row];
		assert(element.semanticName == "INSTANCEWORLD");
		assert(element.semanticIndex == row);
		assert(element.format == RHI::Format::RGBA32_Float);
		assert(element.inputSlot == 3);
		assert(element.alignedByteOffset == row * 16);
		assert(element.perInstance);
		assert(element.instanceStepRate == 1);
	}

	const RHI::InputElementDesc& objectInfo = layout[4];
	assert(objectInfo.semanticName == "INSTANCEOBJECT");
	assert(objectInfo.semanticIndex == 0);
	assert(objectInfo.format == RHI::Format::RGBA32_UInt);
	assert(objectInfo.inputSlot == 3);
	assert(objectInfo.alignedByteOffset == 64);
	assert(objectInfo.perInstance);
	assert(objectInfo.instanceStepRate == 1);

	std::vector<RHI::InputElementDesc> existing;
	RHI::InputElementDesc vertexPosition;
	vertexPosition.semanticName = "POSITION";
	existing.push_back(vertexPosition);
	StaticBatchInstanceInputLayout::Append(existing);
	assert(existing.size() == 6);
	assert(existing.front().semanticName == "POSITION");
	assert(existing[1].inputSlot == StaticBatchInstanceInputLayout::DefaultInputSlot);
	return 0;
}
