// Engine/Scene/scene.cpp

#include "scene.h"

#include <string>
#include "Engine/DebugTools/debugSystem.h"

#include "Engine/Graphics/mainRenderer.h"

#include "Engine/Platform/InputSystem/InputSystem.h"

#include "Engine/Resources/resourceSystem.h"
#include "Engine/Resources/Data/modelData.h"
#include "Engine/Resources/Loader/modelLoader.h"
#include "Engine/Resources/Loader/shaderLoader.h"
#include "Engine/Resources/Loader/textureLoader.h"
#include "Engine/Resources/Data/vertexShaderData.h"
#include "Engine/Resources/Data/pixelShaderData.h"

#include "sceneManager.h"

#include "Registry/entityRegistry.h"
#include "Registry/componentRegistry.h"
#include "Registry/systemRegistry.h"

#include "System/transformSystem.h"
#include "System/renderSystem.h"
#include "System/cameraSystem.h"
#include "System/playerSystem.h"

#include "Component/modelRendererComponent.h"
#include "Component/meshRendererComponent.h"
#include "Component/transformComponent.h"
#include "Component/cameraComponent.h"
#include "Component/playerComponent.h"

Scene::Scene(){

}

Scene::~Scene(){
}

void Scene::Initialize(SceneManagerContext* set){

	m_SceneManagerContext = set;
	m_SceneManagerContext->debug->LOG_INFO(u8"Sceneを初期化中...");

	m_entityRegistry = std::make_shared<EntityRegistry>();
	m_componentRegistry = std::make_shared<ComponentRegistry>(m_entityRegistry.get());
	m_systemRegistry = std::make_shared<SystemRegistry>();

	// コンポーネントを登録（Archetype or Sparse を選択）
	m_componentRegistry->RegisterComponent<TransformComponent>(true);   
	m_componentRegistry->RegisterComponent<MeshRendererComponent>(true);
	m_componentRegistry->RegisterComponent<ModelRendererComponent>(true);
	m_componentRegistry->RegisterComponent<PlayerComponent>(false);
	m_componentRegistry->RegisterComponent<CameraComponent>(true);

	// システムを登録
	m_systemRegistry->RegisterSystem(std::make_unique<TransformSystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<CameraSystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<RenderSystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<PlayerSystem>(&m_SceneContext));

	auto Renderer = m_SceneManagerContext->renderer;
	auto graphicsContext = Renderer->GetGraphicsContext();
	auto resource = m_SceneManagerContext->resource;

	m_SceneContext.entity = m_entityRegistry.get();
	m_SceneContext.component = m_componentRegistry.get();
	m_SceneContext.system = m_systemRegistry.get();
	m_SceneContext.manager = m_SceneManagerContext;

	auto entityRegistry = m_SceneContext.entity;
	auto componentRegistry = m_SceneContext.component;
	auto systemRegistry = m_SceneContext.system;

	m_systemRegistry->InitializeAll();



	LIGHT light{};
	light.Enable = TRUE;
	light.Direction = DirectX::XMFLOAT4(0.2f, -1.0f, 0.2f, 0.0f);
	light.Position = DirectX::XMFLOAT4(0, 3, 0,0);
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
		mesh->m_TextureData = m_SceneManagerContext->resource->GetTextureLoader()->LoadTexture(L"Asset\\Texture\\texture.jpg");
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

		m_SceneManagerContext->renderer->GetGraphicsContext()->GetDevice()->CreateBuffer(&bd, &sd, &mesh->m_VertexBuffer);
		m_SceneManagerContext->renderer->GetGraphicsContext()->CreateVertexShader("Asset\\Shader\\unlitTextureVS.cso", &mesh->m_VertexShader, &mesh->m_VertexLayout);
		m_SceneManagerContext->renderer->GetGraphicsContext()->CreatePixelShader("Asset\\Shader\\unlitTexturePS.cso", &mesh->m_PixelShader);

		meshRenderer->mesh = mesh;
	}
	*/

	{
		//エンティティを作成し、TransformとModelRendererを追加
		Entity entity = entityRegistry->Create();

		// TransformComponentを追加
		auto* transform = componentRegistry->AddComponent<TransformComponent>(entity);
		transform->position = Vector3(0.0f, 0.0f, 0);
		transform->scale = Vector3(1.0f, 1.0f, 1.0f);
		transform->rotation = Vector3(0.0f, 0.0f, 0.0f);


		// ModelRendererComponentを追加
		auto* modelRenderer = componentRegistry->AddComponent<ModelRendererComponent>(entity);
		modelRenderer->model = resource->GetModelLoader()->LoadModel("Asset\\Model\\player.obj");
		modelRenderer->vertexShader = resource->GetShaderLoader()->LoadVertexShader("Asset\\Shader\\pointLightingBlinnPhongVS.cso");
		modelRenderer->pixelShader = resource->GetShaderLoader()->LoadPixelShader("Asset\\Shader\\pointLightingBlinnPhongPS.cso");

		auto player = componentRegistry->AddComponent<PlayerComponent>(entity);
	}

	for (int i = 0; i < 1; i++) {

		//エンティティを作成し、TransformとModelRendererを追加
		Entity entity = entityRegistry->Create();

		// TransformComponentを追加
		auto* transform = componentRegistry->AddComponent<TransformComponent>(entity);
		transform->position = Vector3(0.0f, -100.0f, 0);
		transform->scale = Vector3(100.0f, 100.0f, 100.0f);
		transform->rotation = Vector3(0.0f, 0.0f, 0.0f);


		// ModelRendererComponentを追加
		auto* modelRenderer = componentRegistry->AddComponent<ModelRendererComponent>(entity);
		modelRenderer->model = resource->GetModelLoader()->LoadModel("Asset\\Model\\cube.fbx");
		modelRenderer->vertexShader = resource->GetShaderLoader()->LoadVertexShader("Asset\\Shader\\pointLightingBlinnPhongVS.cso");
		modelRenderer->pixelShader = resource->GetShaderLoader()->LoadPixelShader("Asset\\Shader\\pointLightingBlinnPhongPS.cso");
	}

	{
		//エンティティを作成し、TransformとCameraを追加
		Entity entity = entityRegistry->Create();

		// CameraComponentを追加
		auto* camera = componentRegistry->AddComponent<CameraComponent>(entity);

		// TransformComponentを追加
		auto* transform = componentRegistry->AddComponent<TransformComponent>(entity);
		transform->position = Vector3(0.0f, 10.0f, 0.0f);
		transform->scale = Vector3(1.0f, 1.0f, 1.0f);
		transform->rotation = Vector3(-1.0f, 0.0f, 0.0f);
	}
	m_SceneManagerContext->debug->LOG_INFO(u8"Sceneを開始します");

	m_systemRegistry->StartAll();
}

void Scene::Update(float deltaTime){

	m_systemRegistry->UpdateAll(deltaTime);
}

void Scene::FixedUpdate(float fixedDeltaTime){

	m_systemRegistry->FixedUpdateAll(fixedDeltaTime);
}

void Scene::Render(){

	m_systemRegistry->DrawAll();
}

void Scene::Shutdown(){
	m_SceneManagerContext->debug->LOG_INFO(u8"Sceneを終了中...");

	// システムの終了処理
	m_entityRegistry.reset();
	m_componentRegistry.reset();
	m_systemRegistry.reset();
}
