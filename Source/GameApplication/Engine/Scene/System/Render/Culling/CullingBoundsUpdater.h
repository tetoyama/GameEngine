// =======================================================================
//
// CullingBoundsUpdater.h
//
// =======================================================================
#pragma once

#include <cstddef>

#include "CullingBoundsRuntime.h"
#include "Scene/Component/transformComponent.h"
#include "Scene/Registry/componentRegistry.h"

namespace CullingBoundsUpdater {

struct UpdateResult {
	size_t visited = 0;
	size_t updated = 0;
	size_t unavailable = 0;
};

// Resource側がLocal Boundsと0以外のsourceRevisionを供給したEntityだけを更新する。
// 未供給またはTransformなしの場合はboundsValid=falseを維持し、安全側で描画候補へ残す。
inline UpdateResult UpdateScene(ComponentRegistry& components){
	UpdateResult result;
	const auto entities =
		components.FindEntitiesWithComponent<CullingComponent>();

	for(Entity entity : entities){
		++result.visited;
		CullingComponent* culling =
			components.GetComponent<CullingComponent>(entity);
		TransformComponent* transform =
			components.GetComponent<TransformComponent>(entity);

		if(!culling || !transform || culling->sourceRevision == 0 ||
			!culling->localBounds.IsValid()){
			if(culling) culling->boundsValid = false;
			++result.unavailable;
			continue;
		}

		const DirectX::XMMATRIX worldMatrix =
			transform->CalculateWorldMatrix(transform, &components);
		if(CullingBoundsRuntime::UpdateWorldBounds(
			*culling,
			worldMatrix,
			culling->sourceRevision
		)){
			++result.updated;
		}
	}
	return result;
}

} // namespace CullingBoundsUpdater
