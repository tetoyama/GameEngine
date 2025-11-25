// Engine/Scene/System/renderSystem.cpp
#include "renderSystem.h"

#include <algorithm>
#include <filesystem>
#include <queue>

#include <DirectXMath.h>

#include "Backends/DirectX11/DirectXTex.h"
#include "Backends/ImGui/ImGui.h"
#include "Backends/ImGui/ImGuizmo.h"
#include "Backends/myVector3.h"

#include "Backends/Assimp/material.h"
#include "Backends/Assimp/scene.h"
#include "Backends/Assimp/cimport.h"
#include "Backends/Assimp/postprocess.h"
#include "Backends/Assimp/matrix4x4.h"

#include "Backends/myMath.h"

#include "Engine/DebugTools/debugSystem.h"

#include "Engine/Resources/Data/modelData.h"

#include "Registry/entityRegistry.h"
#include "Registry/systemRegistry.h"
#include "Registry/componentRegistry.h"

#include "Engine/Graphics/mainRenderer.h"

#include "Engine/Resources/resourceService.h"
#include "Engine/Resources/Data/vertexShaderData.h"
#include "Engine/Resources/Data/pixelShaderData.h"

#include "Scene.h"
#include "SceneManager.h"

#include "System/physicSystem.h"
#include "System/RenderSystem/cameraEntityData.h"
#include "System/RenderSystem/renderPhase.h"
#include "System/RenderSystem/renderTarget.h"
#include "System/RenderSystem/RenderPass/RenderPassContext.h"

#include <Component/bumpMapComponent.h>
#include <Component/2DspriteRendererComponent.h>
#include <Component/RenderLayerComponent.h>
#include <Component/particleComponent.h>
#include <Component/outlineComponent.h>
#include <Component/EffectComponent.h>
#include <Component/terrainComponent.h>
#include <Component/waveComponent.h>
#include "Component/transformComponent.h"
#include "Component/textureComponent.h"
#include "Component/meshRendererComponent.h"
#include "Component/modelRendererComponent.h"
#include "Component/BillBoardRendererComponent.h"
#include "Component/cameraComponent.h"

constexpr int maxLineCount = 99999;

RenderPassContext::RenderPassContext(const RenderPhase& renderPass, bool* renderLayer, std::shared_ptr<PixelShaderData> setPixelShader, std::shared_ptr<VertexShaderData> setVertexShader, const CameraEntityData& data, const Vector2& setScreenSize) {

	passPhase = renderPass;

	renderLayerVisibility = renderLayer;

	pixelShader = setPixelShader;
	vertexShader = setVertexShader;

	cameraData = data;

	screenSize = setScreenSize;

	CameraComponent* cameraComponent = cameraData.cameraComponent;
	TransformComponent* transformComponent = cameraData.transformComponent;

	if (cameraComponent == nullptr || transformComponent == nullptr) {
		return;
	}

	// プロジェクションマトリクス設定
	projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(cameraComponent->FOV, screenSize.x / screenSize.y, cameraComponent->NearClip, cameraComponent->FarClip);

	cameraPosition = DirectX::XMFLOAT4(transformComponent->position.x, transformComponent->position.y, transformComponent->position.z, 0.0f);

	viewMatrix = cameraComponent->viewMatrix;
}

Effekseer::Matrix44 ConvertXMMATRIXToMatrix44(const DirectX::XMMATRIX& matrix){
	Effekseer::Matrix44 result;
	DirectX::XMFLOAT4X4 tempMatrix;
	DirectX::XMStoreFloat4x4(&tempMatrix, matrix);

	//行列の各成分をEffekseerの行列にコピー
	for(int row = 0; row < 4; ++row){
		for(int col = 0; col < 4; ++col){
			result.Values[row][col] = tempMatrix.m[row][col];
		}
	}
	return result;
}

RenderLayer GetRenderLayerFromEntity(Entity entity, ComponentRegistry* registry) {
	auto* layerComponent = registry->GetComponent<RenderLayerComponent>(entity);
	if (layerComponent) {
		return layerComponent->layer;
	}
	if(registry->HasComponent<SpriteRendererComponent>(entity)){
		return RenderLayer::OverlayUI;
	}
	if(registry->HasComponent<BillBoardRendererComponent>(entity)){
		return RenderLayer::Transparent3D;
	}
	if(registry->HasComponent<ParticleComponent>(entity)){
		return RenderLayer::SortTransparent3D;
	}
	auto* texture = registry->GetComponent<TextureComponent>(entity);
	if (texture && texture->Material.Diffuse.w < 1.0f) {
		return RenderLayer::SortTransparent3D;
	}
	return RenderLayer::Opaque3D;
}

struct RenderOrderComparator {
	ComponentRegistry* registry;
	Vector3 cameraPosition;

	bool operator()(Entity a, Entity b) const{
		// Layer取得
		auto* layerA = registry->GetComponent<RenderLayerComponent>(a);
		auto* layerB = registry->GetComponent<RenderLayerComponent>(b);

		RenderLayer layerValA = GetRenderLayerFromEntity(a, registry);
		RenderLayer layerValB = GetRenderLayerFromEntity(b, registry);

		if(layerValA != layerValB){
			return static_cast<int>(layerValA) < static_cast<int>(layerValB);
		}
		if(layerValA == RenderLayer::Opaque3D || layerValA == RenderLayer::Transparent3D){
			return false;
		}
		if(layerValA == RenderLayer::OverlayUI || layerValA == RenderLayer::Background2D){

			auto* orderA = registry->GetComponent<OrderInLayerComponent>(a);
			auto* orderB = registry->GetComponent<OrderInLayerComponent>(b);

			int orderValA = orderA ? orderA->order : 0;
			int orderValB = orderB ? orderB->order : 0;

			return orderValA < orderValB;
		}
		if(layerValA == RenderLayer::SortTransparent3D){
			auto* transformA = registry->GetComponent<TransformComponent>(a);
			auto* transformB = registry->GetComponent<TransformComponent>(b);
			if(transformA && transformB){
				float distA = (cameraPosition - transformA->position).length();
				float distB = (cameraPosition - transformB->position).length();
				return distA > distB;
			}
		}
		return false;
	}
};


