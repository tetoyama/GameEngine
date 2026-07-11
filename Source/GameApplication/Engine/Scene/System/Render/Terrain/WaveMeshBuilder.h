// =======================================================================
//
// WaveMeshBuilder.h
//
// Step 17-E: Waveメッシュの純CPU Build側。
// D3D11 Resourceへ触れず、Topologyと毎Frame頂点をstagingへ生成する。
//
// =======================================================================
#pragma once

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include <DirectXMath.h>

#include "Shader/Common.hlsl"

namespace WaveMeshBuilder {

inline void HashByte(std::uint64_t& hash, std::uint8_t value) noexcept {
	hash ^= value;
	hash *= 1099511628211ull;
}

inline void HashUint32(std::uint64_t& hash, std::uint32_t value) noexcept {
	for(unsigned shift = 0; shift < 32; shift += 8){
		HashByte(hash, static_cast<std::uint8_t>(value >> shift));
	}
}

inline std::uint64_t ComputeInputSignature(
	int resolution,
	float amplitude,
	float wavelength,
	float time
) noexcept {
	std::uint64_t hash = 14695981039346656037ull;
	HashUint32(hash, static_cast<std::uint32_t>(resolution));
	HashUint32(hash, std::bit_cast<std::uint32_t>(amplitude));
	HashUint32(hash, std::bit_cast<std::uint32_t>(wavelength));
	HashUint32(hash, std::bit_cast<std::uint32_t>(time));
	return hash == 0 ? 1 : hash;
}

inline bool TryComputeCounts(
	int resolution,
	std::size_t& vertexCount,
	std::size_t& indexCount
) noexcept {
	vertexCount = 0;
	indexCount = 0;
	if(resolution <= 0) return false;

	const std::size_t grid = static_cast<std::size_t>(resolution);
	if(grid == (std::numeric_limits<std::size_t>::max)()) return false;
	const std::size_t side = grid + 1;
	if(side > (std::numeric_limits<std::size_t>::max)() / side) return false;
	vertexCount = side * side;

	if(grid > (std::numeric_limits<std::size_t>::max)() / grid) return false;
	const std::size_t cells = grid * grid;
	if(cells > (std::numeric_limits<std::size_t>::max)() / 6) return false;
	indexCount = cells * 6;

	const std::size_t maximumUint32 = static_cast<std::size_t>(
		(std::numeric_limits<std::uint32_t>::max)()
	);
	if(vertexCount > maximumUint32 || indexCount > maximumUint32){
		return false;
	}
	if(vertexCount > maximumUint32 / sizeof(VERTEX_3D)){
		return false;
	}
	if(indexCount > maximumUint32 / sizeof(std::uint32_t)){
		return false;
	}
	return true;
}

inline void FillBaseVertex(
	VERTEX_3D& vertex,
	float x,
	float y,
	float z,
	float u,
	float v
) noexcept {
	vertex.Position = DirectX::XMFLOAT3(x, y, z);
	vertex.Normal = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
	vertex.Tangent = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
	vertex.TexCoord = DirectX::XMFLOAT2(u, v);
	vertex.Diffuse = DirectX::XMFLOAT4(0.3f, 0.6f, 1.0f, 1.0f);
}

inline bool BuildTopology(
	int resolution,
	std::vector<VERTEX_3D>& outVertices,
	std::vector<std::uint32_t>& outIndices
){
	std::size_t vertexCount = 0;
	std::size_t indexCount = 0;
	if(!TryComputeCounts(resolution, vertexCount, indexCount)){
		outVertices.clear();
		outIndices.clear();
		return false;
	}

	outVertices.assign(vertexCount, VERTEX_3D{});
	outIndices.assign(indexCount, 0u);
	const float gridFloat = static_cast<float>(resolution);
	const std::size_t side = static_cast<std::size_t>(resolution) + 1;

	for(int z = 0; z <= resolution; ++z){
		for(int x = 0; x <= resolution; ++x){
			const std::size_t vertexIndex =
				static_cast<std::size_t>(z) * side +
				static_cast<std::size_t>(x);
			const float u = static_cast<float>(x) / gridFloat;
			const float v = static_cast<float>(z) / gridFloat;
			FillBaseVertex(
				outVertices[vertexIndex],
				(u - 0.5f) * 2.0f,
				0.0f,
				(v - 0.5f) * 2.0f,
				u,
				v
			);
		}
	}

	std::size_t index = 0;
	for(int z = 0; z < resolution; ++z){
		for(int x = 0; x < resolution; ++x){
			const std::size_t topLeftValue =
				static_cast<std::size_t>(z) * side +
				static_cast<std::size_t>(x);
			const std::size_t bottomLeftValue = topLeftValue + side;
			const std::uint32_t topLeft =
				static_cast<std::uint32_t>(topLeftValue);
			const std::uint32_t topRight = topLeft + 1;
			const std::uint32_t bottomLeft =
				static_cast<std::uint32_t>(bottomLeftValue);
			const std::uint32_t bottomRight = bottomLeft + 1;
			outIndices[index++] = topLeft;
			outIndices[index++] = bottomLeft;
			outIndices[index++] = topRight;
			outIndices[index++] = topRight;
			outIndices[index++] = bottomLeft;
			outIndices[index++] = bottomRight;
		}
	}
	return true;
}

inline bool BuildAnimatedVertices(
	int resolution,
	float amplitude,
	float wavelength,
	float time,
	std::vector<VERTEX_3D>& outVertices
){
	std::size_t vertexCount = 0;
	std::size_t ignoredIndexCount = 0;
	if(!TryComputeCounts(resolution, vertexCount, ignoredIndexCount) ||
		!std::isfinite(amplitude) ||
		!std::isfinite(wavelength) ||
		!std::isfinite(time) ||
		std::fabs(wavelength) <= 1.0e-6f){
		outVertices.clear();
		return false;
	}

	outVertices.assign(vertexCount, VERTEX_3D{});
	const float gridFloat = static_cast<float>(resolution);
	const std::size_t side = static_cast<std::size_t>(resolution) + 1;
	const float omega = 2.0f * DirectX::XM_PI;
	const float waveNumber = 2.0f * DirectX::XM_PI / wavelength;

	for(int z = 0; z <= resolution; ++z){
		for(int x = 0; x <= resolution; ++x){
			const std::size_t vertexIndex =
				static_cast<std::size_t>(z) * side +
				static_cast<std::size_t>(x);
			const float u = static_cast<float>(x) / gridFloat;
			const float v = static_cast<float>(z) / gridFloat;
			const float px = (u - 0.5f) * 2.0f;
			const float pz = (v - 0.5f) * 2.0f;
			const float radius = std::sqrt(px * px + pz * pz);
			const float y = amplitude * std::sin(waveNumber * radius - omega * time);
			FillBaseVertex(outVertices[vertexIndex], px, y, pz, u, v);
		}
	}
	return true;
}

} // namespace WaveMeshBuilder
