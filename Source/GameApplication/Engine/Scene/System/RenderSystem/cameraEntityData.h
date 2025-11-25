#pragma once
#include "GameApplication/Engine/Scene/Entity/Entity.h"

struct SceneContext;

class CameraComponent;
class TransformComponent;

struct CameraEntityData {

	CameraComponent* cameraComponent = nullptr;
	TransformComponent* transformComponent = nullptr;

	// 念のためエンティティも保持
	Entity entity = 0;
	SceneContext* sceneContext = nullptr;
};
