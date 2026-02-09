#pragma once
#include "Scene/Entity/Entity.h"

struct SceneContext;

class CameraComponent;
class TransformComponent;

struct CameraEntityData {

	CameraComponent* CameraComponent = nullptr;
	TransformComponent* transformComponent = nullptr;

	// 念のためエンティティも保持
	Entity entity = 0;
	SceneContext* sceneContext = nullptr;
};
