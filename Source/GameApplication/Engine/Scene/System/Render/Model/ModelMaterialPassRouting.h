#pragma once

#include "Scene/Component/materialComponent.h"
#include "System/Render/RenderSystem/RenderPacket/RenderPacket.h"

struct ModelMaterialPassRoutingDecision {
	RenderLayer effectiveLayer = RenderLayer::Opaque3D;
	bool submitGBuffer = false;
	bool submitForward = false;
	bool sortBackToFront = false;
};

namespace ModelMaterialPassRouting {

inline bool RequiresAlphaBlend(const RenderPacket& packet) noexcept {
	if(const MaterialDescriptor* descriptor = packet.modelMaterial.GetDescriptor()){
		if(descriptor->renderState.alphaMode == MaterialAlphaMode::Blend){
			return true;
		}
		if(descriptor->renderState.alphaMode == MaterialAlphaMode::Masked){
			return false;
		}
		return descriptor->parameters.baseColor[3] < 0.999f;
	}
	return packet.bindings.material &&
		packet.bindings.material->Material.BaseColor.w < 0.999f;
}

inline ModelMaterialPassRoutingDecision Resolve(const RenderPacket& packet) noexcept {
	ModelMaterialPassRoutingDecision decision;
	decision.effectiveLayer = packet.layer;
	decision.submitGBuffer = HasRenderPacketPass(
		packet.passMask,
		RenderPacketPassMask::GBuffer
	);
	decision.submitForward = HasRenderPacketPass(
		packet.passMask,
		RenderPacketPassMask::Forward
	);

	if(RequiresAlphaBlend(packet)){
		decision.submitGBuffer = false;
		decision.submitForward = HasRenderPacketPass(
			RenderPacketPassesForKind(packet.kind),
			RenderPacketPassMask::Forward
		);
		if(packet.kind != RenderPacketKind::Sprite &&
			(packet.layer == RenderLayer::Opaque3D ||
			 packet.layer == RenderLayer::Background2D)){
			decision.effectiveLayer = RenderLayer::SortTransparent3D;
		}
	}
	decision.sortBackToFront =
		decision.effectiveLayer == RenderLayer::SortTransparent3D;
	return decision;
}

inline bool ShouldSubmitGBuffer(const RenderPacket& packet) noexcept {
	return Resolve(packet).submitGBuffer;
}

inline bool ShouldSubmitForward(const RenderPacket& packet) noexcept {
	return Resolve(packet).submitForward;
}

} // namespace ModelMaterialPassRouting
