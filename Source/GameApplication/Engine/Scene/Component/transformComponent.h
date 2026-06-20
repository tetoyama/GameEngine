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

// Entityのローカル変換と階層参照を保持するComponent。
//
// YAML、Inspector、階層行列、UI Rect変換の実装はOperations配下へ分離する。
// 既存呼び出し元との互換性のため、移行期間中は薄いメンバー関数を残す。
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
		Vector3 euler = GetRotationEuler();
		euler.x = value;
		SetRotationEuler(euler);
	}

	void SetRotationY(float value){
		Vector3 euler = GetRotationEuler();
		euler.y = value;
		SetRotationEuler(euler);
	}

	void SetRotationZ(float value){
		Vector3 euler = GetRotationEuler();
		euler.z = value;
		SetRotationEuler(euler);
	}

	void AddRotationX(float value){
		Vector3 euler = GetRotationEuler();
		euler.x += value;
		SetRotationEuler(euler);
	}

	void AddRotationY(float value){
		Vector3 euler = GetRotationEuler();
		euler.y += value;
		SetRotationEuler(euler);
	}

	void AddRotationZ(float value){
		Vector3 euler = GetRotationEuler();
		euler.z += value;
		SetRotationEuler(euler);
	}

	void SetRotationEuler(const Vector3& euler){
		rotationEuler = euler;

		const DirectX::XMVECTOR quaternion =
			DirectX::XMQuaternionRotationRollPitchYaw(
				euler.x,
				euler.y,
				euler.z
			);
		DirectX::XMStoreFloat4(&rotation, quaternion);
	}

	void SetRotation(const DirectX::XMFLOAT4 quaternion){
		rotation = quaternion;
		rotationEuler = GetRotationEuler();
	}

	const DirectX::XMFLOAT4& GetRotation() const {
		return rotation;
	}

	Vector3 GetRotationEuler() const {
		DirectX::XMFLOAT4 quaternion{};
		DirectX::XMStoreFloat4(&quaternion, rotationVector());

		const float ySquared = quaternion.y * quaternion.y;

		const float t0 =
			2.0f * (quaternion.w * quaternion.x + quaternion.y * quaternion.z);
		const float t1 =
			1.0f - 2.0f * (quaternion.x * quaternion.x + ySquared);
		const float roll = atan2f(t0, t1);

		float t2 =
			2.0f * (quaternion.w * quaternion.y - quaternion.z * quaternion.x);
		if(t2 > 1.0f) t2 = 1.0f;
		if(t2 < -1.0f) t2 = -1.0f;
		const float pitch = asinf(t2);

		const float t3 =
			2.0f * (quaternion.w * quaternion.z + quaternion.x * quaternion.y);
		const float t4 =
			1.0f - 2.0f * (ySquared + quaternion.z * quaternion.z);
		const float yaw = atan2f(t3, t4);

		return Vector3(roll, pitch, yaw);
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