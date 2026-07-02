#pragma once

#include <array>
#include <cstdint>

#include "Shader/commonDefine.h"
#include "System/Render/RenderSystem/RenderPacket/StaticBatchInstanceDataBuffer.h"

static_assert(GBufferParamChannel_SceneID == 0);
static_assert(GBufferParamChannel_ObjectID == 1);
static_assert(GBufferParamChannel_ShaderID == 2);
static_assert(GBufferParamChannel_MaterialFlags == 3);
static_assert(StaticInstanceObjectChannel_EntityIndex == 0);
static_assert(StaticInstanceObjectChannel_EntityGeneration == 1);
static_assert(StaticInstanceObjectChannel_SceneID == 2);
static_assert(StaticInstanceObjectChannel_Reserved == 3);

struct StaticBatchPickingParameter {
	std::array<std::uint32_t, 4> channels{};

	std::uint32_t SceneID() const noexcept {
		return channels[GBufferParamChannel_SceneID];
	}

	std::uint32_t ObjectID() const noexcept {
		return channels[GBufferParamChannel_ObjectID];
	}

	std::uint32_t ShaderID() const noexcept {
		return channels[GBufferParamChannel_ShaderID];
	}

	std::uint32_t MaterialFlags() const noexcept {
		return channels[GBufferParamChannel_MaterialFlags];
	}
};

namespace StaticBatchPickingContract {

inline StaticBatchPickingParameter Build(
	const StaticBatchInstanceData& instance,
	std::uint32_t shaderID,
	std::uint32_t materialFlags
) noexcept {
	StaticBatchPickingParameter parameter;
	parameter.channels[GBufferParamChannel_SceneID] =
		instance.sceneContextID;
	parameter.channels[GBufferParamChannel_ObjectID] =
		instance.entityIndex;
	parameter.channels[GBufferParamChannel_ShaderID] = shaderID;
	parameter.channels[GBufferParamChannel_MaterialFlags] = materialFlags;
	return parameter;
}

} // namespace StaticBatchPickingContract