void RenderSystem::Initialize(){
	m_context->debug->LOG_DEBUG("RenderSystemを初期化中...");

	ID3D11Device* device = m_context->graphics->GetDevice();

	showPlayer = &m_context->imgui->GetManubar()->showPlayerView;
	showEditor = &m_context->imgui->GetManubar()->showEditorView;

	m_context->debug->LOG_DEBUG("UI用テクスチャの取得中...");
	PlayButtonTexture = m_context->resource->Load<TextureData>("Asset/Texture/UI/Control/Play.png");
	PauseButtonTexture = m_context->resource->Load<TextureData>("Asset/Texture/UI/Control/Pause.png");
	StopButtonTexture = m_context->resource->Load<TextureData>("Asset/Texture/UI/Control/Stop.png");
	StepButtonTexture = m_context->resource->Load<TextureData>("Asset/Texture/UI/Control/Step.png");

	m_Player = new RenderTarget(m_context->PlayerScreenSize, m_context->graphics);
	m_Editor = new RenderTarget(m_context->EditorScreenSize, m_context->graphics);
	m_Shadow = new RenderTarget(Vector2(SHADOWMAP_SIZE, SHADOWMAP_SIZE), m_context->graphics);

	m_billBoardMesh = new MeshRendererComponent;
	if(m_billBoardMesh){

		m_billBoardMesh->mesh.meshCount = 4;
		VERTEX_3D vertex[4]{};

		vertex[0].Position = DirectX::XMFLOAT3(-0.5f, 0.5f, 0.0f);
		vertex[0].Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
		vertex[0].Tangent = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
		vertex[0].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[0].TexCoord = DirectX::XMFLOAT2(0.0f, 0.0f);

		vertex[1].Position = DirectX::XMFLOAT3(0.5f, 0.5f, 0.0f);
		vertex[1].Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
		vertex[1].Tangent = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
		vertex[1].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[1].TexCoord = DirectX::XMFLOAT2(1.0f, 0.0f);

		vertex[2].Position = DirectX::XMFLOAT3(-0.5f, -0.5f, 0.0f);
		vertex[2].Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
		vertex[2].Tangent = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
		vertex[2].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[2].TexCoord = DirectX::XMFLOAT2(0.0f, 1.0f);

		vertex[3].Position = DirectX::XMFLOAT3(0.5f, -0.5f, 0.0f);
		vertex[3].Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
		vertex[3].Tangent = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
		vertex[3].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[3].TexCoord = DirectX::XMFLOAT2(1.0f, 1.0f);

		D3D11_BUFFER_DESC bd{};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(VERTEX_3D) * 4;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = 0;

		D3D11_SUBRESOURCE_DATA sd{};
		sd.pSysMem = vertex;

		m_context->renderer->GetGraphicsContext()->GetDevice()->CreateBuffer(&bd, &sd, m_billBoardMesh->mesh.m_VertexBuffer.GetAddressOf());
		m_context->renderer->GetGraphicsContext()->CreateVertexShader("Asset\\Shader\\commonVS.cso", m_billBoardMesh->mesh.m_VertexShader.GetAddressOf(), m_billBoardMesh->mesh.m_VertexLayout.GetAddressOf());
		m_context->renderer->GetGraphicsContext()->CreatePixelShader("Asset\\Shader\\unlitUVTexturePS.cso", m_billBoardMesh->mesh.m_PixelShader.GetAddressOf());
	}

	m_SpriteMesh = new MeshRendererComponent;
	if (m_SpriteMesh) {

		m_SpriteMesh->mesh.meshCount = 4;
		VERTEX_3D vertex[4]{};

		vertex[0].Position = DirectX::XMFLOAT3(0.5f, 0.5f, 0.0f);
		vertex[0].Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
		vertex[0].Tangent = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
		vertex[0].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[0].TexCoord = DirectX::XMFLOAT2(1.0f, 1.0f);

		vertex[1].Position = DirectX::XMFLOAT3(-0.5f, 0.5f, 0.0f);
		vertex[1].Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
		vertex[1].Tangent = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
		vertex[1].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[1].TexCoord = DirectX::XMFLOAT2(0.0f, 1.0f);

		vertex[2].Position = DirectX::XMFLOAT3(0.5f, -0.5f, 0.0f);
		vertex[2].Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
		vertex[2].Tangent = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
		vertex[2].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[2].TexCoord = DirectX::XMFLOAT2(1.0f, 0.0f);

		vertex[3].Position = DirectX::XMFLOAT3(-0.5f, -0.5f, 0.0f);
		vertex[3].Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
		vertex[3].Tangent = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
		vertex[3].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[3].TexCoord = DirectX::XMFLOAT2(0.0f, 0.0f);

		D3D11_BUFFER_DESC bd{};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(VERTEX_3D) * 4;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = 0;

		D3D11_SUBRESOURCE_DATA sd{};
		sd.pSysMem = vertex;

		m_context->renderer->GetGraphicsContext()->GetDevice()->CreateBuffer(&bd, &sd, m_SpriteMesh->mesh.m_VertexBuffer.GetAddressOf());
		m_context->renderer->GetGraphicsContext()->CreateVertexShader("Asset\\Shader\\commonVS.cso", m_SpriteMesh->mesh.m_VertexShader.GetAddressOf(), m_SpriteMesh->mesh.m_VertexLayout.GetAddressOf());
		m_context->renderer->GetGraphicsContext()->CreatePixelShader("Asset\\Shader\\unlitUVTexturePS.cso", m_SpriteMesh->mesh.m_PixelShader.GetAddressOf());
	}

	m_VertexShader = m_context->resource->Load<VertexShaderData>("Asset\\Shader\\OutlineVS.cso");
	m_PixelShader = m_context->resource->Load<PixelShaderData>("Asset\\Shader\\OutlinePS.cso");

	m_LineVertexShader = m_context->resource->Load<VertexShaderData>("Asset\\Shader\\DebugLineVS.cso");
	m_LinePixelShader = m_context->resource->Load<PixelShaderData>("Asset\\Shader\\DebugLinePS.cso");


	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(VERTEX_3D) * maxLineCount * 2;
	// 1ライン = 2頂点、最大ライン数を想定して確保しておく
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	HRESULT hr = device->CreateBuffer(&bd, nullptr, &pPhysicsDebugLineVB);
	if(FAILED(hr)){
		throw std::runtime_error("Failed to create physics debug line vertex buffer.");
	}

	auto m_FullScreenVS = m_context->resource->Load<VertexShaderData>("Asset\\Shader\\PostEffectVS.cso");
	auto m_FullScreenPS = m_context->resource->Load<PixelShaderData>("Asset\\Shader\\PostEffectPS.cso");

	copyShader.m_VS = m_FullScreenVS->m_VertexShader; // フルスクリーンVS
	copyShader.m_PS = m_FullScreenPS->m_PixelShader; // 単純に SRV → out を返す PS
	copyShader.m_InputLayout = m_FullScreenVS->m_VertexLayout;



}

void RenderSystem::Finalize(){
	pPhysicsDebugLineVB->Release();
	pPhysicsDebugLineVB = nullptr;
	delete m_billBoardMesh;
	delete m_SpriteMesh;

	delete m_Shadow;
	delete m_Editor;
	delete m_Player;
}

void RenderSystem::Update(float deltaTime) {

	// コンポーネントを持つエンティティの検索
	for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
		auto context = scene->GetSceneContext();
		const auto& modelEntities = context->component->FindEntitiesWithComponent<ModelRendererComponent>();
		if (modelEntities.empty()) {
			return;
		} else {
			for (Entity entity : modelEntities) {
				auto* modelRenderer = context->component->GetComponent<ModelRendererComponent>(entity);
				modelRenderer->animationTime += deltaTime * 60.0f;
			}
		}
	}
}

void RenderSystem::Draw(){


	//return;

	if(*showPlayer){
		PlayerView();
	} else{

		m_context->graphics->ResetViewport();
		m_context->graphics->GetDeviceContext()->OMSetRenderTargets(1, m_context->graphics->GetpRenderTargetView(), m_context->graphics->GetDepthStencilView());

		Vector2 ScreenSize = Vector2(
			(float)m_context->renderer->GetGraphicsContext()->m_width,
			(float)m_context->renderer->GetGraphicsContext()->m_height
		);

		m_context->PlayerScreenSize = ScreenSize;

		CameraEntityData cameraEntity = FindCameraEntity();
		if (!cameraEntity.cameraComponent) {
			return;
		}

		RenderPassContext renderPassContext(
			RenderPhase::PHASE_GBUFFER,
			playerRenderLayerVisible,
			nullptr,
			nullptr,
			cameraEntity,
			ScreenSize
		);


		DirectX::XMFLOAT4 clearColor = { 0,0,0,1 };
		m_context->graphics->ResetPingPongBuffer(&clearColor.x); // 初期描画用バッファ

		ID3D11ShaderResourceView* finalSRV = RenderSceneWithPostEffects(m_context->graphics->GetCurrentSRV(), renderPassContext);

		ID3D11RenderTargetView* rtv = *m_context->graphics->GetpRenderTargetView(); // バックバッファ
		m_context->graphics->GetDeviceContext()->OMSetRenderTargets(1, &rtv, m_context->graphics->GetDepthStencilView());
		m_context->graphics->DrawQuad(&copyShader, finalSRV);
	}
	if(*showEditor){
		EditorView();
	}

	m_context->graphics->ResetViewport();
	m_context->graphics->GetDeviceContext()->OMSetRenderTargets(1, m_context->graphics->GetpRenderTargetView(), m_context->graphics->GetDepthStencilView());

}

