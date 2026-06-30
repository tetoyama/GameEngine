// =======================================================================
//
// RenderPacketViewCulling.h
//
// =======================================================================
#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include <DirectXMath.h>

#include "CullingFrustumRuntime.h"
#include "CullingVisibilityBuilder.h"
#include "Scene/System/Render/RenderSystem/RenderPacket/RenderPacketBuffer.h"

struct RenderPacketCullingView {
	Entity camera{};
	CullingViewKind kind = CullingViewKind::Custom;
	std::uint32_t instanceID = 0;
	DirectX::XMMATRIX viewProjection = DirectX::XMMatrixIdentity();
	bool depthClipEnabled = true;
};

namespace RenderPacketViewCulling {

inline bool HasStableViewIdentity(
	const RenderPacketCullingView& view
) noexcept {
	if(view.camera) return true;
	return view.kind == CullingViewKind::Shadow && view.instanceID != 0;
}

inline CullingViewKey MakeViewKey(
	std::uint32_t sceneContextID,
	const RenderPacketCullingView& view
) noexcept {
	return {
		sceneContextID,
		view.camera,
		view.kind,
		view.instanceID
	};
}

inline CullingFrustum BuildFrustum(
	const RenderPacketCullingView& view
) noexcept {
	CullingFrustum frustum =
		CullingFrustumRuntime::FromViewProjection(view.viewProjection);
	if(view.kind == CullingViewKind::Shadow && !view.depthClipEnabled){
		// Directional / CSMはDepthClipEnable=falseで描画する。
		// GPUが残すNear/Far範囲外CasterをCPUだけで除外しないよう、
		// Depth Clampを使うShadow Viewでは左右上下Planeだけを使用する。
		frustum.planes[4] = {};
		frustum.planes[5] = {};
	}
	return frustum;
}

inline void Prepare(
	CullingVisibilitySet& visibility,
	const RenderPacketFrameBuffer& packets,
	const RenderPacketCullingView& view
){
	if(visibility.FrameSerial() != packets.Generation()){
		visibility.BeginFrame(packets.Generation());
	}
	if(!HasStableViewIdentity(view)) return;

	const CullingFrustum frustum = BuildFrustum(view);
	std::vector<SceneContext*> preparedScenes;

	for(const RenderPacket& packet : packets.Packets()){
		SceneContext* context = packet.bindings.sceneContext;
		if(!context || !context->component || context->contextID == 0) continue;
		if(std::find(preparedScenes.begin(), preparedScenes.end(), context) !=
			preparedScenes.end()){
			continue;
		}
		preparedScenes.push_back(context);

		const CullingViewKey key = MakeViewKey(context->contextID, view);
		if(visibility.HasView(key)) continue;

		CullingVisibilityBuilder::BuildView(
			visibility,
			key,
			frustum,
			*context->component,
			context->storageConfig.visibleEntityReserve,
			context->storageConfig.allowRuntimeGrowth
		);
	}
}

inline bool ShouldRender(
	const CullingVisibilitySet& visibility,
	const RenderPacketCullingView& view,
	const RenderPacket& packet
){
	SceneContext* context = packet.bindings.sceneContext;
	if(!context || !context->component) return false;
	if(!HasStableViewIdentity(view)) return true;

	return CullingVisibilityBuilder::ShouldRender(
		visibility,
		MakeViewKey(packet.sceneContextID, view),
		packet.entity,
		*context->component
	);
}

} // namespace RenderPacketViewCulling
