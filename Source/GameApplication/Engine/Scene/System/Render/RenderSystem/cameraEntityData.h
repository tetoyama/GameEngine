#pragma once
#include "Scene/Entity/EntityRef.h"

struct SceneContext;

class CameraComponent;
class TransformComponent;

struct CameraEntityData {

	CameraComponent* CameraComponent = nullptr;
	TransformComponent* transformComponent = nullptr;

	// エンティティへの安全なリファレンス
	EntityRef ref;
};