void RenderSystem::EditorUpdate(float deltaTime) {




	ImGuiIO& io = ImGui::GetIO();

	// マウス右クリックで操作有効
	static bool isCameraActive = false;
	if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
		isCameraActive = true;
	} else if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
		isCameraActive = false;
	}
	// 入力で動かすベクトル
	Vector3 velocity = {0,0,0};
	float speed = 20.0f;

	if(isCameraActive){
		// ------ 1. マウス移動で回転 ------
		float mouseSensitivity = 0.005f;
		m_EditorCameraRotation.x += io.MouseDelta.x * mouseSensitivity; // yaw
		m_EditorCameraRotation.y += io.MouseDelta.y * mouseSensitivity; // pitch

		// ピッチ制限
		const float pitchLimit = DirectX::XM_PIDIV2 - 0.01f;
		if(m_EditorCameraRotation.y > pitchLimit) m_EditorCameraRotation.y = pitchLimit;
		if(m_EditorCameraRotation.y < -pitchLimit) m_EditorCameraRotation.y = -pitchLimit;

		// 回転から方向ベクトル取得
		Vector3 front;
		front.x = cosf(m_EditorCameraRotation.y) * sinf(m_EditorCameraRotation.x);
		front.y = 0.0f;
		front.z = cosf(m_EditorCameraRotation.y) * cosf(m_EditorCameraRotation.x);
		front = front.normalize();
		Vector3 right = (Vec3Cross(front, Vector3(0.0f, 1.0f, 0.0f))).normalize();
		Vector3 up = (Vec3Cross(right, front)).normalize();

		if(ImGui::IsKeyDown(ImGuiKey_W)) velocity += front;
		if(ImGui::IsKeyDown(ImGuiKey_S)) velocity -= front;
		if(ImGui::IsKeyDown(ImGuiKey_A)) velocity += right;
		if(ImGui::IsKeyDown(ImGuiKey_D)) velocity -= right;
		if(ImGui::IsKeyDown(ImGuiKey_Q) || ImGui::IsKeyDown(ImGuiKey_LeftShift)) velocity -= up;
		if(ImGui::IsKeyDown(ImGuiKey_E) || ImGui::IsKeyDown(ImGuiKey_Space)) velocity += up;
		if(velocity.length() > 0.0f){
			m_EditorCameraPosition += velocity.normalize() * speed * deltaTime;
		}
	} else if(mouseOnEditor){

		Vector3 front;
		front.x = cosf(m_EditorCameraRotation.y) * sinf(m_EditorCameraRotation.x);
		front.y = sinf(m_EditorCameraRotation.y);
		front.z = cosf(m_EditorCameraRotation.y) * cosf(m_EditorCameraRotation.x);
		front = front.normalize();
		Vector3 right = (Vec3Cross(front, Vector3(0.0f, 1.0f, 0.0f))).normalize();
		Vector3 up = (Vec3Cross(right, front)).normalize();

		if(ImGui::IsMouseDown(ImGuiMouseButton_Middle)){
			float panSensitivity = 0.1f;
			m_EditorCameraPosition += right * io.MouseDelta.x * panSensitivity;
			m_EditorCameraPosition += up * io.MouseDelta.y * panSensitivity;
		}

		if(m_MouseWheel != 0.0f){
			m_EditorCameraPosition += front * m_MouseWheel * 2.0f;
		}
	}
}

void RenderSystem::DrawRenderLayerToggleUI() {

	ImGui::SameLine();

	std::string previewText;
	for (int i = 0; i < (int)RenderLayer::MaxRenderLayer; ++i) {
		if (editorRenderLayerVisible[i]) {
			if (!previewText.empty()) previewText += ", ";
			previewText += GetRenderLayerName(static_cast<RenderLayer>(i));
		}
	}
	if (previewText.empty()) previewText = "None";

	if (ImGui::BeginCombo("##Visible Layers", previewText.c_str())) {
		for (int i = 0; i < (int)RenderLayer::MaxRenderLayer; ++i) {
			ImGui::Selectable(
				GetRenderLayerName(static_cast<RenderLayer>(i)),
				&editorRenderLayerVisible[i],
				ImGuiSelectableFlags_DontClosePopups
			);
		}
		ImGui::EndCombo();
	}
}

TransformComponent RenderSystem::CalculateRectTransform(
	const RenderPassContext& renderPassContext,
	const SpriteRendererComponent& sprite,
	const TransformComponent& originalTransform
){
	TransformComponent adjustedTransform = originalTransform;

	Vector2 screenSize = renderPassContext.screenSize;
	Vector2 viewportSize = {
		(float)m_context->renderer->GetGraphicsContext()->m_width,
		(float)m_context->renderer->GetGraphicsContext()->m_height
	};

	// 仮想UI基準解像度
	const Vector2 referenceResolution = {1.0f, 1.0f};

	// アスペクト比
	float screenAspect = screenSize.x / screenSize.y;
	float referenceAspect = referenceResolution.x / referenceResolution.y;
	float aspectRatioScaleX = referenceAspect / screenAspect;

	// アンカー位置（画面サイズ基準）
	Vector2 anchoredPosition = {
		viewportSize.x * sprite.anchor.x,
		viewportSize.y * sprite.anchor.y
	};

	// スプライトサイズ（ピクセル単位）
	Vector2 adjustedScale = {
		originalTransform.scale.x * aspectRatioScaleX / referenceResolution.x * viewportSize.x,
		originalTransform.scale.y / referenceResolution.y * viewportSize.y
	};

	// ピボット補正（ピクセル単位）
	Vector2 pivotOffset = {
		adjustedScale.x * -sprite.pivot.x,
		adjustedScale.y * -sprite.pivot.y
	};

	// 仮想座標オフセット（position）→ ピクセルスケーリング ＋ アスペクト比補正
	Vector2 positionOffset = {
		originalTransform.position.x * aspectRatioScaleX / referenceResolution.x * viewportSize.x,
		originalTransform.position.y / referenceResolution.y * viewportSize.y
	};

	// 最終位置
	Vector2 finalPosition = {
		anchoredPosition.x - pivotOffset.x + positionOffset.x,
		anchoredPosition.y - pivotOffset.y + positionOffset.y
	};

	adjustedTransform.position = Vector3(finalPosition.x, finalPosition.y, originalTransform.position.z);
	adjustedTransform.scale = Vector3(adjustedScale.x, adjustedScale.y, originalTransform.scale.z);

	return adjustedTransform;
}

const CameraEntityData RenderSystem::FindCameraEntity() {

	CameraEntityData cameraData{};
	for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
		auto context = scene->GetSceneContext();
		// カメラを取得
		auto entities = context->component->FindEntitiesWithComponent<CameraComponent>();
		if (entities.empty()) {
			continue;
		}
		cameraData.sceneContext = context;
		cameraData.entity = entities[0];
		cameraData.cameraComponent = context->component->GetComponent<CameraComponent>(cameraData.entity);
		cameraData.transformComponent = context->component->GetComponent<TransformComponent>(cameraData.entity);
		return cameraData;
	}
	return cameraData;
}

