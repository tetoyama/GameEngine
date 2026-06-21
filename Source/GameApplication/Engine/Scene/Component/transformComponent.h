// =======================================================================
//
// transformComponent.h
//
// =======================================================================
#pragma once

#include <vector>

#include <DirectXMath.h>

#include "Interface/IComponent.h"
#include "Backends/myVector3.h"
#include "Registry/componentRegistry.h"
#include "2DspriteRendererComponent.h"
#include "Operations/TransformRotationMath.h"

// Entityのローカル変換と階層参照を保持するComponent。
//
// Quaternionを実際の回転値とし、rotationEulerはInspectorと軸単位操作のための
// 連続した表示キャッシュとして保持する。
// YAML、Inspector、階層行列、UI Rect変換の実装はOperations配下へ分離する。
class TransformComponent {
private:
	Vector3 rotationEuler = Vector3(0.0f, 0.0f, 0.0f);
	DirectX::XMFLOAT4 rotation = {0.0f, 0.0f, 0.0f, 1.0f};

public:
	Vector3 position = Vector3(0.0f, 0.0f, 0.0f);
	Vector3 scale = Vector3(1.0f, 1.0f, 1.0f);
	Entity parent{};

	// Sceneが階層変更時に再構築する派生データ。
	// 将来のParent / Children専用Storage移行までは互換維持のため残す。
	std::vector<Entity> children;

	DirectX::XMVECTOR rotationVector() const {
		return DirectX::XMLoadFloat4(&rotation);
	}

	void SetRotationX(float value){
		Vector3 euler = rotationEuler;
		euler.x = value;
		SetRotationEuler(euler);
	}

	void SetRotationY(float value){
		Vector3 euler = rotationEuler;
		euler.y = value;
		SetRotationEuler(euler);
	}

	void SetRotationZ(float value){
		Vector3 euler = rotationEuler;
		euler.z = value;
		SetRotationEuler(euler);
	}

	void AddRotationX(float value){
		Vector3 euler = rotationEuler;
		euler.x += value;
		SetRotationEuler(euler);
	}

	void AddRotationY(float value){
		Vector3 euler = rotationEuler;
		euler.y += value;
		SetRotationEuler(euler);
	}

	void AddRotationZ(float value){
		Vector3 euler = rotationEuler;
		euler.z += value;
		SetRotationEuler(euler);
	}

	// Eulerは内部ではラジアン。X=Pitch、Y=Yaw、Z=Roll。
	void SetRotationEuler(const Vector3& euler){
		rotationEuler = euler;

		rotation = TransformRotationMath::QuaternionFromEuler(
			DirectX::XMFLOAT3(euler.x, euler.y, euler.z)
		);
	}

	// 外部Quaternionを設定する。
	// 同じ回転を表すQuaternionが再入力された場合はEulerキャッシュを維持し、
	// PhysicsDownloadなどによる毎Tickの再分解でInspector値が跳ねるのを防ぐ。
	void SetRotation(const DirectX::XMFLOAT4& quaternion){
		const DirectX::XMFLOAT4 aligned =
			TransformRotationMath::AlignHemisphere(quaternion, rotation);
		const bool sameRotation =
			TransformRotationMath::Equivalent(rotation, aligned);

		rotation = aligned;
		if(sameRotation){
			return;
		}

		const DirectX::XMFLOAT3 reference(
			rotationEuler.x,
			rotationEuler.y,
			rotationEuler.z
		);
		const DirectX::XMFLOAT3 converted =
			TransformRotationMath::EulerFromQuaternion(rotation, reference);
		rotationEuler = Vector3(converted.x, converted.y, converted.z);
	}

	const DirectX::XMFLOAT4& GetRotation() const {
		return rotation;
	}

	// Inspectorと軸単位操作では、Quaternionから毎回再計算せず連続キャッシュを返す。
	Vector3 GetRotationEuler() const {
		return rotationEuler;
	}

	Vector3 front() const {
		const DirectX::XMVECTOR value = DirectX::XMVector3Rotate(
			DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
			rotationVector()
		);
		DirectX::XMFLOAT3 result{};
		DirectX::XMStoreFloat3(&result, value);
		return Vector3(result.x, result.y, result.z);
	}

	Vector3 up() const {
		const DirectX::XMVECTOR value = DirectX::XMVector3Rotate(
			DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
			rotationVector()
		);
		DirectX::XMFLOAT3 result{};
		DirectX::XMStoreFloat3(&result, value);
		return Vector3(result.x, result.y, result.z);
	}

	Vector3 right() const {
		const DirectX::XMVECTOR value = DirectX::XMVector3Rotate(
			DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f),
			rotationVector()
		);
		DirectX::XMFLOAT3 result{};
		DirectX::XMStoreFloat3(&result, value);
		return Vector3(result.x, result.y, result.z);
	}

	// Registry初期登録との互換経路。実装はTransformComponentOperations.hに置く。
	YAML::Node encode();
	bool decode(SceneContext* context, const YAML::Node& node);
	void inspector(SceneContext* context);

	// 既存コード向け互換API。実装はTransformMath.hへ分離済み。
	DirectX::XMMATRIX CalculateWorldMatrix(
		const TransformComponent* transform,
		ComponentRegistry* componentRegistry
	) const;

	Vector3 GetWorldPosition(
		ComponentRegistry* componentRegistry
	) const;

	// 既存コード向け互換API。実装はRectTransformUtility.hへ分離済み。
	TransformComponent CalculateRectTransform(
		const Vector2& viewportSize,
		const SpriteRendererComponent& sprite,
		const TransformComponent& originalTransform
	);

	TransformComponent ReverseCalculateRectTransform(
		const Vector2& viewportSize,
		const SpriteRendererComponent& sprite,
		const TransformComponent& adjustedTransform
	);
};

#include "Operations/TransformComponentOperations.h"
#include "Operations/TransformMath.h"
#include "Operations/RectTransformUtility.h"
