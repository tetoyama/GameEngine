#pragma once

#include <cmath>
#include <DirectXMath.h>
#include "CullingMath.h"

namespace CullingFrustumRuntime {

inline CullingPlane MakePlane(float x, float y, float z, float d) noexcept {
	const float length = std::sqrt(x * x + y * y + z * z);
	if(length <= 0.000001f) return {};
	const float inverse = 1.0f / length;
	return {Vector3(x * inverse, y * inverse, z * inverse), d * inverse};
}

inline CullingFrustum FromViewProjection(
	const DirectX::XMMATRIX& viewProjection
) noexcept {
	DirectX::XMFLOAT4X4 m{};
	DirectX::XMStoreFloat4x4(&m, viewProjection);

	CullingFrustum result;
	result.planes[0] = MakePlane(m._14 + m._11, m._24 + m._21, m._34 + m._31, m._44 + m._41);
	result.planes[1] = MakePlane(m._14 - m._11, m._24 - m._21, m._34 - m._31, m._44 - m._41);
	result.planes[2] = MakePlane(m._14 + m._12, m._24 + m._22, m._34 + m._32, m._44 + m._42);
	result.planes[3] = MakePlane(m._14 - m._12, m._24 - m._22, m._34 - m._32, m._44 - m._42);
	result.planes[4] = MakePlane(m._13, m._23, m._33, m._43);
	result.planes[5] = MakePlane(m._14 - m._13, m._24 - m._23, m._34 - m._33, m._44 - m._43);
	return result;
}

} // namespace CullingFrustumRuntime