void RenderSystem::DrawMesh(ComponentRegistry* componentRegistry, TransformComponent* transform, MeshRendererComponent* meshRenderer, TextureComponent* pTexture){


	GraphicsContext* graphicsContext = m_context->graphics;
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();


	if (!pTexture) {
		if (meshRenderer->mesh.m_TextureData) {
			deviceContext->PSSetShaderResources(0, 1, meshRenderer->mesh.m_TextureData->pTexture.GetAddressOf());

			MATERIAL material{};
			material.DiffuseTextureEnable = true;

			material.Diffuse = DirectX::XMFLOAT4(1, 1, 1, 1);
			graphicsContext->SetMaterial(material);
		}
	}
	if (meshRenderer->mesh.m_VertexLayout) {
		deviceContext->IASetInputLayout(meshRenderer->mesh.m_VertexLayout.Get());
	}
	if (meshRenderer->mesh.m_VertexShader) {
		deviceContext->VSSetShader(meshRenderer->mesh.m_VertexShader.Get(), NULL, 0);
	}
	if (meshRenderer->mesh.m_PixelShader) {
		deviceContext->PSSetShader(meshRenderer->mesh.m_PixelShader.Get(), NULL, 0);
	}
	DirectX::XMMATRIX World = transform->CalculateWorldMatrix(transform, componentRegistry);

	graphicsContext->SetWorldViewProjection2D();
	graphicsContext->SetWorldMatrix(World);
	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;

	deviceContext->IASetVertexBuffers(0, 1, meshRenderer->mesh.m_VertexBuffer.GetAddressOf(), &stride, &offset);

	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	deviceContext->Draw(meshRenderer->mesh.meshCount, 0);

	graphicsContext->SetDepthEnable(true);
	graphicsContext->SetViewMatrix(m_CameraView);
	graphicsContext->SetProjectionMatrix(m_CameraProjection);

}
void RenderSystem::DrawModel(ComponentRegistry* componentRegistry, TransformComponent* transform, ModelRendererComponent* modelRenderer, TextureComponent* pTexture, OutlineComponent* pOutline){
	ModelData* pModel = modelRenderer->model.get();
	if(!pModel || !pModel->AiScene){
		//m_context->debug->LOG_ERROR("ModelData is null or AiScene is not initialized.");
		return;
	}

	GraphicsContext* graphicsContext = m_context->graphics;
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();
	ID3D11Device* device = graphicsContext->GetDevice();

	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//pModel->Update(modelRenderer->currentAnimationName.c_str(), (int)modelRenderer->animationTime, m_context->graphics);
	pModel->Update(modelRenderer->animationTime, m_context->graphics);
	deviceContext->IASetInputLayout(m_VertexShader->m_VertexLayout.Get());


	// ワールド行列計算
	DirectX::XMMATRIX World = transform->CalculateWorldMatrix(transform, componentRegistry);
	for(int i = 0; i < 2; i++){
		if(i == 0){
			if(pOutline){
				deviceContext->PSSetShader(m_PixelShader->m_PixelShader.Get(), nullptr, 0);
				deviceContext->VSSetShader(m_VertexShader->m_VertexShader.Get(), nullptr, 0);
				graphicsContext->SetCullMode(CullMode::Front);
			} else{
				continue;
			}
		} else{
			if(modelRenderer->pixelShader){
				deviceContext->PSSetShader(modelRenderer->pixelShader->m_PixelShader.Get(), nullptr, 0);
			}
			if(modelRenderer->vertexShader){
				deviceContext->IASetInputLayout(modelRenderer->vertexShader->m_VertexLayout.Get());
				deviceContext->VSSetShader(modelRenderer->vertexShader->m_VertexShader.Get(), nullptr, 0);
			}
			graphicsContext->SetCullMode(CullMode::Back);
		}

		for(unsigned int m = 0; m < pModel->AiScene->mNumMeshes; m++){

			if(pModel->SetTexture){
				aiMaterial* material = pModel->AiScene->mMaterials[pModel->AiScene->mMeshes[m]->mMaterialIndex];
				if(!pTexture || !pTexture->m_TextureData){
					aiString texName;
					if(material->GetTexture(aiTextureType_DIFFUSE, 0, &texName) == AI_SUCCESS && texName.length > 0){
						auto it = pModel->m_Texture.find(texName.C_Str());
						if(it != pModel->m_Texture.end()){
							deviceContext->PSSetShaderResources(0, 1, &it->second);
						}
					}
					if(material->GetTexture(aiTextureType_NORMALS, 0, &texName) == AI_SUCCESS && texName.length > 0){
						auto it = pModel->m_Texture.find(texName.C_Str());
						if(it != pModel->m_Texture.end()){
							deviceContext->PSSetShaderResources(1, 1, &it->second);
						}
					}
				}
			}

			if(!pTexture){
				MATERIAL materialData;
				aiMaterial* aiMat = pModel->AiScene->mMaterials[pModel->AiScene->mMeshes[m]->mMaterialIndex];
				aiColor4D color;
				if(aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
					materialData.Diffuse = {color.r, color.g, color.b, color.a};
				if(aiMat->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS)
					materialData.Ambient = {color.r, color.g, color.b, color.a};
				if(aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, color) == AI_SUCCESS)
					materialData.Emission = {color.r, color.g, color.b, color.a};
				if(aiMat->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS)
					materialData.Specular = {color.r, color.g, color.b, color.a};

				float shininess = 0.0f;
				if(aiMat->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS)
					materialData.Shininess = std::clamp(shininess, 1.0f, 128.0f);

				materialData.DiffuseTextureEnable = pModel->SetTexture;
				graphicsContext->SetMaterial(materialData);
			}

			graphicsContext->SetWorldMatrix(World);

			// 頂点バッファ設定
			UINT stride = sizeof(VERTEX_3D);
			UINT offset = 0;
			graphicsContext->GetDeviceContext()->IASetVertexBuffers(0, 1, &pModel->VertexBuffer[m], &stride, &offset);

			// インデックスバッファ設定
			graphicsContext->GetDeviceContext()->IASetIndexBuffer(pModel->IndexBuffer[m], DXGI_FORMAT_R32_UINT, 0);

			
			
			deviceContext->DrawIndexed(pModel->AiScene->mMeshes[m]->mNumFaces * 3, 0, 0);
		}
	}
}


void RenderSystem::DrawBillBoard(ComponentRegistry* componentRegistry, TransformComponent* transform, MeshRendererComponent* meshRenderer, BillBoardRendererComponent* billBoard, TextureComponent* pTexture) {

	DirectX::XMMATRIX InvViewBillBoardMatrix = DirectX::XMMatrixRotationQuaternion(transform->rotationVector());

	if(!billBoard->RotateXYZ.x && !billBoard->RotateXYZ.y && !billBoard->RotateXYZ.z){
		// 全軸false：回転しない → 単位行列（通常メッシュと同じ）
	} else if(billBoard->RotateXYZ.y && !billBoard->RotateXYZ.x && !billBoard->RotateXYZ.z){
		// Y軸だけ回転（XZビルボード） → UIパネルなどで多用される

		DirectX::XMMATRIX invView = DirectX::XMMatrixInverse(nullptr, m_CameraView);
		DirectX::XMFLOAT4X4 invViewFloat4x4;
		DirectX::XMStoreFloat4x4(&invViewFloat4x4, invView);

		// カメラの前方向ベクトル（Z軸）
		DirectX::XMVECTOR forward = DirectX::XMVectorSet(invViewFloat4x4._31, 0.0f, invViewFloat4x4._33, 0.0f);
		forward = DirectX::XMVector3Normalize(forward);

		// up = Y軸固定
		DirectX::XMVECTOR up = DirectX::XMVectorSet(0, 1, 0, 0);
		DirectX::XMVECTOR right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(up, forward));
		up = DirectX::XMVector3Cross(forward, right);

		InvViewBillBoardMatrix = DirectX::XMMATRIX(
			right,
			up,
			forward,
			DirectX::XMVectorSet(0, 0, 0, 1)
		);
	} else{
		// 任意の軸制御（全軸、Y+Z など）
		DirectX::XMMATRIX invView = DirectX::XMMatrixInverse(nullptr, m_CameraView);
		DirectX::XMFLOAT4X4 invViewFloat4x4;
		DirectX::XMStoreFloat4x4(&invViewFloat4x4, invView);

		DirectX::XMVECTOR forward = DirectX::XMVectorSet(invViewFloat4x4._31, invViewFloat4x4._32, invViewFloat4x4._33, 0.0f);
		DirectX::XMVECTOR right = DirectX::XMVectorSet(invViewFloat4x4._11, invViewFloat4x4._12, invViewFloat4x4._13, 0.0f);
		DirectX::XMVECTOR up = DirectX::XMVectorSet(invViewFloat4x4._21, invViewFloat4x4._22, invViewFloat4x4._23, 0.0f);

		const DirectX::XMVECTOR worldRight = DirectX::XMVectorSet(1, 0, 0, 0);
		const DirectX::XMVECTOR worldUp = DirectX::XMVectorSet(0, 1, 0, 0);
		const DirectX::XMVECTOR worldForward = DirectX::XMVectorSet(0, 0, 1, 0);

		if(!billBoard->RotateXYZ.x) right = worldRight;
		if(!billBoard->RotateXYZ.y) up = worldUp;
		if(!billBoard->RotateXYZ.z) forward = worldForward;

		// 再直交化（forward優先）
		forward = DirectX::XMVector3Normalize(forward);
		right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(up, forward));
		up = DirectX::XMVector3Cross(forward, right);

		InvViewBillBoardMatrix = DirectX::XMMATRIX(
			right,
			up,
			forward,
			DirectX::XMVectorSet(0, 0, 0, 1)
		);
	}

	GraphicsContext* graphicsContext = m_context->graphics;
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();


	if (!pTexture) {
		if (meshRenderer->mesh.m_TextureData) {
			deviceContext->PSSetShaderResources(0, 1, meshRenderer->mesh.m_TextureData->pTexture.GetAddressOf());

			MATERIAL material{};
			material.Diffuse = DirectX::XMFLOAT4(1, 1, 1, 1);
			graphicsContext->SetMaterial(material);
		}
	}

	deviceContext->IASetInputLayout(meshRenderer->mesh.m_VertexLayout.Get());

	deviceContext->VSSetShader(meshRenderer->mesh.m_VertexShader.Get(), NULL, 0);
	deviceContext->PSSetShader(meshRenderer->mesh.m_PixelShader.Get(), NULL, 0);

	// ローカル変換行列（スケール・ビルボード回転・位置）
	DirectX::XMMATRIX LocalMatrix =
		DirectX::XMMatrixScaling(transform->scale.x, transform->scale.y, transform->scale.z) *
		InvViewBillBoardMatrix *
		DirectX::XMMatrixTranslation(transform->position.x, transform->position.y, transform->position.z);

	DirectX::XMMATRIX WorldMatrix = LocalMatrix;

	if(transform->parent != 0){
		auto parentTransform = componentRegistry->GetComponent<TransformComponent>(transform->parent);
		if(parentTransform){
			DirectX::XMMATRIX parentWorld = parentTransform->CalculateWorldMatrix(parentTransform, componentRegistry);

			// 親の位置だけを取得（分解）
			DirectX::XMVECTOR parentScale, parentRotation, parentTranslation;
			DirectX::XMMatrixDecompose(&parentScale, &parentRotation, &parentTranslation, parentWorld);

			// 親の位置だけの平行移動行列を作る
			DirectX::XMMATRIX parentTranslationMatrix = DirectX::XMMatrixTranslationFromVector(parentTranslation);

			// 親の回転・スケールは無視し、親位置だけ足す
			WorldMatrix = LocalMatrix * parentTranslationMatrix;
		}
	}

	graphicsContext->SetWorldMatrix(WorldMatrix);
	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;

	deviceContext->IASetVertexBuffers(0, 1, meshRenderer->mesh.m_VertexBuffer.GetAddressOf(), &stride, &offset);

	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	deviceContext->Draw(meshRenderer->mesh.meshCount, 0);
}

