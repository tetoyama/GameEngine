#pragma once

#include <cstdint>

#include "RenderPacketViewCulling.h"
#include "System/Render/RenderSystem/RenderPass/RenderPassContext.h"

namespace ShadowRenderPacketCullingView {

inline std::uint32_t MakeInstanceID(
	CullingViewKind parentKind,
	std::uint32_t parentInstanceID,
	std::uint32_t tileIndex
) noexcept {
	std::uint32_t hash = 2166136261u;
	const auto combine = [&hash](std::uint32_t value){
		hash ^= value;
		hash *= 16777619u;
	};
	combine(static_cast<std::uint32_t>(parentKind));
	combine(parentInstanceID);
	combine(tileIndex + 1u);
	return hash == 0 ? 1u : hash;
}

inline RenderPacketCullingView Build(
	const RenderPassContext& lightContext,
	CullingViewKind parentKind,
	std::uint32_t parentInstanceID,
	std::uint32_t tileIndex,
	bool depthClipEnabled = false
) noexcept {
	RenderPacketCullingView view;
	view.kind = CullingViewKind::Shadow;
	view.instanceID = MakeInstanceID(
		parentKind,
		parentInstanceID,
		tileIndex
	);
	view.viewProjection =
		lightContext.viewMatrix * lightContext.projectionMatrix;

	// Shadow caster culling must be conservative.
	// Point / Spot shadows still render with GPU DepthClipEnable=true, but CPU
	// far-plane culling can drop large or boundary-touching casters before the GPU
	// clips them. That creates a visible band where lighting still reaches the
	// receiver but the shadow caster disappeared from the shadow map.
	view.depthClipEnabled = false;
	return view;
}

} // namespace ShadowRenderPacketCullingView
