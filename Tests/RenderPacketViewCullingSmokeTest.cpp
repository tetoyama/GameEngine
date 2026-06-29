#include <array>
#include <cassert>
#include <DirectXMath.h>

#include "Engine/Scene/Registry/componentRegistry.h"
#include "Engine/Scene/Registry/entityRegistry.h"
#include "Engine/Scene/System/Render/Culling/RenderPacketViewCulling.h"
#include "Engine/Scene/System/Render/Culling/ShadowRenderPacketCullingView.h"

namespace {

RenderPacket MakePacket(Entity entity, SceneContext* context){
	RenderPacket packet;
	packet.sceneContextID = context->contextID;
	packet.entity = entity;
	packet.kind = RenderPacketKind::Model;
	packet.layer = RenderLayer::Opaque3D;
	packet.passMask = RenderPacketPassMask::GBuffer;
	packet.bindings.sceneContext = context;
	return packet;
}

RenderPacketCullingView MakeShadowView(
	CullingViewKind parentKind,
	std::uint32_t parentInstanceID,
	std::uint32_t tileIndex
){
	RenderPacketCullingView view;
	view.kind = CullingViewKind::Shadow;
	view.instanceID = ShadowRenderPacketCullingView::MakeInstanceID(
		parentKind,
		parentInstanceID,
		tileIndex
	);
	view.viewProjection =
		DirectX::XMMatrixOrthographicLH(2.0f, 2.0f, 0.0f, 20.0f);
	return view;
}

} // namespace

int main(){
	EntityRegistry entities;
	SceneContext context{};
	ComponentRegistry components(&entities, &context);
	context.entity = &entities;
	context.component = &components;
	context.contextID = 7;
	context.storageConfig.visibleEntityReserve = 32;

	const Entity camera{100, 1};
	const Entity center = entities.Create();
	const Entity side = entities.Create();

	auto* centerBounds = components.AddComponent<CullingComponent>(center);
	centerBounds->worldBounds = {
		Vector3(-0.5f, -0.5f, 4.0f),
		Vector3(0.5f, 0.5f, 6.0f)
	};
	centerBounds->boundsValid = true;

	auto* sideBounds = components.AddComponent<CullingComponent>(side);
	sideBounds->worldBounds = {
		Vector3(4.5f, -0.5f, 4.0f),
		Vector3(5.5f, 0.5f, 6.0f)
	};
	sideBounds->boundsValid = true;

	RenderPacketWorkerBuffer worker(0);
	worker.Add(MakePacket(center, &context));
	worker.Add(MakePacket(side, &context));
	std::array<RenderPacketWorkerBuffer, 1> workers{worker};

	RenderPacketFrameBuffer packets;
	packets.BeginFrame(12);
	packets.Merge(workers);
	assert(packets.Size() == 2);

	RenderPacketCullingView editorView;
	editorView.camera = camera;
	editorView.kind = CullingViewKind::Editor;
	editorView.viewProjection =
		DirectX::XMMatrixOrthographicLH(2.0f, 2.0f, 0.0f, 20.0f);

	RenderPacketCullingView playerView;
	playerView.camera = camera;
	playerView.kind = CullingViewKind::Player;
	playerView.viewProjection =
		DirectX::XMMatrixOrthographicLH(20.0f, 20.0f, 0.0f, 20.0f);

	const RenderPacketCullingView shadowView =
		MakeShadowView(CullingViewKind::Player, 0, 0);
	const RenderPacketCullingView playerSecondTile =
		MakeShadowView(CullingViewKind::Player, 0, 1);
	const RenderPacketCullingView editorShadowView =
		MakeShadowView(CullingViewKind::Editor, 0, 0);

	RenderPacketCullingView invalidShadowView;
	invalidShadowView.kind = CullingViewKind::Shadow;
	invalidShadowView.instanceID = 0;
	invalidShadowView.viewProjection = shadowView.viewProjection;

	assert(shadowView.instanceID != 0);
	assert(shadowView.instanceID != playerSecondTile.instanceID);
	assert(shadowView.instanceID != editorShadowView.instanceID);
	assert(RenderPacketViewCulling::HasStableViewIdentity(editorView));
	assert(RenderPacketViewCulling::HasStableViewIdentity(shadowView));
	assert(!RenderPacketViewCulling::HasStableViewIdentity(invalidShadowView));

	CullingVisibilitySet visibility;
	RenderPacketViewCulling::Prepare(visibility, packets, editorView);
	RenderPacketViewCulling::Prepare(visibility, packets, playerView);
	RenderPacketViewCulling::Prepare(visibility, packets, shadowView);
	RenderPacketViewCulling::Prepare(visibility, packets, playerSecondTile);
	RenderPacketViewCulling::Prepare(visibility, packets, editorShadowView);
	RenderPacketViewCulling::Prepare(visibility, packets, invalidShadowView);

	assert(visibility.FrameSerial() == 12);
	assert(visibility.ViewCount() == 5);
	assert(RenderPacketViewCulling::ShouldRender(
		visibility, editorView, packets.Packets()[0]));
	assert(!RenderPacketViewCulling::ShouldRender(
		visibility, editorView, packets.Packets()[1]));
	assert(RenderPacketViewCulling::ShouldRender(
		visibility, playerView, packets.Packets()[1]));
	assert(RenderPacketViewCulling::ShouldRender(
		visibility, shadowView, packets.Packets()[0]));
	assert(!RenderPacketViewCulling::ShouldRender(
		visibility, shadowView, packets.Packets()[1]));
	assert(RenderPacketViewCulling::ShouldRender(
		visibility, invalidShadowView, packets.Packets()[1]));
	return 0;
}
