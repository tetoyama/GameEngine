// =======================================================================
//
// MeshLocalBoundsBuilder.h
//
// =======================================================================
#pragma once

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Scene/Component/meshRendererComponent.h"
#include "Shader/common.hlsl"

namespace MeshLocalBoundsBuilder {

inline void HashFloat(std::uint64_t& hash, float value) noexcept {
	hash ^= static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(value));
	hash *= 1099511628211ull;
}

inline std::uint64_t MakeGeometryResourceKey(
	std::span<const VERTEX_3D> vertices
) noexcept {
	if(vertices.empty()) return 0;

	std::uint64_t hash = 1469598103934665603ull;
	hash ^= static_cast<std::uint64_t>(vertices.size());
	hash *= 1099511628211ull;

	for(const VERTEX_3D& vertex : vertices){
		HashFloat(hash, vertex.Position.x);
		HashFloat(hash, vertex.Position.y);
		HashFloat(hash, vertex.Position.z);
		HashFloat(hash, vertex.Normal.x);
		HashFloat(hash, vertex.Normal.y);
		HashFloat(hash, vertex.Normal.z);
		HashFloat(hash, vertex.Tangent.x);
		HashFloat(hash, vertex.Tangent.y);
		HashFloat(hash, vertex.Tangent.z);
		HashFloat(hash, vertex.Diffuse.x);
		HashFloat(hash, vertex.Diffuse.y);
		HashFloat(hash, vertex.Diffuse.z);
		HashFloat(hash, vertex.Diffuse.w);
		HashFloat(hash, vertex.TexCoord.x);
		HashFloat(hash, vertex.TexCoord.y);
	}
	return hash == 0 ? 1 : hash;
}

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
	mesh.SetGeometryResourceKey(MakeGeometryResourceKey(vertices));
	return true;
}

} // namespace MeshLocalBoundsBuilder
