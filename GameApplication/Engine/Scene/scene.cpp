// Engine/Scene/scene.cpp
#include "scene.h"
#include "Entity/entityRegistry.h"
#include "System/transformSystem.h"
#include "System/renderSystem.h"

#include "Component/transformComponent.h"

Scene::Scene()
	: m_entityRegistry(std::make_shared<EntityRegistry>()),
	m_transformSystem(new TransformSystem(m_entityRegistry.get())),
	m_renderSystem(new RenderSystem(m_entityRegistry.get())){}

Scene::~Scene(){
	Shutdown();
}

void Scene::Initialize(){
	// 例: 1つのエンティティを作成し、TransformとMeshRendererを追加
	auto registry = GetEntityRegistry();
	EntityID entity = registry->CreateEntity();

	// TransformComponentを追加
	auto* transform = registry->AddComponent<TransformComponent>(entity);
	transform->position[0] = 0.0f;
	transform->position[1] = 1.0f;
	transform->position[2] = 0.0f;
	transform->scale[0] = 2.0f; // x方向だけスケール

	// MeshRendererComponentを追加
	//auto* meshRenderer = registry->AddComponent<MeshRendererComponent>(entity);
	// meshRenderer->mesh = ...; // メッシュやマテリアルの設定（必要に応じて）

	// 必要に応じて他のコンポーネントも追加}
}

void Scene::Update(float deltaTime){
	if(m_transformSystem) m_transformSystem->Update(deltaTime);
}

void Scene::FixedUpdate(float fixedDeltaTime){
	//if(m_physicsSystem) m_physicsSystem->FixedUpdate(fixedDeltaTime);
}

void Scene::Render(){
	if(m_renderSystem)   m_renderSystem->Render();
}

void Scene::Shutdown(){
	// システムの終了処理
	m_renderSystem.reset();
	m_transformSystem.reset();
	m_entityRegistry.reset();
}

std::shared_ptr<EntityRegistry> Scene::GetEntityRegistry(){
	return m_entityRegistry;
}
