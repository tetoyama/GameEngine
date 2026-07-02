// =======================================================================
//
// CullingBoundsRuntime.h
//
// =======================================================================
#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>

#include <DirectXMath.h>

#include "Scene/Component/CullingComponent.h"

namespace CullingBoundsRuntime {

inline std::uint64_t MakeTransformRevision(
	const DirectX::XMMATRIX& worldMatrix
) noexcept {
	DirectX::XMFLOAT4X4 stored{};
	DirectX::XMStoreFloat4x4(&stored, worldMatrix);

	const float* values = &stored._11;
	std::uint64_t hash = 1469598103934665603ull;
	for(size_t index = 0; index < 16; ++index){
		hash ^= std::bit_cast<std::uint32_t>(values[index]);
		hash *= 1099511628211ull;
	}
	return hash;
}

inline EntityAABB TransformBounds(
	const EntityAABB& localBounds,
	const DirectX::XMMATRIX& worldMatrix
) noexcept {
	if(!localBounds.IsValid()){
		return {
			Vector3(1.0f, 1.0f, 1.0f),
			Vector3(-1.0f, -1.0f, -1.0f)
		};
	}

	const std::array<DirectX::XMVECTOR, 8> corners{
		DirectX::XMVectorSet(localBounds.min.x, localBounds.min.y, localBounds.min.z, 1.0f),
		DirectX::XMVectorSet(localBounds.max.x, localBounds.min.y, localBounds.min.z, 1.0f),
		DirectX::XMVectorSet(localBounds.min.x, localBounds.max.y, localBounds.min.z, 1.0f),
		DirectX::XMVectorSet(localBounds.max.x, localBounds.max.y, localBounds.min.z, 1.0f),
		DirectX::XMVectorSet(localBounds.min.x, localBounds.min.y, localBounds.max.z, 1.0f),
		DirectX::XMVectorSet(localBounds.max.x, localBounds.min.y, localBounds.max.z, 1.0f),
		DirectX::XMVectorSet(localBounds.min.x, localBounds.max.y, localBounds.max.z, 1.0f),
		DirectX::XMVectorSet(localBounds.max.x, localBounds.max.y, localBounds.max.z, 1.0f)
	};

	DirectX::XMFLOAT3 first{};
	DirectX::XMStoreFloat3(
		&first,
		DirectX::XMVector3TransformCoord(corners[0], worldMatrix)
	);
	EntityAABB worldBounds{
		Vector3(first.x, first.y, first.z),
		Vector3(first.x, first.y, first.z)
	};

	for(size_t index = 1; index < corners.size(); ++index){
		DirectX::XMFLOAT3 transformed{};
		DirectX::XMStoreFloat3(
			&transformed,
			DirectX::XMVector3TransformCoord(corners[index], worldMatrix)
		);
		worldBounds.min.x = (std::min)(worldBounds.min.x, transformed.x);
		worldBounds.min.y = (std::min)(worldBounds.min.y, transformed.y);
		worldBounds.min.z = (std::min)(worldBounds.min.z, transformed.z);
		worldBounds.max.x = (std::max)(worldBounds.max.x, transformed.x);
		worldBounds.max.y = (std::max)(worldBounds.max.y, transformed.y);
		worldBounds.max.z = (std::max)(worldBounds.max.z, transformed.z);
	}
	return worldBounds;
}

// Local Bounds供給元またはWorld Transformが変わった時だけ派生Boundsを更新する。
// 戻り値は実際に再計算した場合にtrue。
inline bool UpdateWorldBounds(
	CullingComponent& culling,
	const DirectX::XMMATRIX& worldMatrix,
	std::uint64_t sourceRevision
) noexcept {
	if(!culling.localBounds.IsValid()){
		culling.boundsValid = false;
		return false;
	}

	const std::uint64_t transformRevision =
		MakeTransformRevision(worldMatrix);
	if(culling.boundsValid &&
		culling.sourceRevision == sourceRevision &&
		culling.transformRevision == transformRevision){
		return false;
	}

	culling.worldBounds = TransformBounds(culling.localBounds, worldMatrix);
	culling.sourceRevision = sourceRevision;
	culling.transformRevision = transformRevision;
	culling.boundsValid = culling.worldBounds.IsValid();
	return true;
}

} // namespace CullingBoundsRuntime
