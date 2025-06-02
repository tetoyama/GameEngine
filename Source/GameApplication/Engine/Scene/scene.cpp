// Engine/Scene/scene.cpp
#include "_Old/model.h"

#include "scene.h"
#include "Entity/entityRegistry.h"
#include "System/transformSystem.h"
#include "System/renderSystem.h"
#include "Engine/Graphics/mainRenderer.h"
#include "Component/transformComponent.h"
#include "Engine/Resources/TextureLoader.h"

#include "Engine/Platform/InputSystem/InputSystem.h"
#include <string>



static MODEL* g_Model = nullptr;
static float Timer = 0.0f;
static GraphicsContext* pGraphicContext = nullptr;
static float XPos = 0.0f;
class CSO
{
public:
	CSO(){}
	~CSO(){}

	void Init(const std::string filename){
		std::string VS = "VS.cso";
		std::string PS = "PS.cso";
		pGraphicContext->CreateVertexShader((filename + VS).c_str(), &m_VertexShader, &m_VertexLayout);
		pGraphicContext->CreatePixelShader((filename + PS).c_str(), &m_PixelShader);
	}

	void Load(){
		pGraphicContext->GetDeviceContext()->IASetInputLayout(m_VertexLayout);
		pGraphicContext->GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
		pGraphicContext->GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);
	}
private:
	ID3D11VertexShader* m_VertexShader;
	ID3D11PixelShader* m_PixelShader;
	ID3D11InputLayout* m_VertexLayout;
};

static CSO unlitTexture;
static CSO pixelLighting;
static CSO pixelLightingBlinnPhong;
static CSO vertexDirectionalLighting;
static CSO pointLightingBlinnPhong;
Scene::Scene(){

}

Scene::~Scene(){
	Shutdown();
}

void Scene::Initialize(GraphicsContext* graphiccontext, MainRenderer* mainRenderer, InputSystem* inputsystem){
	pGraphicContext = graphiccontext;
	m_InputSystem = inputsystem;
	m_MainRenderer = mainRenderer;
	InitModel(graphiccontext);
	g_Model = LoadModel("Asset\\Model\\model.fbx");

	unlitTexture.Init("Asset\\Shader\\unlitTexture");
	pixelLighting.Init("Asset\\Shader\\pixelLighting");
	pixelLightingBlinnPhong.Init("Asset\\Shader\\pixelLightingBlinnPhong");
	vertexDirectionalLighting.Init("Asset\\Shader\\vertexDirectionalLighting");
	pointLightingBlinnPhong.Init("Asset\\Shader\\pointLightingBlinnPhong");

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
	XPos += deltaTime * (m_InputSystem->IsKey(m_MainRenderer->GetHWND(), VK_RIGHT) - m_InputSystem->IsKey(m_MainRenderer->GetHWND(), VK_LEFT)) * 50.0f;
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
	pGraphicContext->SetCamera(camera);

	LIGHT light = {};
	light.Enable = TRUE;
	light.Position = DirectX::XMFLOAT4(0, 0, 100, 1);
	light.Diffuse = DirectX::XMFLOAT4(1, 1, 1, 1);
	light.Ambient = DirectX::XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);
	light.PointLightParam = DirectX::XMFLOAT4(100.0f, 0, 0, 0);

	light.SkyColor = DirectX::XMFLOAT4(0.4f, 0.6f, 1.0f, 1.0f);
	light.GroundColor = DirectX::XMFLOAT4(0.1f, 0.2f, 0.1f, 1.0f);
	light.GroundNormal = DirectX::XMFLOAT4(0, 1, 0, 0);

	DirectX::XMMATRIX projection;
	projection = DirectX::XMMatrixPerspectiveFovLH(1.0f, (float)pGraphicContext->m_width / pGraphicContext->m_height, 0.01f, 10000.0f);

	pGraphicContext->SetProjectionMatrix(projection);
	pGraphicContext->SetViewMatrix(DirectX::XMMatrixLookAtLH({0.0f,0.0f,0.0f}, {0.0,0.0f,1.0f}, {0.0f,1.0f,0.0f}));
	pGraphicContext->SetDepthEnable(true);
	unlitTexture.Load();

	DrawModel(DirectX::XMMatrixScaling(10.0f, 10.0f, 10.0f) * DirectX::XMMatrixRotationRollPitchYaw(Timer, Timer, 0) * DirectX::XMMatrixTranslation(XPos - 60, 0, 100), g_Model);
	
	vertexDirectionalLighting.Load();


	DrawModel(DirectX::XMMatrixScaling(10.0f, 10.0f, 10.0f) * DirectX::XMMatrixRotationRollPitchYaw(Timer, -Timer, 0) * DirectX::XMMatrixTranslation(XPos - 30, 0, 100), g_Model);
	pixelLighting.Load();

	DrawModel(DirectX::XMMatrixScaling(10.0f, 10.0f, 10.0f) * DirectX::XMMatrixRotationRollPitchYaw(-Timer, -Timer, 0) * DirectX::XMMatrixTranslation(XPos, 0, 100), g_Model);
	pixelLightingBlinnPhong.Load();

	DrawModel(DirectX::XMMatrixScaling(10.0f, 10.0f, 10.0f) * DirectX::XMMatrixRotationRollPitchYaw(-Timer, Timer, 0) * DirectX::XMMatrixTranslation(XPos + 30, 0, 100), g_Model);
	pointLightingBlinnPhong.Load();
	DrawModel(DirectX::XMMatrixScaling(10.0f, 10.0f, 10.0f) * DirectX::XMMatrixRotationRollPitchYaw(Timer, Timer, 0) * DirectX::XMMatrixTranslation(XPos + 60, 0, 100), g_Model);
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