void RenderSystem::DrawParticle(ComponentRegistry* componentRegistry, TransformComponent* pTransform, ParticleComponent* pParticle, TextureComponent* pTexture){


	GraphicsContext* graphicsContext = m_context->renderer->GetGraphicsContext();
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();
	graphicsContext->SetBlendMode(BlendMode::Additive);

	for(int i = 0; i < MAXPARTICLE; i++){
		if(pParticle->Particle[i].LifeTime > 0.0f){
			MATERIAL material;

			if (pTexture) {
					// マテリアル設定
				material.Diffuse = pTexture->Material.Diffuse;
				material.DiffuseTextureEnable = ((bool)pTexture->m_TextureData);
				if (pTexture->m_TextureData) {
					deviceContext->PSSetShaderResources(0, 1, pTexture->m_TextureData->pTexture.GetAddressOf());
				}

				UVMatrix uv;
				if (pTexture->UV_Slice_X != 0 && pTexture->UV_Slice_Y != 0) {
					uv.Start.x = (float)(pTexture->AnimationNum % pTexture->UV_Slice_X) * 1.0f / (float)pTexture->UV_Slice_X;
					uv.Start.y = (float)(pTexture->AnimationNum / pTexture->UV_Slice_X) * 1.0f / (float)pTexture->UV_Slice_Y;

					uv.End.x = (float)uv.Start.x + 1.0f / (float)pTexture->UV_Slice_X;
					uv.End.y = (float)uv.Start.y + 1.0f / (float)pTexture->UV_Slice_Y;
				}
				graphicsContext->SetUVMatrix(uv);
				material.Diffuse.w = pTexture->Material.Diffuse.w * pParticle->Particle[i].LifeTime / pParticle->particleLifeTime;

			} else {
				// マテリアル設定
				material.DiffuseTextureEnable = false;
				material.Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

				UVMatrix uv;
				graphicsContext->SetUVMatrix(uv);
			}
			graphicsContext->SetMaterial(material);

			TransformComponent transform = *pTransform;
			transform.position += pParticle->Particle[i].Position;
			transform.scale *= pParticle->particleSize;

			BillBoardRendererComponent billBoard;

			DrawBillBoard(componentRegistry ,&transform, m_billBoardMesh, &billBoard, pTexture);
		}
	}
	graphicsContext->SetBlendMode(BlendMode::Alpha);
}

void RenderSystem::DrawTerrain(ComponentRegistry* componentRegistry, TransformComponent* pTransform, TerrainComponent* pTerrain, TextureComponent* pTexture) {
	if (!pTerrain || !pTerrain->meshRenderer) {
		return;
	}

	auto meshRenderer = pTerrain->meshRenderer;
	auto transform = pTransform;

	GraphicsContext* graphicsContext = m_context->graphics;
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();


	//if (!pTexture) {
	//	if (meshRenderer->mesh.m_TextureData) {
	//		deviceContext->PSSetShaderResources(0, 1, meshRenderer->mesh.m_TextureData->pTexture.GetAddressOf());

	//		MATERIAL material{};
	//		material.DiffuseTextureEnable = true;

	//		material.Diffuse = DirectX::XMFLOAT4(1, 1, 1, 1);
	//		graphicsContext->SetMaterial(material);
	//	}
	//}
	//if (meshRenderer->mesh.m_VertexLayout) {
	//	deviceContext->IASetInputLayout(meshRenderer->mesh.m_VertexLayout.Get());
	//}
	//if (meshRenderer->mesh.m_VertexShader) {
	//	deviceContext->VSSetShader(meshRenderer->mesh.m_VertexShader.Get(), NULL, 0);
	//}
	//if (meshRenderer->mesh.m_PixelShader) {
	//	deviceContext->PSSetShader(meshRenderer->mesh.m_PixelShader.Get(), NULL, 0);
	//}
	DirectX::XMMATRIX World = transform->CalculateWorldMatrix(transform, componentRegistry);

	graphicsContext->SetWorldMatrix(World);
	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	deviceContext->IASetVertexBuffers(0, 1, meshRenderer->mesh.m_VertexBuffer.GetAddressOf(), &stride, &offset);
	deviceContext->IASetIndexBuffer(*meshRenderer->mesh.m_IndexBuffer.GetAddressOf(), DXGI_FORMAT_R32_UINT,0);

	deviceContext->DrawIndexed(meshRenderer->mesh.indexCount, 0,0);

	graphicsContext->SetDepthEnable(true);
	graphicsContext->SetViewMatrix(m_CameraView);
	graphicsContext->SetProjectionMatrix(m_CameraProjection);
}

void RenderSystem::DrawWave(ComponentRegistry* componentRegistry, TransformComponent* pTransform, WaveComponent* pWave, TextureComponent* pTexture){
	if(!pWave || !pWave->meshRenderer){
		return;
	}

	auto meshRenderer = pWave->meshRenderer;
	auto transform = pTransform;

	GraphicsContext* graphicsContext = m_context->graphics;
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();


	//if (!pTexture) {
	//	if (meshRenderer->mesh.m_TextureData) {
	//		deviceContext->PSSetShaderResources(0, 1, meshRenderer->mesh.m_TextureData->pTexture.GetAddressOf());

	//		MATERIAL material{};
	//		material.DiffuseTextureEnable = true;

	//		material.Diffuse = DirectX::XMFLOAT4(1, 1, 1, 1);
	//		graphicsContext->SetMaterial(material);
	//	}
	//}
	//if (meshRenderer->mesh.m_VertexLayout) {
	//	deviceContext->IASetInputLayout(meshRenderer->mesh.m_VertexLayout.Get());
	//}
	//if (meshRenderer->mesh.m_VertexShader) {
	//	deviceContext->VSSetShader(meshRenderer->mesh.m_VertexShader.Get(), NULL, 0);
	//}
	//if (meshRenderer->mesh.m_PixelShader) {
	//	deviceContext->PSSetShader(meshRenderer->mesh.m_PixelShader.Get(), NULL, 0);
	//}
	DirectX::XMMATRIX World = transform->CalculateWorldMatrix(transform, componentRegistry);

	graphicsContext->SetWorldMatrix(World);
	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	deviceContext->IASetVertexBuffers(0, 1, meshRenderer->mesh.m_VertexBuffer.GetAddressOf(), &stride, &offset);
	deviceContext->IASetIndexBuffer(*meshRenderer->mesh.m_IndexBuffer.GetAddressOf(), DXGI_FORMAT_R32_UINT, 0);

	deviceContext->DrawIndexed(meshRenderer->mesh.indexCount, 0, 0);

	graphicsContext->SetDepthEnable(true);
	graphicsContext->SetViewMatrix(m_CameraView);
	graphicsContext->SetProjectionMatrix(m_CameraProjection);


}

void RenderSystem::ControllButton(){

	if(!PlayButtonTexture){
		return;
	}

	ImTextureRef Play;
	Play._TexID = (ImTextureID)PlayButtonTexture.get()->pTexture.Get();
	ImTextureRef Pause;
	Pause._TexID = (ImTextureID)PauseButtonTexture.get()->pTexture.Get();
	ImTextureRef Stop;
	Stop._TexID = (ImTextureID)StopButtonTexture.get()->pTexture.Get();
	ImTextureRef Step;
	Step._TexID = (ImTextureID)StepButtonTexture.get()->pTexture.Get();

	ImVec4 DefaultButtonColor = ImVec4(1.0f, 1.0f, 1.0f, 0.8f);
	ImVec4 StopButtonColor = ImVec4(1.0f, 1.0f, 1.0f, 0.8f);
	if(m_context->sceneManager->State == SceneManagerState::Stopped){
		StopButtonColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
	}
	ImGui::BeginDisabled(m_context->sceneManager->State == SceneManagerState::Stopped);

	if(ImGui::ImageButton("Stop", Stop, ImVec2(20, 20),ImVec2(0,0),ImVec2(1,1),ImVec4(0,0,0,0), StopButtonColor)){
		if(m_context->sceneManager->State != SceneManagerState::Stopped){

			m_context->sceneManager->State = SceneManagerState::Stopped; // シーンの状態をエディタに戻す
			ImGui::SetWindowFocus("Editor View");
		}
	}
	ImGui::EndDisabled();

	ImGui::SameLine();

	// ツールバー内容
	if(m_context->sceneManager->State == SceneManagerState::Playing){
		if(ImGui::ImageButton("Pause", Pause, ImVec2(20, 20), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), DefaultButtonColor)){
			m_context->sceneManager->State = SceneManagerState::Paused; // シーンの状態を 一時停止に変更
		}
	} else{
		if(ImGui::ImageButton("Play", Play, ImVec2(20, 20), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), DefaultButtonColor)){
			m_context->sceneManager->State = SceneManagerState::Playing; // シーンの状態を再生中に変更
			ImGui::SetWindowFocus("Play View");
		}
	}
	ImGui::SameLine();
	if(ImGui::ImageButton("Step", Step, ImVec2(20, 20), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), DefaultButtonColor)){
		m_context->sceneManager->State = SceneManagerState::Step; // シーンの状態を ステップに変更
	}
}

