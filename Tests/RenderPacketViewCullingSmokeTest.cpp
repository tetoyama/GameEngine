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
	std::uint32_t tileIndex,
	bool gpuDepthClipEnabled = false
){
	return ShadowRenderPacketCullingView::BuildFromViewProjection(
		DirectX::XMMatrixOrthographicLH(2.0f, 2.0f, 0.0f, 20.0f),
		parentKind,
		parentInstanceID,
		tileIndex,
		gpuDepthClipEnabled
	);
}

bool IsZeroPlane(const CullingPlane& plane){
	return plane.normal.x == 0.0f &&
		plane.normal.y == 0.0f &&
		plane.normal.z == 0.0f &&
		plane.distance == 0.0f;
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
	const Entity beforeNear = entities.Create();
	const Entity beyondFar = entities.Create();

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

	auto* beforeNearBounds =
		components.AddComponent<CullingComponent>(beforeNear);
	beforeNearBounds->worldBounds = {
		Vector3(-0.5f, -0.5f, -6.0f),
		Vector3(0.5f, 0.5f, -4.0f)
	};
	beforeNearBounds->boundsValid = true;

	auto* beyondFarBounds =
		components.AddComponent<CullingComponent>(beyondFar);
	beyondFarBounds->worldBounds = {
		Vector3(-0.5f, -0.5f, 24.0f),
		Vector3(0.5f, 0.5f, 26.0f)
	};
	beyondFarBounds->boundsValid = true;

	RenderPacketWorkerBuffer worker(0);
	worker.Add(MakePacket(center, &context));
	worker.Add(MakePacket(side, &context));
	worker.Add(MakePacket(beforeNear, &context));
	worker.Add(MakePacket(beyondFar, &context));
	std::array<RenderPacketWorkerBuffer, 1> workers{worker};

	RenderPacketFrameBuffer packets;
	packets.BeginFrame(12);
	packets.Merge(workers);
	assert(packets.Size() == 4);

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
		MakeShadowView(CullingViewKind::Player, 0, 0, false);
	const RenderPacketCullingView playerSecondTile =
		MakeShadowView(CullingViewKind::Player, 0, 1, false);
	const RenderPacketCullingView editorShadowView =
		MakeShadowView(CullingViewKind::Editor, 0, 0, false);
	const RenderPacketCullingView perspectiveShadowView =
		MakeShadowView(CullingViewKind::Player, 0, 2, true);

	RenderPacketCullingView invalidShadowView;
	invalidShadowView.kind = CullingViewKind::Shadow;
	invalidShadowView.instanceID = 0;
	invalidShadowView.viewProjection = shadowView.viewProjection;

	assert(shadowView.instanceID != 0);
	assert(shadowView.instanceID != playerSecondTile.instanceID);
	assert(shadowView.instanceID != editorShadowView.instanceID);
	assert(shadowView.instanceID != perspectiveShadowView.instanceID);
	assert(RenderPacketViewCulling::HasStableViewIdentity(editorView));
	assert(RenderPacketViewCulling::HasStableViewIdentity(shadowView));
	assert(RenderPacketViewCulling::HasStableViewIdentity(perspectiveShadowView));
	assert(!RenderPacketViewCulling::HasStableViewIdentity(invalidShadowView));

	const CullingFrustum editorFrustum =
		RenderPacketViewCulling::BuildFrustum(editorView);
	const CullingFrustum shadowFrustum =
		RenderPacketViewCulling::BuildFrustum(shadowView);
	const CullingFrustum perspectiveShadowFrustum =
		RenderPacketViewCulling::BuildFrustum(perspectiveShadowView);
	assert(!IsZeroPlane(editorFrustum.planes[4]));
	assert(!IsZeroPlane(editorFrustum.planes[5]));
	assert(IsZeroPlane(shadowFrustum.planes[4]));
	assert(IsZeroPlane(shadowFrustum.planes[5]));
	assert(IsZeroPlane(perspectiveShadowFrustum.planes[4]));
	assert(IsZeroPlane(perspectiveShadowFrustum.planes[5]));

	CullingVisibilitySet visibility;
	RenderPacketViewCulling::Prepare(visibility, packets, editorView);
	RenderPacketViewCulling::Prepare(visibility, packets, playerView);
	RenderPacketViewCulling::Prepare(visibility, packets, shadowView);
	RenderPacketViewCulling::Prepare(visibility, packets, playerSecondTile);
	RenderPacketViewCulling::Prepare(visibility, packets, editorShadowView);
	RenderPacketViewCulling::Prepare(
		visibility,
		packets,
		perspectiveShadowView
	);
	RenderPacketViewCulling::Prepare(visibility, packets, invalidShadowView);

	assert(visibility.FrameSerial() == 12);
	assert(visibility.ViewCount() == 6);
	assert(RenderPacketViewCulling::ShouldRender(
		visibility, editorView, packets.Packets()[0]));
	assert(!RenderPacketViewCulling::ShouldRender(
		visibility, editorView, packets.Packets()[1]));
	assert(!RenderPacketViewCulling::ShouldRender(
		visibility, editorView, packets.Packets()[2]));
	assert(!RenderPacketViewCulling::ShouldRender(
		visibility, editorView, packets.Packets()[3]));
	assert(RenderPacketViewCulling::ShouldRender(
		visibility, playerView, packets.Packets()[1]));
	assert(RenderPacketViewCulling::ShouldRender(
		visibility, shadowView, packets.Packets()[0]));
	assert(!RenderPacketViewCulling::ShouldRender(
		visibility, shadowView, packets.Packets()[1]));
	assert(RenderPacketViewCulling::ShouldRender(
		visibility, shadowView, packets.Packets()[2]));
	assert(RenderPacketViewCulling::ShouldRender(
		visibility, shadowView, packets.Packets()[3]));
	assert(RenderPacketViewCulling::ShouldRender(
		visibility, perspectiveShadowView, packets.Packets()[0]));
	assert(!RenderPacketViewCulling::ShouldRender(
		visibility, perspectiveShadowView, packets.Packets()[1]));
	assert(RenderPacketViewCulling::ShouldRender(
		visibility, perspectiveShadowView, packets.Packets()[2]));
	assert(RenderPacketViewCulling::ShouldRender(
		visibility, perspectiveShadowView, packets.Packets()[3]));
	assert(RenderPacketViewCulling::ShouldRender(
		visibility, invalidShadowView, packets.Packets()[1]));
	return 0;
}
