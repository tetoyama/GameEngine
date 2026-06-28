#include <algorithm>
#include <cassert>
#include <vector>

#include "Engine/Scene/System/Render/RenderSystem/RenderPacket/RenderPacket.h"

int main(){
	RenderPacket legacyPacket;
	assert(legacyPacket.TargetsAllSubMeshes());
	assert(!legacyPacket.TargetsSubMesh(0));

	RenderPacket firstSubMesh;
	firstSubMesh.sceneContextID = 1;
	firstSubMesh.entity = Entity{8, 2};
	firstSubMesh.kind = RenderPacketKind::Model;
	firstSubMesh.layer = RenderLayer::Opaque3D;
	firstSubMesh.materialKey = 3;
	firstSubMesh.sortKey = MakeRenderPacketSortKey(
		firstSubMesh.layer,
		firstSubMesh.kind,
		firstSubMesh.materialKey,
		0
	);
	firstSubMesh.subMeshIndex = 0;
	firstSubMesh.stableSequence = 2;
	assert(!firstSubMesh.TargetsAllSubMeshes());
	assert(firstSubMesh.TargetsSubMesh(0));

	RenderPacket secondSubMesh = firstSubMesh;
	secondSubMesh.subMeshIndex = 1;
	secondSubMesh.stableSequence = 1;
	assert(secondSubMesh.TargetsSubMesh(1));

	RenderPacket allSubMeshes = firstSubMesh;
	allSubMeshes.subMeshIndex = RenderPacketAllSubMeshes;
	allSubMeshes.stableSequence = 0;

	std::vector<RenderPacket> packets{
		allSubMeshes,
		secondSubMesh,
		firstSubMesh
	};
	std::sort(packets.begin(), packets.end(), RenderPacketLess);

	assert(packets[0].subMeshIndex == 0);
	assert(packets[1].subMeshIndex == 1);
	assert(packets[2].TargetsAllSubMeshes());
	return 0;
}
