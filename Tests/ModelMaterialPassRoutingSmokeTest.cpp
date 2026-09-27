#include <cassert>
#include <memory>

#include "Engine/Scene/Component/materialComponent.h"
#include "Engine/Scene/System/Render/Model/ModelMaterialPassRouting.h"

namespace {

RenderPacket MakeModelPacket(
	RenderLayer layer,
	RenderPacketPassMask passMask,
	MaterialDescriptor descriptor
){
	RenderPacket packet;
	packet.kind = RenderPacketKind::Model;
	packet.layer = layer;
	packet.passMask = passMask;
	packet.modelMaterial.ownedDescriptor =
		std::make_shared<MaterialDescriptor>(std::move(descriptor));
	packet.modelMaterial.descriptor =
		packet.modelMaterial.ownedDescriptor.get();
	return packet;
}

} // namespace

int main(){
	const RenderPacketPassMask deferredPasses =
		RenderPacketPassMask::Shadow |
		RenderPacketPassMask::GBuffer;

	MaterialDescriptor opaque;
	opaque.renderState.alphaMode = MaterialAlphaMode::Opaque;
	opaque.parameters.baseColor[3] = 1.0f;
	const RenderPacket opaquePacket = MakeModelPacket(
		RenderLayer::Opaque3D,
		deferredPasses,
		opaque
	);
	const ModelMaterialPassRoutingDecision opaqueRouting =
		ModelMaterialPassRouting::Resolve(opaquePacket);
	assert(opaqueRouting.submitGBuffer);
	assert(!opaqueRouting.submitForward);
	assert(!opaqueRouting.sortBackToFront);
	assert(opaqueRouting.effectiveLayer == RenderLayer::Opaque3D);

	MaterialDescriptor blend = opaque;
	blend.renderState.alphaMode = MaterialAlphaMode::Blend;
	const RenderPacket blendPacket = MakeModelPacket(
		RenderLayer::Opaque3D,
		deferredPasses,
		blend
	);
	const ModelMaterialPassRoutingDecision blendRouting =
		ModelMaterialPassRouting::Resolve(blendPacket);
	assert(!blendRouting.submitGBuffer);
	assert(blendRouting.submitForward);
	assert(blendRouting.sortBackToFront);
	assert(blendRouting.effectiveLayer == RenderLayer::SortTransparent3D);
	assert(HasRenderPacketPass(
		blendPacket.passMask,
		RenderPacketPassMask::Shadow
	));

	MaterialDescriptor masked = opaque;
	masked.renderState.alphaMode = MaterialAlphaMode::Masked;
	masked.parameters.baseColor[3] = 0.25f;
	const RenderPacket maskedPacket = MakeModelPacket(
		RenderLayer::Opaque3D,
		deferredPasses,
		masked
	);
	const ModelMaterialPassRoutingDecision maskedRouting =
		ModelMaterialPassRouting::Resolve(maskedPacket);
	assert(maskedRouting.submitGBuffer);
	assert(!maskedRouting.submitForward);
	assert(!maskedRouting.sortBackToFront);

	MaterialDescriptor defensiveOpaque = opaque;
	defensiveOpaque.parameters.baseColor[3] = 0.5f;
	const RenderPacket defensivePacket = MakeModelPacket(
		RenderLayer::Opaque3D,
		deferredPasses,
		defensiveOpaque
	);
	assert(ModelMaterialPassRouting::RequiresAlphaBlend(defensivePacket));
	assert(ModelMaterialPassRouting::ShouldSubmitForward(defensivePacket));
	assert(!ModelMaterialPassRouting::ShouldSubmitGBuffer(defensivePacket));

	const RenderPacket explicitTransparentPacket = MakeModelPacket(
		RenderLayer::Transparent3D,
		RenderPacketPassMask::Shadow | RenderPacketPassMask::Forward,
		blend
	);
	const ModelMaterialPassRoutingDecision explicitTransparentRouting =
		ModelMaterialPassRouting::Resolve(explicitTransparentPacket);
	assert(!explicitTransparentRouting.submitGBuffer);
	assert(explicitTransparentRouting.submitForward);
	assert(!explicitTransparentRouting.sortBackToFront);
	assert(explicitTransparentRouting.effectiveLayer ==
		RenderLayer::Transparent3D);

	const RenderPacket overlayPacket = MakeModelPacket(
		RenderLayer::OverlayUI,
		RenderPacketPassMask::Overlay,
		blend
	);
	const ModelMaterialPassRoutingDecision overlayRouting =
		ModelMaterialPassRouting::Resolve(overlayPacket);
	assert(!overlayRouting.submitGBuffer);
	assert(!overlayRouting.submitForward);
	assert(!overlayRouting.sortBackToFront);
	assert(overlayRouting.effectiveLayer == RenderLayer::OverlayUI);

	RenderPacket legacyPacket;
	legacyPacket.kind = RenderPacketKind::Model;
	legacyPacket.layer = RenderLayer::Opaque3D;
	legacyPacket.passMask = deferredPasses;
	MaterialComponent legacyMaterial;
	legacyMaterial.Material.BaseColor.w = 0.4f;
	legacyPacket.bindings.material = &legacyMaterial;
	const ModelMaterialPassRoutingDecision legacyRouting =
		ModelMaterialPassRouting::Resolve(legacyPacket);
	assert(!legacyRouting.submitGBuffer);
	assert(legacyRouting.submitForward);
	assert(legacyRouting.sortBackToFront);

	return 0;
}
