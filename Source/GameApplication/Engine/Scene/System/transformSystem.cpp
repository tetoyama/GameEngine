// Engine/Scene/System/transformSystem.cpp
#include "transformSystem.h"
#include "Entity/entityRegistry.h"

TransformSystem::TransformSystem(EntityRegistry* registry)
	: m_registry(registry){}

TransformSystem::~TransformSystem(){}

void TransformSystem::Update(float deltaTime){
	// TransformComponentを持つ全エンティティの更新処理をここに記述
	// 例: 物理やスクリプトによるTransformの変更など
}
