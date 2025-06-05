#pragma once
#include "IComponent.h"
#include "Backends/myVector3.h"

class TransformComponent : public IComponent{
public:
	Vector3 position = Vector3(0, 0, 0);
	Vector3 rotation = Vector3(0, 0, 0);
	Vector3 scale = Vector3(1, 1, 1);

	Vector3 front() const {
		float pitch = rotation.x;
		float yaw = rotation.y;
		float roll = rotation.z;

		float cosPitch = cos(pitch);
		float sinPitch = sin(pitch);
		float cosYaw = cos(yaw);
		float sinYaw = sin(yaw);
		float cosRoll = cos(roll);
		float sinRoll = sin(roll);

		return Vector3(
			cosPitch * sinYaw,
			sinPitch,
			cosPitch * cosYaw
		);
	}

	Vector3 up() const {
		float pitch = rotation.x;
		float yaw = rotation.y;
		float roll = rotation.z;

		float cosPitch = cos(pitch);
		float sinPitch = sin(pitch);
		float cosYaw = cos(yaw);
		float sinYaw = sin(yaw);
		float cosRoll = cos(roll);
		float sinRoll = sin(roll);

		return Vector3(
			-cosPitch * sinYaw * sinRoll + sinPitch * cosRoll,
			cosPitch * cosRoll + sinPitch * sinYaw * sinRoll,
			cosYaw * sinPitch
		);
	}

	Vector3 right() const {
		float pitch = rotation.x;
		float yaw = rotation.y;
		float roll = rotation.z;

		float cosPitch = cos(pitch);
		float sinPitch = sin(pitch);
		float cosYaw = cos(yaw);
		float sinYaw = sin(yaw);
		float cosRoll = cos(roll);
		float sinRoll = sin(roll);

		return Vector3(
			cosPitch * cosYaw,
			-sinPitch,
			cosPitch * sinYaw
		);
	}
};
