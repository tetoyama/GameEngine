#include <cassert>
#include <vector>
#include <DirectXMath.h>

#include "Engine/Scene/Registry/componentRegistry.h"
#include "Engine/Scene/Registry/entityRegistry.h"
#include "Engine/Scene/System/Render/Culling/CullingFrustumRuntime.h"
#include "Engine/Scene/System/Render/StaticBatch/StaticBatchGroupVisibility.h"

static StaticBatchInstanceData MakeInstance(Entity entity, std::uint32_t sceneID){
	StaticBatchInstanceData value;
	value.entityIndex = entity.GetIndex();
	value.entityGeneration = entity.GetGeneration();
	value.sceneContextID = sceneID;
	return value;
}

int main(){
	EntityRegistry entities;
	SceneContext context{};
	ComponentRegistry components(&entities, &context);
	context.entity = &entities;
	context.component = &components;
	context.contextID = 7;

	const Entity visible = entities.Create();
	const Entity hidden = entities.Create();
	const Entity uncullable = entities.Create();

	auto* visibleBounds = components.AddComponent<CullingComponent>(visible);
	visibleBounds->worldBounds = {
		Vector3(-0.5f, -0.5f, 4.0f),
		Vector3(0.5f, 0.5f, 6.0f)
	};
	visibleBounds->boundsValid = true;

	auto* hiddenBounds = components.AddComponent<CullingComponent>(hidden);
	hiddenBounds->worldBounds = {
		Vector3(4.5f, -0.5f, 4.0f),
		Vector3(5.5f, 0.5f, 6.0f)
	};
	hiddenBounds->boundsValid = true;

	RenderPacket representative;
	representative.sceneContextID = context.contextID;
	representative.entity = visible;
	representative.kind = RenderPacketKind::Model;
	representative.layer = RenderLayer::Opaque3D;
	representative.passMask = RenderPacketPassMask::GBuffer;
	representative.bindings.sceneContext = &context;
	std::vector<RenderPacket> packets{representative};

	std::vector<StaticBatchInstanceData> instances{
		MakeInstance(visible, context.contextID),
		MakeInstance(hidden, context.contextID),
		MakeInstance(uncullable, context.contextID)
	};

	RenderPacketCullingView view;
	view.camera = Entity{100, 1};
	view.kind = CullingViewKind::Editor;
	view.viewProjection = DirectX::XMMatrixOrthographicLH(
		2.0f, 2.0f, 0.0f, 20.0f
	);

	CullingVisibilitySet visibility;
	visibility.BeginFrame(1);
	CullingVisibilityBuilder::BuildView(
		visibility,
		RenderPacketViewCulling::MakeViewKey(context.contextID, view),
		CullingFrustumRuntime::FromViewProjection(view.viewProjection),
		components,
		32,
		true
	);

	StaticBatchPacketCacheEntry group;
	group.key.kind = RenderPacketKind::Model;
	group.sceneContextID = context.contextID;
	group.representativePacketIndex = 0;

	group.firstInstance = 0;
	group.instanceCount = 1;
	auto result = StaticBatchGroupVisibility::Evaluate(
		group, instances, packets, visibility, view
	);
	assert(result.CanSubmitInstanced());

	group.firstInstance = 1;
	result = StaticBatchGroupVisibility::Evaluate(
		group, instances, packets, visibility, view
	);
	assert(result.CanSkipEntireGroup());

	group.firstInstance = 0;
	group.instanceCount = 2;
	result = StaticBatchGroupVisibility::Evaluate(
		group, instances, packets, visibility, view
	);
	assert(result.RequiresPacketFallback());
	assert(result.state == StaticBatchGroupVisibilityState::Mixed);

	group.firstInstance = 2;
	group.instanceCount = 1;
	result = StaticBatchGroupVisibility::Evaluate(
		group, instances, packets, visibility, view
	);
	assert(result.CanSubmitInstanced());

	CullingVisibilitySet missingView;
	missingView.BeginFrame(1);
	group.firstInstance = 0;
	group.instanceCount = 2;
	result = StaticBatchGroupVisibility::Evaluate(
		group, instances, packets, missingView, view
	);
	assert(result.CanSubmitInstanced());

	group.firstInstance = instances.size();
	group.instanceCount = 1;
	result = StaticBatchGroupVisibility::Evaluate(
		group, instances, packets, visibility, view
	);
	assert(result.state == StaticBatchGroupVisibilityState::Invalid);
	return 0;
}
