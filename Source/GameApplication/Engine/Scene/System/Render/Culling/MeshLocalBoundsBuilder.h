// =======================================================================
//
// MeshLocalBoundsBuilder.h
//
// =======================================================================
#pragma once

#include <cmath>
#include <cstddef>
#include <span>

#include "Scene/Component/meshRendererComponent.h"
#include "Shader/common.hlsl"

namespace MeshLocalBoundsBuilder {

inline bool TryBuild(
	std::span<const VERTEX_3D> vertices,
	Vector3& outMinimum,
	Vector3& outMaximum
) noexcept {
	if(vertices.empty()) return false;

	const DirectX::XMFLOAT3& first = vertices.front().Position;
	if(!std::isfinite(first.x) ||
		!std::isfinite(first.y) ||
		!std::isfinite(first.z)){
		return false;
	}

	outMinimum = Vector3(first.x, first.y, first.z);
	outMaximum = outMinimum;

	for(size_t index = 1; index < vertices.size(); ++index){
		const DirectX::XMFLOAT3& position = vertices[index].Position;
		if(!std::isfinite(position.x) ||
			!std::isfinite(position.y) ||
			!std::isfinite(position.z)){
			return false;
		}

		outMinimum.x = (std::min)(outMinimum.x, position.x);
		outMinimum.y = (std::min)(outMinimum.y, position.y);
		outMinimum.z = (std::min)(outMinimum.z, position.z);
		outMaximum.x = (std::max)(outMaximum.x, position.x);
		outMaximum.y = (std::max)(outMaximum.y, position.y);
		outMaximum.z = (std::max)(outMaximum.z, position.z);
	}
	return true;
}

inline bool Apply(
	MeshData& mesh,
	std::span<const VERTEX_3D> vertices
) noexcept {
	Vector3 minimum;
	Vector3 maximum;
	if(!TryBuild(vertices, minimum, maximum)){
		mesh.ClearLocalBounds();
		return false;
	}
	mesh.SetLocalBounds(minimum, maximum);
	return true;
}

} // namespace MeshLocalBoundsBuilder
