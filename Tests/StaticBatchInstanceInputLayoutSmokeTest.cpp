#include <cassert>
#include <cstddef>

#include "Engine/Scene/System/Render/RenderSystem/RenderPacket/StaticBatchInstanceInputLayout.h"

int main(){
	static_assert(sizeof(VERTEX_3D) == 60);
	static_assert(offsetof(VERTEX_3D, Position) == 0);
	static_assert(offsetof(VERTEX_3D, Normal) == 12);
	static_assert(offsetof(VERTEX_3D, Tangent) == 24);
	static_assert(offsetof(VERTEX_3D, Diffuse) == 36);
	static_assert(offsetof(VERTEX_3D, TexCoord) == 52);
	static_assert(sizeof(StaticBatchInstanceData) == 80);
	static_assert(offsetof(StaticBatchInstanceData, entityIndex) == 64);

	const auto layout = StaticBatchInstanceInputLayout::BuildFull(2, 3);
	assert(layout.size() == 10);

	const char* vertexSemantics[] = {
		"POSITION", "NORMAL", "TANGENT", "COLOR", "TEXCOORD"
	};
	const RHI::Format vertexFormats[] = {
		RHI::Format::RGB32_Float,
		RHI::Format::RGB32_Float,
		RHI::Format::RGB32_Float,
		RHI::Format::RGBA32_Float,
		RHI::Format::RG32_Float
	};
	const std::uint32_t vertexOffsets[] = {0, 12, 24, 36, 52};
	for(std::size_t index = 0; index < 5; ++index){
		const RHI::InputElementDesc& element = layout[index];
		assert(element.semanticName == vertexSemantics[index]);
		assert(element.semanticIndex == 0);
		assert(element.format == vertexFormats[index]);
		assert(element.inputSlot == 2);
		assert(element.alignedByteOffset == vertexOffsets[index]);
		assert(!element.perInstance);
		assert(element.instanceStepRate == 0);
	}

	for(std::size_t row = 0; row < 4; ++row){
		const RHI::InputElementDesc& element = layout[5 + row];
		assert(element.semanticName == "INSTANCEWORLD");
		assert(element.semanticIndex == row);
		assert(element.format == RHI::Format::RGBA32_Float);
		assert(element.inputSlot == 3);
		assert(element.alignedByteOffset == row * 16);
		assert(element.perInstance);
		assert(element.instanceStepRate == 1);
	}

	const RHI::InputElementDesc& objectInfo = layout[9];
	assert(objectInfo.semanticName == "INSTANCEOBJECT");
	assert(objectInfo.semanticIndex == 0);
	assert(objectInfo.format == RHI::Format::RGBA32_UInt);
	assert(objectInfo.inputSlot == 3);
	assert(objectInfo.alignedByteOffset == 64);
	assert(objectInfo.perInstance);
	assert(objectInfo.instanceStepRate == 1);

	const auto instanceOnly =
		StaticBatchInstanceInputLayout::BuildInstanceOnly(4);
	assert(instanceOnly.size() == 5);
	assert(instanceOnly.front().inputSlot == 4);
	return 0;
}
