// Engine/Scene/System/renderSystem.cpp
#include "renderSystem.h"
#include "Entity/entityRegistry.h"

RenderSystem::RenderSystem(EntityRegistry* registry)
	: m_registry(registry){}

RenderSystem::~RenderSystem(){}

void RenderSystem::Render(){
	// MeshRendererComponent等を持つ全エンティティの描画処理をここに記述
	// 例: 描画コマンドの発行など
}
