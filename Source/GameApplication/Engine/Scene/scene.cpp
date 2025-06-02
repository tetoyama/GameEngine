// Engine/Scene/scene.cpp
#include "_Old/model.h"

#include "scene.h"
#include "Entity/entityRegistry.h"
#include "System/transformSystem.h"
#include "System/renderSystem.h"
#include "Engine/Graphics/mainRenderer.h"
#include "Component/transformComponent.h"
#include "Engine/Resources/TextureLoader.h"

static MODEL* g_Model = nullptr;
static float Timer = 0.0f;
Scene::Scene(){

}

Scene::~Scene(){
	Shutdown();
}

void Scene::Initialize(GraphicsContext* graphiccontext, MainRenderer* mainRenderer){
	m_GC = graphiccontext;

	InitModel(graphiccontext);
	g_Model = LoadModel("Asset\\Model\\model.fbx");

	mainRenderer->GetGraphicsContext()->CreateVertexShader("Asset\\Shader\\pixelLightingBlinPhongVS.cso", &m_VertexShader, &m_VertexLayout);
	mainRenderer->GetGraphicsContext()->CreatePixelShader("Asset\\Shader\\pixelLightingBlinPhongPS.cso", &m_PixelShader);

	m_entityRegistry = std::make_shared<EntityRegistry>();

	m_transformSystem = std::make_unique<TransformSystem>(m_entityRegistry.get());
	m_renderSystem = std::make_unique<RenderSystem>(m_entityRegistry.get(), mainRenderer);

	m_renderSystem->Initialize();

	auto registry = GetEntityRegistry();

	{
		//エンティティを作成し、TransformとMeshRendererを追加
		EntityID entity = registry->CreateEntity();

		// TransformComponentを追加
		auto* transform = registry->AddComponent<TransformComponent>(entity);
		transform->position.x = 0.0f;
		transform->position.y = 100.0f;
		transform->position.z = 0.0f;

		// MeshRendererComponentを追加
		auto* meshRenderer = registry->AddComponent<MeshRendererComponent>(entity);

		auto mesh = std::make_shared<MeshData>();

		mesh->meshCount = 4;
		mesh->m_TextureID = LoadTexture(L"Asset\\Texture\\texture.jpg");
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

		mainRenderer->GetGraphicsContext()->GetDevice()->CreateBuffer(&bd, &sd, &mesh->m_VertexBuffer);
		mainRenderer->GetGraphicsContext()->CreateVertexShader("Asset\\Shader\\unlitTextureVS.cso", &mesh->m_VertexShader, &mesh->m_VertexLayout);
		mainRenderer->GetGraphicsContext()->CreatePixelShader("Asset\\Shader\\unlitTexturePS.cso", &mesh->m_PixelShader);

		meshRenderer->mesh = mesh;
	}
}

void Scene::Update(float deltaTime){
	Timer += deltaTime;
	if(m_transformSystem) m_transformSystem->Update(deltaTime);
}

void Scene::FixedUpdate(float fixedDeltaTime){
	//if(m_physicsSystem) m_physicsSystem->FixedUpdate(fixedDeltaTime);
}

void Scene::Render(){
	if(m_renderSystem){
		m_renderSystem->Draw();
	}

	Camera camera;
	camera.CameraPosition = {0.0f,0.0f,0.0f,0.0f};
	m_GC->SetCamera(camera);

	LIGHT light = {};
	light.Enable = TRUE;
	light.Position = DirectX::XMFLOAT4(0, 30, 0, 1);
	light.Diffuse = DirectX::XMFLOAT4(1, 1, 1, 1);
	light.Ambient = DirectX::XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);
	light.PointLightParam = DirectX::XMFLOAT4(50.0f, 0, 0, 0);

	light.SkyColor = DirectX::XMFLOAT4(0.4f, 0.6f, 1.0f, 1.0f);
	light.GroundColor = DirectX::XMFLOAT4(0.1f, 0.05f, 0.03f, 1.0f);
	light.GroundNormal = DirectX::XMFLOAT4(0, 1, 0, 0);

	DirectX::XMMATRIX projection;
	projection = DirectX::XMMatrixPerspectiveFovLH(1.0f, (float)m_GC->m_width / m_GC->m_height, 0.01f, 10000.0f);

	m_GC->SetProjectionMatrix(projection);
	m_GC->SetViewMatrix(DirectX::XMMatrixLookAtLH({0.0f,0.0f,0.0f}, {0.0,0.0f,1.0f}, {0.0f,1.0f,0.0f}));
	m_GC->SetDepthEnable(true);

	DrawModel(DirectX::XMMatrixScaling(10.0f, 10.0f, 10.0f) * DirectX::XMMatrixRotationRollPitchYaw(Timer, Timer, 0) * DirectX::XMMatrixTranslation(-60, 0, 100), g_Model);

	m_GC->GetDeviceContext()->IASetInputLayout(m_VertexLayout);
	m_GC->GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
	m_GC->GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);

	DrawModel(DirectX::XMMatrixScaling(10.0f, 10.0f, 10.0f) * DirectX::XMMatrixRotationRollPitchYaw(Timer, -Timer, 0) * DirectX::XMMatrixTranslation(-30, 0, 100), g_Model);

	DrawModel(DirectX::XMMatrixScaling(10.0f, 10.0f, 10.0f) * DirectX::XMMatrixRotationRollPitchYaw(-Timer, -Timer, 0) * DirectX::XMMatrixTranslation(0, 0, 100), g_Model);

	DrawModel(DirectX::XMMatrixScaling(10.0f, 10.0f, 10.0f) * DirectX::XMMatrixRotationRollPitchYaw(-Timer, Timer, 0) * DirectX::XMMatrixTranslation(30, 0, 100), g_Model);

	DrawModel(DirectX::XMMatrixScaling(10.0f, 10.0f, 10.0f) * DirectX::XMMatrixRotationRollPitchYaw(Timer, Timer, 0) * DirectX::XMMatrixTranslation(60, 0, 100), g_Model);
}

void Scene::Shutdown(){
	//ReleaseModel(g_Model);

	// システムの終了処理
	m_renderSystem.reset();
	m_transformSystem.reset();
	m_entityRegistry.reset();
}

std::shared_ptr<EntityRegistry> Scene::GetEntityRegistry(){
	return m_entityRegistry;
}
