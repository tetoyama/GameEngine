// =======================================================================
//
// TransformMath.h
//
// TransformComponent定義後にincludeする実装ヘッダー。
//
// =======================================================================
#pragma once

#include <cstdint>
#include <unordered_set>

#include <DirectXMath.h>

namespace TransformMath {

inline DirectX::XMMATRIX CalculateLocalMatrix(
	const TransformComponent& transform
){
	return
		DirectX::XMMatrixScaling(
			transform.scale.x,
			transform.scale.y,
			transform.scale.z
		) *
		DirectX::XMMatrixRotationQuaternion(
			DirectX::XMLoadFloat4(&transform.GetRotation())
		) *
		DirectX::XMMatrixTranslation(
			transform.position.x,
			transform.position.y,
			transform.position.z
		);
}

inline DirectX::XMMATRIX CalculateWorldMatrix(
	const TransformComponent& transform,
	ComponentRegistry* componentRegistry
){
	DirectX::XMMATRIX world = CalculateLocalMatrix(transform);
	if(!componentRegistry){
		return world;
	}

	Entity parent = transform.parent;
	std::unordered_set<uint64_t> visitedParents;

	while(parent){
		// 自己参照・複数Entityによる循環の両方を停止する。
		if(!visitedParents.insert(parent.GetPackedValue()).second){
			break;
		}

		const TransformComponent* parentTransform =
			componentRegistry->GetComponent<TransformComponent>(parent);
		if(!parentTransform){
			break;
		}

		world = world * CalculateLocalMatrix(*parentTransform);
		parent = parentTransform->parent;
	}

	return world;
}

inline Vector3 GetWorldPosition(
	const TransformComponent& transform,
	ComponentRegistry* componentRegistry
){
	DirectX::XMFLOAT4X4 matrix{};
	DirectX::XMStoreFloat4x4(
		&matrix,
		CalculateWorldMatrix(transform, componentRegistry)
	);
	return Vector3(matrix._41, matrix._42, matrix._43);
}

} // namespace TransformMath

// 既存コードを段階移行するための互換ラッパー。
inline DirectX::XMMATRIX TransformComponent::CalculateWorldMatrix(
	const TransformComponent* transform,
	ComponentRegistry* componentRegistry
) const {
	return TransformMath::CalculateWorldMatrix(
		transform ? *transform : *this,
		componentRegistry
	);
}

inline Vector3 TransformComponent::GetWorldPosition(
	ComponentRegistry* componentRegistry
) const {
	return TransformMath::GetWorldPosition(*this, componentRegistry);
}
