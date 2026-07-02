// =======================================================================
//
// CullingVisibilityBuilder.h
//
// =======================================================================
#pragma once

#include <cstddef>

#include "CullingMath.h"
#include "CullingVisibilitySet.h"
#include "Scene/Component/EntityStateComponents.h"
#include "Scene/Registry/componentRegistry.h"

namespace CullingVisibilityBuilder {

// CullingComponentを持つEntityだけをView単位で判定する。
// CullingComponentなしEntityは常時描画候補であり、この集合へは登録しない。
inline void BuildView(
	CullingVisibilitySet& visibility,
	CullingViewKey view,
	const CullingFrustum& frustum,
	ComponentRegistry& components,
	size_t visibleReserve,
	bool allowRuntimeGrowth = true
){
	visibility.BeginView(view, allowRuntimeGrowth);
	visibility.ReserveView(view, visibleReserve);

	const auto entities =
		components.FindEntitiesWithComponent<CullingComponent>();
	for(Entity entity : entities){
		if(components.HasComponent<DisabledComponent>(entity)) continue;
		if(components.HasComponent<HiddenComponent>(entity)) continue;

		const CullingComponent* culling =
			components.GetComponent<CullingComponent>(entity);
		if(!culling) continue;

		// Bounds未生成時は安全側へ倒し、可視候補として残す。
		if(!culling->boundsValid ||
			CullingMath::IntersectsFrustum(culling->worldBounds, frustum)){
			if(!visibility.SetVisible(view, entity) &&
				visibility.IsViewOverflowed(view)){
				// 部分的な可視集合は誤Cullingを起こすため破棄し、
				// HasView=falseの全描画Fallbackへ戻す。
				break;
			}
		}
	}
}

// Visibility Viewがまだ構築されていない場合も安全側へ倒す。
inline bool ShouldRender(
	const CullingVisibilitySet& visibility,
	CullingViewKey view,
	Entity entity,
	const ComponentRegistry& components
){
	if(!components.HasComponent<CullingComponent>(entity)) return true;
	if(!visibility.HasView(view)) return true;
	return visibility.IsVisible(view, entity);
}

} // namespace CullingVisibilityBuilder
