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

#include "System/inspectorSystem.h"
#include "System/transformSystem.h"
#include "System/renderSystem.h"
#include "System/cameraSystem.h"

#include "System/playerSystem.h"
#include "System/bulletSystem.h"
#include "System/enemySystem.h"
#include "System/explosionEffectSystem.h"

#include "Component/entityNameComponent.h"
#include "Component/transformComponent.h"
#include "Component/cameraComponent.h"
#include "Component/modelRendererComponent.h"
#include "Component/meshRendererComponent.h"
#include "Component/BillBoardRendererComponent.h"
#include "Component/textureComponent.h"

#include "Component/playerComponent.h"
#include "Component/bulletComponent.h"
#include "Component/enemyComponent.h"
#include "Component/explosionEffectComponent.h"

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
	// ボトルネックが見つかったコンポーネントから ArchetypeStorage<T> に移行
	m_componentRegistry->RegisterComponent<NameComponent>(false);
	m_componentRegistry->RegisterComponent<TransformComponent>(false);
	m_componentRegistry->RegisterComponent<CameraComponent>(false);

	m_componentRegistry->RegisterComponent<TextureComponent>(false);
	m_componentRegistry->RegisterComponent<MeshRendererComponent>(false);
	m_componentRegistry->RegisterComponent<ModelRendererComponent>(false);
	m_componentRegistry->RegisterComponent<BillBoardRendererComponent>(false);

	m_componentRegistry->RegisterComponent<PlayerComponent>(false);
	m_componentRegistry->RegisterComponent<BulletComponent>(false);
	m_componentRegistry->RegisterComponent<EnemyComponent>(false);
	m_componentRegistry->RegisterComponent<ExplosionEffectComponent>(false);

	// システムを登録
	m_systemRegistry->RegisterSystem(std::make_unique<InspectorSystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<TransformSystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<CameraSystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<RenderSystem>(&m_SceneContext));

	m_systemRegistry->RegisterSystem(std::make_unique<PlayerSystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<BulletSystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<EnemySystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<ExplosionEffectSystem>(&m_SceneContext));

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
	light.Direction = DirectX::XMFLOAT4(0.0f, -1.0f, 0.0f, 0.0f);
	light.Position = DirectX::XMFLOAT4(0, 5, 0,0);
	light.Diffuse = DirectX::XMFLOAT4(0.9f, 0.9f, 1.0f, 1);
	light.Ambient = DirectX::XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);
	light.PointLightParam = DirectX::XMFLOAT4(20.0f, 0, 0, 0);
	light.Angle = DirectX::XMFLOAT4(DirectX::XM_PI / 180.0f * 60.0f, 0.0f, 0.0f, 0.0f);

	light.SkyColor = DirectX::XMFLOAT4(0.8f, 0.8f, 1.0f, 0.1f);
	light.GroundColor = DirectX::XMFLOAT4(1.0f, 0.8f, 0.5f, 0.05f);
	light.GroundNormal = DirectX::XMFLOAT4(0, 0, 1, 0);
	graphicsContext->SetLight(light);
	graphicsContext->SetDepthEnable(true);

	{
		//エンティティを作成し、TransformとModelRendererを追加
		Entity entity = entityRegistry->Create();

		auto* name = componentRegistry->AddComponent<NameComponent>(entity);
		name->name = "Field";

		// TransformComponentを追加
		auto* transform = componentRegistry->AddComponent<TransformComponent>(entity);
		transform->scale = Vector3(100.0f, 10.0f, 100.0f);

		transform->position = Vector3(0.0f, -transform->scale.y, 0);
		transform->rotation = Vector3(0.0f, 0.0f, 0.0f);


		// ModelRendererComponentを追加
		auto* modelRenderer = componentRegistry->AddComponent<ModelRendererComponent>(entity);
		modelRenderer->model = resource->GetModelLoader()->LoadModel("Asset\\Model\\cube.fbx");
		modelRenderer->vertexShader = resource->GetShaderLoader()->LoadVertexShader("Asset\\Shader\\pointLightingBlinnPhongVS.cso");
		modelRenderer->pixelShader = resource->GetShaderLoader()->LoadPixelShader("Asset\\Shader\\pointLightingBlinnPhongPS.cso");
	}

	{
		//エンティティを作成し、TransformとModelRendererを追加
		Entity entity = entityRegistry->Create();

		auto* name = componentRegistry->AddComponent<NameComponent>(entity);
		name->name = "Player";

		auto* texture = componentRegistry->AddComponent<TextureComponent>(entity);
		texture->m_TextureData = m_SceneManagerContext->resource->GetTextureLoader()->LoadTexture(L"Asset\\Texture\\white.tga");
		texture->Material.Diffuse = DirectX::XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);

		// TransformComponentを追加
		auto* transform = componentRegistry->AddComponent<TransformComponent>(entity);
		transform->position = Vector3(0.0f, 0.2f, 0);
		transform->scale = Vector3(1.0f, 1.0f, 1.0f);
		transform->rotation = Vector3(0.0f, 0.0f, 0.0f);


		// ModelRendererComponentを追加
		auto* modelRenderer = componentRegistry->AddComponent<ModelRendererComponent>(entity);
		modelRenderer->model = resource->GetModelLoader()->LoadModel("Asset\\Model\\player.obj");
		modelRenderer->vertexShader = resource->GetShaderLoader()->LoadVertexShader("Asset\\Shader\\limLightVS.cso");
		modelRenderer->pixelShader = resource->GetShaderLoader()->LoadPixelShader("Asset\\Shader\\limLightPS.cso");

		auto player = componentRegistry->AddComponent<PlayerComponent>(entity);
	}

	{
		//エンティティを作成し、TransformとCameraを追加
		Entity entity = entityRegistry->Create();

		auto* name = componentRegistry->AddComponent<NameComponent>(entity);
		name->name = "Camera";

		// TransformComponentを追加
		auto* transform = componentRegistry->AddComponent<TransformComponent>(entity);
		transform->position = Vector3(0.0f, 10.0f, 5.0f);
		transform->scale = Vector3(1.0f, 1.0f, 1.0f);
		transform->rotation = Vector3(-1.0f, 0.0f, 0.0f);


		// CameraComponentを追加
		auto* camera = componentRegistry->AddComponent<CameraComponent>(entity);
	}

	int Sample = 20;
	float Distance = 10.0f;
	for(int i = 0; i < Sample; i++){
	
		//エンティティを作成し、TransformとModelRendererを追加
		Entity entity = entityRegistry->Create();

		auto* name = componentRegistry->AddComponent<NameComponent>(entity);
		name->name = "Enemy";

		auto* texture = componentRegistry->AddComponent<TextureComponent>(entity);
		texture->m_TextureData = m_SceneManagerContext->resource->GetTextureLoader()->LoadTexture(L"Asset\\Texture\\white.tga");
		texture->Material.Diffuse = DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);

		// TransformComponentを追加
		auto* transform = componentRegistry->AddComponent<TransformComponent>(entity);
		transform->position = Vector3(cosf(float(i) / Sample * DirectX::XM_2PI) * Distance, 0.2f, sinf(float(i) / Sample * DirectX::XM_2PI) * Distance);
		transform->scale = Vector3(1.0f, 1.0f, 1.0f);
		transform->rotation = Vector3(0.0f, 0.0f, 0.0f);


		// ModelRendererComponentを追加
		auto* modelRenderer = componentRegistry->AddComponent<ModelRendererComponent>(entity);
		modelRenderer->model = resource->GetModelLoader()->LoadModel("Asset\\Model\\player.obj");
		modelRenderer->vertexShader = resource->GetShaderLoader()->LoadVertexShader("Asset\\Shader\\limLightVS.cso");
		modelRenderer->pixelShader = resource->GetShaderLoader()->LoadPixelShader("Asset\\Shader\\limLightPS.cso");

		auto player = componentRegistry->AddComponent<EnemyComponent>(entity);
	}

	{
		Entity entity = entityRegistry->Create();

		auto* name = componentRegistry->AddComponent<NameComponent>(entity);
		name->name = "BillBoard";

		auto* bill = componentRegistry->AddComponent<BillBoardRendererComponent>(entity);

		auto* transform = componentRegistry->AddComponent<TransformComponent>(entity);
		transform->position = Vector3(0.0f, 5.0f, 0.0f);
		transform->scale = Vector3(10.0f, 10.0f, 10.0f);

		auto* texture = componentRegistry->AddComponent<TextureComponent>(entity);
		texture->m_TextureData = m_SceneManagerContext->resource->GetTextureLoader()->LoadTexture(L"Asset\\Texture\\explosion.png");
		//texture->m_TextureData = m_SceneManagerContext->resource->GetTextureLoader()->LoadTexture(L"Asset\\Texture\\texture.jpg");
		texture->Material.Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		texture->UV_Slice_X = 4;
		texture->UV_Slice_Y = 4;
		texture->AnimationNum = 10;

		auto* meshRenderer = componentRegistry->AddComponent<MeshRendererComponent>(entity);

		meshRenderer->mesh.meshCount = 4;

		VERTEX_3D vertex[4]{};

		vertex[0].Position = DirectX::XMFLOAT3(-0.5f, 0.5f, 0.0f);
		vertex[0].Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
		vertex[0].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[0].TexCoord = DirectX::XMFLOAT2(0.0f, 0.0f);

		vertex[1].Position = DirectX::XMFLOAT3(0.5f, 0.5f, 0.0f);
		vertex[1].Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
		vertex[1].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[1].TexCoord = DirectX::XMFLOAT2(1.0f, 0.0f);

		vertex[2].Position = DirectX::XMFLOAT3(-0.5f, -0.5f, 0.0f);
		vertex[2].Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
		vertex[2].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[2].TexCoord = DirectX::XMFLOAT2(0.0f, 1.0f);

		vertex[3].Position = DirectX::XMFLOAT3(0.5f, -0.5f, 0.0f);
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

		m_SceneManagerContext->renderer->GetGraphicsContext()->GetDevice()->CreateBuffer(&bd, &sd, meshRenderer->mesh.m_VertexBuffer.GetAddressOf());
		m_SceneManagerContext->renderer->GetGraphicsContext()->CreateVertexShader("Asset\\Shader\\unlitUVTextureVS.cso", meshRenderer->mesh.m_VertexShader.GetAddressOf(), meshRenderer->mesh.m_VertexLayout.GetAddressOf());
		m_SceneManagerContext->renderer->GetGraphicsContext()->CreatePixelShader("Asset\\Shader\\unlitUVTexturePS.cso", meshRenderer->mesh.m_PixelShader.GetAddressOf());
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