void RenderSystem::ShadowPass(RenderPassContext renderPassContext){

	GraphicsContext* graphicsContext = m_context->graphics;
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

	// シャドウマップ用レンダーターゲットとデプスステンシルビューを設定
	deviceContext->OMSetRenderTargets(1, m_Shadow->rtv.GetAddressOf(), m_Shadow->dsv.Get());

	// シャドウマップ用のカメラ設定
	LIGHT light = m_context->renderer->GetGraphicsContext()->GetLight()[0];


	TransformComponent lightTransform;
	lightTransform.position = Vector3(light.Position.x, light.Position.y, light.Position.z);

	CameraComponent lightCamera;
	lightCamera.NearClip = 0.1f;
	lightCamera.FarClip = 100.0f;
	lightCamera.FOV = DirectX::XM_PIDIV4;
	lightCamera.isLock = false;
	lightCamera.viewMatrix = light.LightView;

	CameraEntityData cameraData;
	cameraData.entity = 0;
	cameraData.sceneContext = nullptr;
	cameraData.cameraComponent = &lightCamera;
	cameraData.transformComponent = &lightTransform;

	renderPassContext.cameraData = cameraData;
	renderPassContext.passPhase = RenderPhase::PHASE_SHADOW;
	renderPassContext.screenSize = Vector2(SHADOWMAP_SIZE, SHADOWMAP_SIZE);

	// シーン内のエンティティを描画
	DrawEntities(renderPassContext);

	// 元のレンダーターゲットとデプスステンシルビューに戻す
	deviceContext->OMSetRenderTargets(1, graphicsContext->GetpRenderTargetView(), graphicsContext->GetDepthStencilView());
}

