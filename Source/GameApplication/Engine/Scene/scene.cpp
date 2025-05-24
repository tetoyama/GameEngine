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
	auto* meshRenderer = registry->AddComponent<MeshRendererComponent>(entity);

	//// --- ここでMeshを初期化 ---
	//struct Vertex {
	//	float position[3];
	//	float color[4];
	//};
	//Vertex vertices[] = {
	//	{ { 0.0f, 0.5f, 0.0f }, { 1, 0, 0, 1 } },
	//	{ { 0.5f, -0.5f, 0.0f }, { 0, 1, 0, 1 } },
	//	{ { -0.5f, -0.5f, 0.0f }, { 0, 0, 1, 1 } }
	//};
	//UINT indices[] = {0, 1, 2};

	//ID3D11Device* device = graphiccontext->GetDevice(); // 取得済みのデバイス
	//D3D11_BUFFER_DESC vbDesc = {};
	//vbDesc.Usage = D3D11_USAGE_DEFAULT;
	//vbDesc.ByteWidth = sizeof(vertices);
	//vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	//D3D11_SUBRESOURCE_DATA vbData = {vertices, 0, 0};
	//ID3D11Buffer* vertexBuffer = nullptr;
	//device->CreateBuffer(&vbDesc, &vbData, &vertexBuffer);

	//D3D11_BUFFER_DESC ibDesc = {};
	//ibDesc.Usage = D3D11_USAGE_DEFAULT;
	//ibDesc.ByteWidth = sizeof(indices);
	//ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	//D3D11_SUBRESOURCE_DATA ibData = {indices, 0, 0};
	//ID3D11Buffer* indexBuffer = nullptr;
	//device->CreateBuffer(&ibDesc, &ibData, &indexBuffer);

	//auto mesh = std::make_shared<MeshData>();
	//mesh->vertexBuffer = vertexBuffer;
	//mesh->indexBuffer = indexBuffer;
	//mesh->indexCount = 3;

	//meshRenderer = registry->AddComponent<MeshRendererComponent>(entity);
	//meshRenderer->mesh = mesh;
	//meshRenderer->material = std::make_shared<Material>();
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
