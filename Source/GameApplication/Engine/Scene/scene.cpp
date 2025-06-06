// Engine/Scene/scene.cpp

#include "scene.h"
#include "sceneManager.h"

#include <string>

#include "Entity/entityRegistry.h"

#include "System/transformSystem.h"
#include "System/renderSystem.h"
#include "System/cameraSystem.h"
#include "System/playerSystem.h"

#include "Component/modelRendererComponent.h"
#include "Component/transformComponent.h"
#include "Component/cameraComponent.h"
#include "Component/playerComponent.h"

#include "Engine/Graphics/mainRenderer.h"

#include "Engine/Platform/InputSystem/InputSystem.h"

#include "Engine/Resources/resourceSystem.h"

#include "Engine/Resources/Data/modelData.h"
#include "Engine/Resources/Loader/modelLoader.h"
#include "Engine/Resources/Loader/shaderLoader.h"
#include "Engine/Resources/Loader/textureLoader.h"
#include "Engine/Resources/Data/vertexShaderData.h"
#include "Engine/Resources/Data/pixelShaderData.h"

Scene::Scene(){

}

Scene::~Scene(){
	Shutdown();
}

void Scene::Initialize(SceneContext* set){

	m_SceneContext = set;

	m_entityRegistry = std::make_shared<EntityRegistry>();

	// コンポーネント型を登録（Archetype or Sparse を選択）
	m_entityRegistry->RegisterComponent<TransformComponent>(true);   
	m_entityRegistry->RegisterComponent<MeshRendererComponent>(false); 
	m_entityRegistry->RegisterComponent<ModelRendererComponent>(false); 
	m_entityRegistry->RegisterComponent<PlayerComponent>(false);
	m_entityRegistry->RegisterComponent<CameraComponent>(true);

	// システムを登録
	m_entityRegistry->RegisterSystem(std::make_unique<TransformSystem>(m_entityRegistry.get()));
	m_entityRegistry->RegisterSystem(std::make_unique<CameraSystem>(m_entityRegistry.get(), m_SceneContext->renderer));
	m_entityRegistry->RegisterSystem(std::make_unique<RenderSystem>(m_entityRegistry.get(), m_SceneContext->renderer));
	m_entityRegistry->RegisterSystem(std::make_unique<PlayerSystem>(m_entityRegistry.get(),m_SceneContext));

	auto Renderer = m_SceneContext->renderer;
	auto graphicsContext = Renderer->GetGraphicsContext();
	auto registry = GetEntityRegistry();
	auto resource = m_SceneContext->resource;

	LIGHT light{};
	light.Enable = TRUE;
	light.Direction = DirectX::XMFLOAT4(0.2f, -1.0f, 0.2f, 0.0f);
	light.Position = DirectX::XMFLOAT4(0, 100, 0, 0);
	light.Diffuse = DirectX::XMFLOAT4(1, 1, 1, 1);
	light.Ambient = DirectX::XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);
	light.PointLightParam = DirectX::XMFLOAT4(100.0f, 0, 0, 0);

	light.SkyColor = DirectX::XMFLOAT4(1.0f, 0.6f, 0.85f, 0.1f);
	light.GroundColor = DirectX::XMFLOAT4(0.8f, 0.8f, 1.0f, 0.5f);
	light.GroundNormal = DirectX::XMFLOAT4(0, 0, 1, 0);
	graphicsContext->SetLight(light);
	graphicsContext->SetDepthEnable(true);

	/*
	 {
		//エンティティを作成し、TransformとMeshRendererを追加
		Entity entity = registry->CreateEntity();

		// TransformComponentを追加
		auto* transform = registry->AddComponent<TransformComponent>(entity);
		transform->position.x = 0.0f;
		transform->position.y = 100.0f;
		transform->position.z = 0.0f;

		// MeshRendererComponentを追加
		auto* meshRenderer = registry->AddComponent<MeshRendererComponent>(entity);

		auto mesh = std::make_shared<MeshData>();

		mesh->meshCount = 4;
		mesh->m_TextureData = m_SceneContext->resource->GetTextureLoader()->LoadTexture(L"Asset\\Texture\\texture.jpg");
		VERTEX_3D vertex[4]{};

		vertex[0].Position = DirectX::XMFLOAT3(100.0f * 0.0f, 100.0f * 0.0f, 0.0f);
		vertex[0].Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
		vertex[0].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[0].TexCoord = DirectX::XMFLOAT2(0.0f, 0.0f);

		vertex[1].Position = DirectX::XMFLOAT3(100.0f * 1.0f, 100.0f * 0.0f, 0.0f);
		vertex[1].Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
		vertex[1].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[1].TexCoord = DirectX::XMFLOAT2(1.0f, 0.0f);

		vertex[2].Position = DirectX::XMFLOAT3(100.0f * 0.0f, 100.0f * 1.0f, 0.0f);
		vertex[2].Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
		vertex[2].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[2].TexCoord = DirectX::XMFLOAT2(0.0f, 1.0f);

		vertex[3].Position = DirectX::XMFLOAT3(100.0f * 1.0f, 100.0f * 1.0f, 0.0f);
		vertex[3].Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
		vertex[3].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[3].TexCoord = DirectX::XMFLOAT2(1.0f, 1.0f);

		D3D11_BUFFER_DESC bd{};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(VERTEX_3D) * 4;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = 0;

		D3D11_SUBRESOURCE_DATA sd{};
		sd.pSysMem = vertex;

		m_SceneContext->renderer->GetGraphicsContext()->GetDevice()->CreateBuffer(&bd, &sd, &mesh->m_VertexBuffer);
		m_SceneContext->renderer->GetGraphicsContext()->CreateVertexShader("Asset\\Shader\\unlitTextureVS.cso", &mesh->m_VertexShader, &mesh->m_VertexLayout);
		m_SceneContext->renderer->GetGraphicsContext()->CreatePixelShader("Asset\\Shader\\unlitTexturePS.cso", &mesh->m_PixelShader);

		meshRenderer->mesh = mesh;
	}
	*/

	{
		//エンティティを作成し、TransformとModelRendererを追加
		Entity entity = registry->CreateEntity();

		// TransformComponentを追加
		auto* transform = registry->AddComponent<TransformComponent>(entity);
		transform->position = Vector3(0.0f, 0.0f, 100.0f);
		transform->scale = Vector3(10.0f, 10.0f, 10.0f);
		transform->rotation = Vector3(0.0f, 0.0f, 0.0f);


		// ModelRendererComponentを追加
		auto* modelRenderer = registry->AddComponent<ModelRendererComponent>(entity);
		modelRenderer->model = resource->GetModelLoader()->LoadModel("Asset\\Model\\player.obj");
		modelRenderer->vertexShader = resource->GetShaderLoader()->LoadVertexShader("Asset\\Shader\\pixelLightingVS.cso");
		modelRenderer->pixelShader = resource->GetShaderLoader()->LoadPixelShader("Asset\\Shader\\pixelLightingPS.cso");

		auto player = registry->AddComponent<PlayerComponent>(entity);
	}

	{
		//エンティティを作成し、TransformとModelRendererを追加
		Entity entity = registry->CreateEntity();

		// TransformComponentを追加
		auto* transform = registry->AddComponent<TransformComponent>(entity);
		transform->position = Vector3(0.0f, -110.0f, 100.0f);
		transform->scale = Vector3(500.0f, 100.0f, 500.0f);
		transform->rotation = Vector3(0.0f, 0.0f, 0.0f);


		// ModelRendererComponentを追加
		auto* modelRenderer = registry->AddComponent<ModelRendererComponent>(entity);
		modelRenderer->model = resource->GetModelLoader()->LoadModel("Asset\\Model\\cube.fbx");
		modelRenderer->vertexShader = resource->GetShaderLoader()->LoadVertexShader("Asset\\Shader\\pixelLightingVS.cso");
		modelRenderer->pixelShader = resource->GetShaderLoader()->LoadPixelShader("Asset\\Shader\\pixelLightingPS.cso");
	}

	{
		//エンティティを作成し、TransformとCameraを追加
		Entity entity = registry->CreateEntity();

		// CameraComponentを追加
		auto* camera = registry->AddComponent<CameraComponent>(entity);

		// TransformComponentを追加
		auto* transform = registry->AddComponent<TransformComponent>(entity);
		transform->position = Vector3(0.0f, 10.0f, 0.0f);
		transform->scale = Vector3(1.0f, 1.0f, 1.0f);
		transform->rotation = Vector3(-1.0f, 0.0f, 0.0f);
	}

	m_entityRegistry->StartAllSystems();
}

void Scene::Update(float deltaTime){

	m_entityRegistry->UpdateAllSystems(deltaTime);
}

void Scene::FixedUpdate(float fixedDeltaTime){

}

void Scene::Render(){

	m_entityRegistry->DrawAllSystems();
}

void Scene::Shutdown(){

	// システムの終了処理
	m_entityRegistry.reset();
}