void RenderSystem::EditorView(){
	GraphicsContext* graphicsContext = m_context->graphics;
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();
	//ImGuiWindowFlags toolbar_window_flags = ImGuiWindowFlags_NoCollapse;
	ImGuiWindowFlags toolbar_window_flags = 0;
	ImGui::Begin("Editor View",showEditor, toolbar_window_flags);

	ControllButton();
	DrawRenderLayerToggleUI();

	ImGui::Separator();

	ImVec2 avail = ImGui::GetContentRegionAvail(); // ウィンドウ内の利用可能サイズ
	float availAspect = avail.x / avail.y;

	TransformComponent editorCameraTransform;
	editorCameraTransform.position = m_EditorCameraPosition;
	editorCameraTransform.SetRotationEuler(Vector3(m_EditorCameraRotation.y, m_EditorCameraRotation.x, m_EditorCameraRotation.z));

	CameraComponent editorCamera;
	editorCamera.FOV = DirectX::XM_PIDIV4;
	editorCamera.NearClip = 0.01f;
	editorCamera.FarClip = 1000.0f;
	editorCamera.isLock = false;
	editorCamera.viewMatrix = DirectX::XMMatrixLookAtLH(
		editorCameraTransform.position.ToXMVECTOR(),
		(editorCameraTransform.position + editorCameraTransform.front()).ToXMVECTOR(),
		{ 0.0f, 1.0f, 0.0f }
	);

	CameraEntityData cameraData;
	cameraData.entity = 0;
	cameraData.sceneContext = nullptr;
	cameraData.cameraComponent = &editorCamera;
	cameraData.transformComponent = &editorCameraTransform;

	m_context->EditorScreenSize = Vector2(avail.x, avail.y);
	m_Editor->Resize(Vector2(avail.x, avail.y),m_context->graphics);

	RenderPassContext renderPassContext(
		RenderPhase::PHASE_GBUFFER,
		editorRenderLayerVisible,
		nullptr,
		nullptr,
		cameraData,
		m_Editor->size
	);
	ShadowPass(renderPassContext);
	
	float clearCol[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
	deviceContext->ClearRenderTargetView(m_Editor->rtv.Get(), clearCol);
	deviceContext->ClearDepthStencilView(m_Editor->dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	graphicsContext->GetDeviceContext()->OMSetRenderTargets(1, m_Editor->rtv.GetAddressOf(), m_Editor->dsv.Get());
	DrawEntities(renderPassContext);

	ImVec2 cursor = ImGui::GetCursorPos();
	ImGui::Image((ImTextureID)m_Editor->srv.Get(), avail, ImVec2(0, 0), ImVec2(1, 1));

	// ImGuizmo の描画領域を設定
	ImGuizmo::SetRect(
		ImGui::GetWindowPos().x + cursor.x,
		ImGui::GetWindowPos().y + cursor.y,
		avail.x,
		avail.y
	);
	ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());

	// ImGuiIO を取得
	ImGuiIO& io = ImGui::GetIO();

	// マウスホイールの値をリセット
	m_MouseWheel = 0.0f;
	mouseOnEditor = false;

	// マウスカーソルの位置を取得
	POINT cursorPos;
	if(GetCursorPos(&cursorPos)){
		// 現在のウィンドウのハンドルを取得
		HWND hwnd = GetActiveWindow();
		if(hwnd){
			// スクリーン座標をウィンドウのクライアント座標に変換
			ScreenToClient(hwnd, &cursorPos);

			// 現在のウィンドウの描画リストを取得
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			ImVec2 mousePos = io.MousePos;

			// マウスカーソルがウィンドウ内にあるかを判定
			if(drawList->GetClipRectMin().x <= mousePos.x && mousePos.x <= drawList->GetClipRectMax().x &&
			   drawList->GetClipRectMin().y <= mousePos.y && mousePos.y <= drawList->GetClipRectMax().y){

				mouseOnEditor = true;

				// マウスホイールの入力を取得
				m_MouseWheel = io.MouseWheel;
			}
		}
	}

	ImGui::End();

	graphicsContext->GetDeviceContext()->OMSetRenderTargets(1, graphicsContext->GetpRenderTargetView(), graphicsContext->GetDepthStencilView());
}


std::vector<int> TopologicalSortPostEffects(CameraComponent* camera){
	std::vector<int> sortedIndices;
	if(!camera) return sortedIndices;

	int n = static_cast<int>(camera->postEffects.size());
	int INPUT_NODE = n;     // -1 の代わり
	int OUTPUT_NODE = n + 1;   // -2 の代わり

	std::unordered_map<int, std::vector<int>> adj;
	std::unordered_map<int, int> indegree;
	std::unordered_set<int> nodes;

	for(const auto& link : camera->postEffectLinks){
		int start = link.startNode;
		int end = link.endNode;

		if(start == -1) start = INPUT_NODE;
		if(end == -2) end = OUTPUT_NODE;

		if(start >= 0 && start <= OUTPUT_NODE && end >= 0 && end <= OUTPUT_NODE){
			adj[start].push_back(end);
			indegree[end]++;
			nodes.insert(start);
			nodes.insert(end);
		}
	}

	std::queue<int> q;
	for(int node : nodes){
		if(indegree.find(node) == indegree.end()) q.push(node);
	}

	while(!q.empty()){
		int node = q.front(); q.pop();
		sortedIndices.push_back(node);

		for(int neighbor : adj[node]){
			indegree[neighbor]--;
			if(indegree[neighbor] == 0) q.push(neighbor);
		}
	}

	// サイクル検出
	if(sortedIndices.size() != nodes.size()){
		OutputDebugStringA("Warning: post effect graph has cycles!\n");
		for(int node : nodes){
			if(std::find(sortedIndices.begin(), sortedIndices.end(), node) == sortedIndices.end()){
				sortedIndices.push_back(node);
			}
		}
	}

	// 仮IDを元に戻す
	for(auto& idx : sortedIndices){
		if(idx == INPUT_NODE) idx = -1;
		if(idx == OUTPUT_NODE) idx = -2;
	}

	return sortedIndices;
}



ID3D11ShaderResourceView* RenderSystem::RenderSceneWithPostEffects(ID3D11ShaderResourceView* initialSRV, const RenderPassContext& renderPassContext) {
	GraphicsContext* graphics = m_context->graphics;

	// 1. シーンを初期バッファに描画
	DirectX::XMFLOAT4 clearColor = { 0,0,0,1 };
	//graphics->ResetPingPongBuffer(&clearColor.x); // 初期描画用バッファ

	//RenderPassContext _renderPassContext = renderPassContext;
	//_renderPassContext.screenSize = Vector2((float)m_context->graphics->m_width, (float)m_context->graphics->m_height);

	DrawEntities(renderPassContext);

	//return initialSRV;

	// 2. トポロジカルソート済みのポストエフェクトノード作成
	std::vector<PostProcessNode> postNodes;
	std::unordered_map<int, int> effectIndexToPostNodeIndex; // camera->postEffects idx → postNodes idx

	CameraComponent* camera = renderPassContext.cameraData.cameraComponent;

	if (camera) {
		auto sortedIndices = TopologicalSortPostEffects(camera);

		for (int idx : sortedIndices) {
			if(idx < 0) continue; // -1/-2 は描画対象としてノード作らない

			auto& e = camera->postEffects[idx];
			if (!e.enabled || !e.ps || !e.vs) continue;

			PostProcessNode node{};
			node.id = idx;
			node.shader.m_VS = e.vs->m_VertexShader;
			node.shader.m_PS = e.ps->m_PixelShader;
			node.shader.m_InputLayout = e.vs->m_VertexLayout;
			node.param = e.Param;

			e.ResizeTexture(graphics->GetDevice(), renderPassContext.screenSize);
			e.Clear(graphics->GetDeviceContext(), &clearColor.x);
			node.rtv = e.rtv.GetAddressOf();
			node.srv = e.srv.Get();
			node.tex = e.tex.Get();

			// リンクから入力ノードを決定
			node.inputs.clear();
			// ノード追加前にインデックスを記録しておく
			int postNodeIndex = static_cast<int>(postNodes.size());
			effectIndexToPostNodeIndex[idx] = postNodeIndex;

			postNodes.push_back(std::move(node));
		}

		// リンクを後から追加（マッピングが揃った後に行う）
		for (auto& node : postNodes) {
			int effectIdx = node.id;
			for (auto& link : camera->postEffectLinks) {
				if (link.endNode == effectIdx) {
					if (link.startNode < 0) {
						node.inputs.push_back(-2); // 初期SRV
					} else {
						auto it = effectIndexToPostNodeIndex.find(link.startNode);
						if (it != effectIndexToPostNodeIndex.end()) {
							node.inputs.push_back(it->second);
						}
						// else: 無効な startNode は無視（スキップされたノードの可能性あり）
					}
				}
			}
		}
	}

	// 3. ApplyPostProcessChain 側で -1 を初期SRVに置き換えるようにする（念のための処理）
	if (!postNodes.empty()) {
		for (auto& node : postNodes) {
			for (size_t i = 0; i < node.inputs.size(); ++i) {
				if (node.inputs[i] == -1) {
					node.inputs[i] = -2; // 特殊値 -2 を初期SRV扱いとして ApplyPostProcessChain で処理
				}
			}
		}

		graphics->GetDeviceContext()->OMSetRenderTargets(0, nullptr, nullptr);
		graphics->ApplyPostProcessChain(postNodes, initialSRV);
		graphics->GetDeviceContext()->OMSetRenderTargets(0, nullptr, nullptr);

		return graphics->m_CurrentSRV;
	}

	return initialSRV;
}

void RenderSystem::PlayerView(){
	GraphicsContext* graphicsContext = m_context->graphics;
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

	ImGui::Begin("Play View", showPlayer, 0);

	// 共通UI（元のControllButtonやSeparatorなど）
	ControllButton();
	ImGui::Separator();

	// カメラコンポーネントを持つエンティティ取得
	const CameraEntityData& cameraData = FindCameraEntity();

	if (!cameraData.cameraComponent) {
		ImGui::Text("No Camera Component found.");
		ImGui::End();
		return;
	}

	// 利用可能な領域サイズを取得
	ImVec2 avail = ImGui::GetContentRegionAvail();

	m_Player->Resize(Vector2(avail.x, avail.y), m_context->graphics);

	//// 画面サイズ計算
	//m_context->PlayerScreenSize = Vector2((float)m_context->graphics->m_width, (float)m_context->graphics->m_height);

	//// アスペクト比
	//float targetAspect = m_context->PlayerScreenSize.x / m_context->PlayerScreenSize.y;
	//float availAspect = avail.x / avail.y;

	//// --- アスペクト比を維持した描画サイズを計算 ---
	ImVec2 drawSize = avail;
	//if (availAspect > targetAspect) {
	//	// 高さに合わせる
	//	drawSize.y = avail.y;
	//	drawSize.x = drawSize.y * targetAspect;
	//} else {
	//	// 幅に合わせる
	//	drawSize.x = avail.x;
	//	drawSize.y = drawSize.x / targetAspect;
	//}


	RenderPassContext renderPassContext(
		RenderPhase::PHASE_GBUFFER,
		playerRenderLayerVisible,
		nullptr,
		nullptr,
		cameraData,
		m_Player->size
	);
	ShadowPass(renderPassContext);

	float clearCol[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
	deviceContext->ClearRenderTargetView(m_Player->rtv.Get(), clearCol);
	deviceContext->ClearDepthStencilView(m_Player->dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	graphicsContext->GetDeviceContext()->OMSetRenderTargets(1, m_Player->rtv.GetAddressOf(), m_Player->dsv.Get());
	// --- ポストプロセス描画 ---
	ID3D11ShaderResourceView* finalSRV = RenderSceneWithPostEffects(m_Player->srv.Get(), renderPassContext);
	if(!finalSRV){
		ImGui::Text("finalSRV is NULL");
	}
	ImVec2 cursor = ImGui::GetCursorPos();
	ImGui::SetCursorPos(ImVec2(
		cursor.x + (avail.x - drawSize.x) * 0.5f,
		cursor.y + (avail.y - drawSize.y) * 0.5f
	));
	ImGui::Image((ImTextureID)finalSRV, drawSize, ImVec2(0, 0), ImVec2(1, 1), ImVec4(1.0f, 1.0f, 1.0f, 1.0f), ImVec4(1.0f, 1.0f, 1.0f, 0.0f));

	ImGui::End();

	// バックバッファへ戻す
	auto graphics = m_context->graphics;
	graphics->GetDeviceContext()->OMSetRenderTargets(
		1, graphics->GetpRenderTargetView(), graphics->GetDepthStencilView()
	);
}

void RenderSystem::ResizeRenderBuffer(const Vector2& screenSize, ID3D11Texture2D** tex, ID3D11RenderTargetView** rtv, ID3D11ShaderResourceView** srv, ID3D11DepthStencilView** dsv) {
	
	D3D11_TEXTURE2D_DESC td = {};
	td.Width = (int)screenSize.x; td.Height = (int)screenSize.y;
	td.MipLevels = 1; td.ArraySize = 1;
	td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	td.SampleDesc.Count = 1;
	td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	GraphicsContext* graphicsContext = m_context->graphics;
	ID3D11Device* device = graphicsContext->GetDevice();

	device->CreateTexture2D(&td, nullptr, tex);
	if (*tex) {
		device->CreateRenderTargetView(*tex, nullptr, rtv);
		device->CreateShaderResourceView(*tex, nullptr, srv);
	}

	// デプスステンシルバッファ作成
	ID3D11Texture2D* depthStencile{};
	D3D11_TEXTURE2D_DESC textureDesc{};
	textureDesc.Width = (int)screenSize.x;
	textureDesc.Height = (int)screenSize.y;
	textureDesc.MipLevels = 1;
	textureDesc.ArraySize = 1;
	textureDesc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;
	textureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	textureDesc.CPUAccessFlags = 0;
	textureDesc.MiscFlags = 0;
	HRESULT hr = device->CreateTexture2D(&textureDesc, NULL, &depthStencile);
	if (FAILED(hr)) {
		return;
	}

	// デプスステンシルビュー作成
	D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc{};
	depthStencilViewDesc.Format = textureDesc.Format;
	depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depthStencilViewDesc.Flags = 0;
	if (depthStencile) {
		hr = device->CreateDepthStencilView(depthStencile, &depthStencilViewDesc, dsv);
		depthStencile->Release();

		if (FAILED(hr)) {
			return;
		}
	}
}

void RenderSystem::DrawEntities(const RenderPassContext& renderPassContext){

	bool* pRenderLayer = renderPassContext.renderLayerVisibility;

	// コンテキストの取得
	GraphicsContext* graphicsContext = m_context->renderer->GetGraphicsContext();
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

	// コンスタントバッファ設定
	CAMERA camera{};
	camera.CameraPosition = renderPassContext.cameraPosition;
	graphicsContext->SetCamera(camera);

	m_CameraView = renderPassContext.cameraData.cameraComponent->viewMatrix;
	m_CameraProjection = renderPassContext.projectionMatrix;

	graphicsContext->SetViewMatrix(renderPassContext.viewMatrix);
	graphicsContext->SetProjectionMatrix(renderPassContext.projectionMatrix);

	D3D11_VIEWPORT vp = {};
	vp.Width = renderPassContext.screenSize.x;
	vp.Height = renderPassContext.screenSize.y;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	deviceContext->RSSetViewports(1, &vp);

	m_context->imgui->SetViewProjectionMatrix(renderPassContext.viewMatrix, renderPassContext.projectionMatrix);

	for (int i = 0; i < (int)RenderLayer::MaxRenderLayer; i++) {

		for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
			auto context = scene->GetSceneContext();

			// コンポーネントを持つエンティティの検索
			std::vector<Entity> entities = context->component->FindEntitiesWithComponent<TransformComponent>();

			if (entities.empty()) {

				continue;

			} else {

				for (Entity entity : entities) {

					RenderLayer layer = GetRenderLayerFromEntity(entity, context->component);

					if ((int)layer != i) {
						continue;
					}

					TransformComponent* transform = context->component->GetComponent<TransformComponent>(entity);
					if (transform) {
						OutlineComponent* outline = context->component->GetComponent<OutlineComponent>(entity);
						BumpMapComponent* bumpMap = context->component->GetComponent<BumpMapComponent>(entity);
						if (bumpMap) {
							// バンプマップの設定
							if (bumpMap->m_TextureData) {
								deviceContext->PSSetShaderResources(1, 1, bumpMap->m_TextureData->pTexture.GetAddressOf());
							} else {
								ID3D11ShaderResourceView* nullSRV = nullptr;
								deviceContext->PSSetShaderResources(1, 1, &nullSRV);
							}
						} else {
							ID3D11ShaderResourceView* nullSRV = nullptr;
							deviceContext->PSSetShaderResources(1, 1, &nullSRV);
						}

						TextureComponent* texture = context->component->GetComponent<TextureComponent>(entity);
						if (texture) {
							// マテリアル設定
							MATERIAL material = texture->Material;
							material.DiffuseTextureEnable = ((bool)texture->m_TextureData);
							if (texture->m_TextureData) {
								deviceContext->PSSetShaderResources(0, 1, texture->m_TextureData->pTexture.GetAddressOf());
							}

							graphicsContext->SetMaterial(material);

							UVMatrix uv;
							if (texture->UV_Slice_X != 0 && texture->UV_Slice_Y != 0) {
								uv.Start.x = (float)(texture->AnimationNum % texture->UV_Slice_X) * 1.0f / (float)texture->UV_Slice_X;
								uv.Start.y = (float)(texture->AnimationNum / texture->UV_Slice_X) * 1.0f / (float)texture->UV_Slice_Y;

								uv.End.x = (float)uv.Start.x + 1.0f / (float)texture->UV_Slice_X;
								uv.End.y = (float)uv.Start.y + 1.0f / (float)texture->UV_Slice_Y;
							}
							graphicsContext->SetUVMatrix(uv);

						} else {
							// マテリアル設定
							MATERIAL material;
							material.DiffuseTextureEnable = false;
							material.Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
							graphicsContext->SetMaterial(material);

							UVMatrix uv;
							graphicsContext->SetUVMatrix(uv);
						}
						SpriteRendererComponent* spriteRenderer = context->component->GetComponent<SpriteRendererComponent>(entity);
						if (spriteRenderer) {
							TransformComponent newTransform = CalculateRectTransform(renderPassContext, *spriteRenderer, *transform);
							DrawMesh(context->component, &newTransform, m_SpriteMesh, texture);
						}
						BillBoardRendererComponent* billBoardRenderer = context->component->GetComponent<BillBoardRendererComponent>(entity);
						if (billBoardRenderer) {
							DrawBillBoard(context->component, transform, m_billBoardMesh, billBoardRenderer, texture);
						}
						TerrainComponent* terrain = context->component->GetComponent<TerrainComponent>(entity);
						if (terrain) {
							DrawTerrain(context->component, transform, terrain, texture);
						}
						WaveComponent* wave = context->component->GetComponent<WaveComponent>(entity);
						if (wave) {
							DrawWave(context->component, transform, wave, texture);
						}

						MeshRendererComponent* meshRenderer = context->component->GetComponent<MeshRendererComponent>(entity);
						if (meshRenderer) {
							DrawMesh(context->component, transform, meshRenderer, texture);
						}

						ModelRendererComponent* modelRenderer = context->component->GetComponent<ModelRendererComponent>(entity);
						if (modelRenderer) {
							DrawModel(context->component, transform, modelRenderer, texture, outline);
						}

						ParticleComponent* particle = context->component->GetComponent<ParticleComponent>(entity);
						if (particle) {
							DrawParticle(context->component, transform, particle, texture);
						}
					}
				}
			}
		}
	}
	//Effekseer
	Effekseer::Matrix44 effekseerProjectionMatrix = ConvertXMMATRIXToMatrix44(renderPassContext.projectionMatrix);
	Effekseer::Matrix44 effekseerViewMatrix = ConvertXMMATRIXToMatrix44(renderPassContext.viewMatrix);

	m_context->graphics->GetEffectRenderer()->SetProjectionMatrix(effekseerProjectionMatrix);
	m_context->graphics->GetEffectRenderer()->SetCameraMatrix(effekseerViewMatrix);

	m_context->graphics->GetEffectRenderer()->BeginRendering();
	m_context->graphics->GetEffectManager()->Draw();
	m_context->graphics->GetEffectRenderer()->EndRendering();

	//PhysX
	if(pRenderLayer[(int)RenderLayer::Debug]){
		for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
			auto context = scene->GetSceneContext();
			auto physics = m_context->systemRegistry->GetSystem<PhysicSystem>();
			const physx::PxRenderBuffer& rb = physics->GetRenderBuffer();

			// 色変換関数
			auto ConvertColor = [](physx::PxU32 c) {
				float a = ((c >> 24) & 0xFF) / 255.0f;
				float r = ((c >> 16) & 0xFF) / 255.0f;
				float g = ((c >> 8) & 0xFF) / 255.0f;
				float b = ((c >> 0) & 0xFF) / 255.0f;
				return DirectX::XMFLOAT4(r, g, b, a);
			};

			std::vector<VERTEX_3D> vertices;
			for (physx::PxU32 i = 0; i < rb.getNbLines(); i++) {
				const physx::PxDebugLine& line = rb.getLines()[i];

				VERTEX_3D v0;
				v0.Position = DirectX::XMFLOAT3(line.pos0.x, line.pos0.y, line.pos0.z);
				v0.Diffuse = ConvertColor(line.color0);

				VERTEX_3D v1;
				v1.Position = DirectX::XMFLOAT3(line.pos1.x, line.pos1.y, line.pos1.z);
				v1.Diffuse = ConvertColor(line.color1);

				vertices.push_back(v0);
				vertices.push_back(v1);
			}

			if (vertices.empty() || vertices.size() >= maxLineCount) continue;

			ID3D11Device* device = graphicsContext->GetDevice();
			ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

			// 頂点バッファ更新
			D3D11_MAPPED_SUBRESOURCE mapped;
			deviceContext->Map(pPhysicsDebugLineVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
			memcpy(mapped.pData, vertices.data(), sizeof(VERTEX_3D) * vertices.size());
			deviceContext->Unmap(pPhysicsDebugLineVB, 0);

			graphicsContext->SetWorldMatrix(DirectX::XMMatrixIdentity());

			UINT stride = sizeof(VERTEX_3D);
			UINT offset = 0;
			deviceContext->IASetVertexBuffers(0, 1, &pPhysicsDebugLineVB, &stride, &offset);
			deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

			// シェーダーをセット（通常のカラー付き頂点用のものを使用）
			deviceContext->VSSetShader(m_LineVertexShader->m_VertexShader.Get(), nullptr, 0);
			deviceContext->PSSetShader(m_LinePixelShader->m_PixelShader.Get(), nullptr, 0);

			// 描画
			deviceContext->Draw(static_cast<UINT>(vertices.size()), 0);
		}
	}
}

