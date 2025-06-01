// Engine/Scene/scene.cpp
#include "scene.h"
#include "Entity/entityRegistry.h"
#include "System/transformSystem.h"
#include "System/renderSystem.h"
#include "Engine/Graphics/mainRenderer.h"
#include "Component/transformComponent.h"

Scene::Scene(){

}

Scene::~Scene(){
	Shutdown();
}

void Scene::Initialize(GraphicsContext* graphiccontext, MainRenderer* mainRenderer){

	m_entityRegistry = std::make_shared<EntityRegistry>();

	m_transformSystem = std::make_unique<TransformSystem>(m_entityRegistry.get());
	m_renderSystem = std::make_unique<RenderSystem>(m_entityRegistry.get(), mainRenderer);

	m_renderSystem->Initialize();

	// 例: 1つのエンティティを作成し、TransformとMeshRendererを追加
	auto registry = GetEntityRegistry();
	EntityID entity = registry->CreateEntity();

	// TransformComponentを追加
	auto* transform = registry->AddComponent<TransformComponent>(entity);
	transform->position.x = 0.0f;
	transform->position.y = 1.0f;
	transform->position.z = 0.0f;
	transform->scale.x = 2.0f; // x方向だけスケール

	// MeshRendererComponentを追加
	auto* meshRenderer = registry->AddComponent<MeshRendererComponent>(entity);

	auto mesh = std::make_shared<MeshData>();

	meshRenderer = registry->AddComponent<MeshRendererComponent>(entity);
	meshRenderer->mesh = mesh;
}

void Scene::Update(float deltaTime){
	if(m_transformSystem) m_transformSystem->Update(deltaTime);
}

void Scene::FixedUpdate(float fixedDeltaTime){
	//if(m_physicsSystem) m_physicsSystem->FixedUpdate(fixedDeltaTime);
}

void Scene::Render(){
	if(m_renderSystem){
		m_renderSystem->Draw();
	}
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
