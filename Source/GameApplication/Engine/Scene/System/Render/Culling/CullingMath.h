// =======================================================================
//
// CullingMath.h
//
// =======================================================================
#pragma once

#include <array>

#include "Scene/Component/CullingComponent.h"

struct CullingPlane {
	Vector3 normal = Vector3(0.0f, 0.0f, 0.0f);
	float distance = 0.0f;
};

struct CullingFrustum {
	std::array<CullingPlane, 6> planes{};
};

namespace CullingMath {

inline float Dot(const Vector3& left, const Vector3& right) noexcept {
	return left.x * right.x + left.y * right.y + left.z * right.z;
}

// AABBがPlaneの負側へ完全に外れているかをPositive Vertexで判定する。
inline bool IsOutsidePlane(
	const EntityAABB& bounds,
	const CullingPlane& plane
) noexcept {
	const Vector3 positiveVertex(
		plane.normal.x >= 0.0f ? bounds.max.x : bounds.min.x,
		plane.normal.y >= 0.0f ? bounds.max.y : bounds.min.y,
		plane.normal.z >= 0.0f ? bounds.max.z : bounds.min.z
	);
	return Dot(plane.normal, positiveVertex) + plane.distance < 0.0f;
}

inline bool IntersectsFrustum(
	const EntityAABB& bounds,
	const CullingFrustum& frustum
) noexcept {
	if(!bounds.IsValid()){
		// Bounds未生成時は安全側へ倒し、描画候補として残す。
		return true;
	}

	for(const CullingPlane& plane : frustum.planes){
		if(IsOutsidePlane(bounds, plane)) return false;
	}
	return true;
}

inline CullingFrustum MakeAxisAlignedFrustum(const EntityAABB& bounds) noexcept {
	CullingFrustum frustum;
	frustum.planes[0] = {Vector3( 1.0f, 0.0f, 0.0f), -bounds.min.x};
	frustum.planes[1] = {Vector3(-1.0f, 0.0f, 0.0f),  bounds.max.x};
	frustum.planes[2] = {Vector3(0.0f,  1.0f, 0.0f), -bounds.min.y};
	frustum.planes[3] = {Vector3(0.0f, -1.0f, 0.0f),  bounds.max.y};
	frustum.planes[4] = {Vector3(0.0f, 0.0f,  1.0f), -bounds.min.z};
	frustum.planes[5] = {Vector3(0.0f, 0.0f, -1.0f),  bounds.max.z};
	return frustum;
}

} // namespace CullingMath
