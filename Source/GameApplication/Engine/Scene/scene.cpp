// Engine/Scene/scene.cpp

#include "scene.h"
#include <commdlg.h> // GetOpenFileName
#include <filesystem>
#include "Backends/Taskbar/taskbar.h"

#include <string>
#include "Engine/DebugTools/debugSystem.h"

#include "Engine/Graphics/mainRenderer.h"

#include "Engine/Platform/InputSystem/InputSystem.h"

#include "Engine/Resources/resourceService.h"
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

#include "System/C#ScriptSystem.h"
#include "System/CustomScriptSystem.h"
#include "System/lightSystem.h"
#include "System/particleSystem.h"
#include "System/audioSystem.h"
#include "System/physicSystem.h"
#include "System/effectSystem.h"

#include "Component/entityNameComponent.h"
#include "Component/transformComponent.h"
#include "Component/cameraComponent.h"
#include "Component/modelRendererComponent.h"
#include "Component/meshRendererComponent.h"
#include "Component/BillBoardRendererComponent.h"
#include "Component/textureComponent.h"
#include "Component/CustomScriptComponent.h"
#include "Component/C#ScriptComponent.h"
#include "Component/bumpMapComponent.h"
#include "Component/2DspriteRendererComponent.h"
#include "Component/RenderLayerComponent.h"
#include "Component/LightComponent.h"
#include "Component/particleComponent.h"
#include "Component/audioComponent.h"
#include "Component/outlineComponent.h"

#include "Script/SetScene.h"
#include "Script/ScoreManager.h"
#include "Script/ScoreSprite.h"
#include "Script/PlayerController.h"

#include <Component/EffectComponent.h>
#include <Component/ColliderComponent.h>

Scene::Scene(){

}

Scene::~Scene(){
}

void Scene::Initialize(ManagerContext* set){
	
	m_SceneManagerContext = set;
	m_SceneManagerContext->debug->LOG_INFO("Sceneを初期化中...");

	m_OldState = m_SceneContext.state;

	m_entityRegistry = std::make_shared<EntityRegistry>();
	m_componentRegistry = std::make_shared<ComponentRegistry>(m_entityRegistry.get(),&m_SceneContext);
	m_systemRegistry = std::make_shared<SystemRegistry>();

	// コンポーネントを登録（Archetype or Sparse を選択）
	// ボトルネックが見つかったコンポーネントから ArchetypeStorage<T> に移行

	// 名前
	m_componentRegistry->RegisterYAMLComponent<NameComponent>("NameComponent", false);

	// トランスフォーム
	m_componentRegistry->RegisterYAMLComponent<TransformComponent>("TransformComponent", false);
	m_componentRegistry->RegisterYAMLComponent<ColliderComponent>("ColliderComponent", false);

	// オーディオ
	m_componentRegistry->RegisterYAMLComponent<AudioComponent>("AudioComponent", false);


	// 描画レイヤー
	m_componentRegistry->RegisterYAMLComponent<RenderLayerComponent>("RenderLayerComponent", false);
	m_componentRegistry->RegisterYAMLComponent<OrderInLayerComponent>("OrderInLayerComponent", false);

	// テクスチャ
	m_componentRegistry->RegisterYAMLComponent<TextureComponent>("TextureComponent", false);
	m_componentRegistry->RegisterYAMLComponent<BumpMapComponent>("BumpMapComponent", false);

	// ライティング
	m_componentRegistry->RegisterYAMLComponent<LightComponent>("LightComponent", false);

	// レンダリング
	m_componentRegistry->RegisterYAMLComponent<MeshRendererComponent>("MeshRendererComponent", false);
	m_componentRegistry->RegisterYAMLComponent<ModelRendererComponent>("ModelRendererComponent", false);
	m_componentRegistry->RegisterYAMLComponent<BillBoardRendererComponent>("BillBoardRendererComponent", false);
	m_componentRegistry->RegisterYAMLComponent<SpriteRendererComponent>("SpriteRendererComponent", false);
	m_componentRegistry->RegisterYAMLComponent<OutlineComponent>("OutlineComponent", false);
	m_componentRegistry->RegisterYAMLComponent<ParticleComponent>("ParticleComponent", false);

	// カメラ
	m_componentRegistry->RegisterYAMLComponent<CameraComponent>("CameraComponent", false);

	// カスタムスクリプト
	m_componentRegistry->RegisterYAMLComponent<CustomScriptComponent>("CustomScriptComponent", false);
	m_componentRegistry->RegisterYAMLComponent<CSharpScriptComponent>("CSharpScriptComponent", false);

	// ユーザー定義のスクリプトコンポーネントを登録
	m_componentRegistry->RegisterYAMLComponent<SetScene>("SetScene", false);
	m_componentRegistry->RegisterYAMLComponent<ScoreManager>("ScoreManager", false);
	m_componentRegistry->RegisterYAMLComponent<ScoreSprite>("ScoreSprite", false);
	m_componentRegistry->RegisterYAMLComponent<PlayerController>("PlayerController", false);

	m_componentRegistry->RegisterYAMLComponent<EffectComponent>("EffectComponent", false);


	// システムを登録
	m_systemRegistry->RegisterSystem(std::make_unique<TransformSystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<CameraSystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<LightSystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<RenderSystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<AudioSystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<InspectorSystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<ParticleSystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<EffectSystem>(&m_SceneContext));

	m_systemRegistry->RegisterSystem(std::make_unique<CSharpScriptSystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<CustomScriptSystem>(&m_SceneContext));

	m_systemRegistry->RegisterSystem(std::make_unique<PhysicSystem>(&m_SceneContext));

	// シーンコンテキストの初期化
	auto Renderer = m_SceneManagerContext->renderer;
	
	m_SceneContext.entity = m_entityRegistry.get();
	m_SceneContext.component = m_componentRegistry.get();
	m_SceneContext.system = m_systemRegistry.get();
	m_SceneContext.manager = m_SceneManagerContext;

	m_systemRegistry->InitializeAll();

	auto graphicsContext = Renderer->GetGraphicsContext();
	// ライティングの仮設定
	LIGHT light;
	graphicsContext->SetLight(light);
	graphicsContext->SetDepthEnable(true);

	// デフォルトのシーンを構築
	if(ScenePath.empty()){
		BuildDefaultScene();
	} else{
		LoadSceneFromYAML(ScenePath);
	}
	m_SceneManagerContext->debug->LOG_INFO("Sceneを開始します");

	// システムの初期化
	m_systemRegistry->StartAll();
}

void Scene::Update(float deltaTime){

	// シーンの状態が変わった場合の処理
	if(m_OldState != m_SceneContext.state){

		if(m_SceneContext.state == SceneState::Playing){

			if(m_OldState == SceneState::Stopped){
				TempSave(); // 一時保存
				m_SceneManagerContext->debug->LOG_INFO("シーンを開始します");
				m_systemRegistry->FinalizeAll();
				m_systemRegistry->InitializeAll();
			} else{
				m_SceneManagerContext->debug->LOG_INFO("シーンを再開します");
			}

			m_systemRegistry->StartAll();

		} else if(m_SceneContext.state == SceneState::Paused){
			m_SceneManagerContext->debug->LOG_INFO("シーンを一時停止します");

		} else if(m_SceneContext.state == SceneState::Stopped){
			m_SceneManagerContext->debug->LOG_INFO("シーンを停止します");
			TempLoad(); // 一時保存の読み込み
			m_systemRegistry->FinalizeAll();
			m_systemRegistry->InitializeAll();
		}
		m_OldState = m_SceneContext.state;
	}

	if(m_SceneContext.state == SceneState::Playing){
		m_systemRegistry->UpdateAll(deltaTime);
	}

	m_systemRegistry->EditorUpdateAll(deltaTime);
}

void Scene::FixedUpdate(float fixedDeltaTime){
	if(m_SceneContext.state == SceneState::Playing){
		m_systemRegistry->FixedUpdateAll(fixedDeltaTime);
	}
}

void Scene::Draw(){

	m_systemRegistry->DrawAll();
}

void Scene::Shutdown(){
	m_SceneManagerContext->debug->LOG_INFO("Sceneを終了中...");
	ResetAll();
	m_systemRegistry->FinalizeAll();

	// システムの終了処理
	m_entityRegistry.reset();
	m_componentRegistry.reset();
	m_systemRegistry.reset();
}

void Scene::BuildDefaultScene(){
	auto entityRegistry = m_SceneContext.entity;
	auto componentRegistry = m_SceneContext.component;
	auto systemRegistry = m_SceneContext.system;

	auto Renderer = m_SceneManagerContext->renderer;
	auto graphicsContext = Renderer->GetGraphicsContext();

	auto resource = m_SceneManagerContext->resource;

	LIGHT light{};
	light.Enable = TRUE;
	light.Direction = DirectX::XMFLOAT4(0.0f, -1.0f, 0.0f, 0.0f);
	light.Position = DirectX::XMFLOAT4(0, 25, 25, 0);
	light.Diffuse = DirectX::XMFLOAT4(0.9f, 0.9f, 1.0f, 1);
	light.Ambient = DirectX::XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);
	light.PointLightParam = DirectX::XMFLOAT4(100.0f, 0, 0, 0);
	light.Angle = DirectX::XMFLOAT4(DirectX::XM_PI / 180.0f * 120.0f, 0.0f, 0.0f, 0.0f);

	light.SkyColor = DirectX::XMFLOAT4(0.8f, 0.8f, 1.0f, 0.1f);
	light.GroundColor = DirectX::XMFLOAT4(1.0f, 0.8f, 0.5f, 0.05f);
	light.GroundNormal = DirectX::XMFLOAT4(0, 0, 1, 0);
	graphicsContext->SetLight(light);
	graphicsContext->SetDepthEnable(true);

	//return;

	{
		//エンティティを作成し、TransformとModelRendererを追加
		Entity entity = entityRegistry->Create();

		auto* name = componentRegistry->AddComponent<NameComponent>(entity);
		name->name = "Field";

		// TransformComponentを追加
		auto* transform = componentRegistry->AddComponent<TransformComponent>(entity);
		transform->scale = Vector3(100.0f, 10.0f, 100.0f);

		transform->position = Vector3(0.0f, -transform->scale.y, 0);
		transform->SetRotationEuler(Vector3(0.0f, 0.0f, 0.0f));


		// ModelRendererComponentを追加
		auto* modelRenderer = componentRegistry->AddComponent<ModelRendererComponent>(entity);
		modelRenderer->model = resource->Load<ModelData>("Asset\\Model\\cube.fbx",false);
		modelRenderer->vertexShader = resource->Load<VertexShaderData>("Asset\\Shader\\commonVS.cso");
		modelRenderer->pixelShader = resource->Load<PixelShaderData>("Asset\\Shader\\PixelShader.cso");

		auto* texture = componentRegistry->AddComponent<TextureComponent>(entity);
		texture->m_TextureData = m_SceneManagerContext->resource->Load<TextureData>("Asset\\Texture\\white.tga");

		//auto* bumpMap = componentRegistry->AddComponent<BumpMapComponent>(entity);
		//bumpMap->m_TextureData = m_SceneManagerContext->resource->Load<TextureData>("Asset\\Texture\\BumpMap/Normal.bmp");

	}
	{
		//エンティティを作成し、Transformを追加
		Entity entity = entityRegistry->Create();

		auto* name = componentRegistry->AddComponent<NameComponent>(entity);
		name->name = "Light";

		// TransformComponentを追加
		auto* transform = componentRegistry->AddComponent<TransformComponent>(entity);
		transform->position = Vector3(0.0f, 25.0f,0.0f);
		transform->scale = Vector3(1.0f, 1.0f, 1.0f);
		transform->SetRotationEuler(Vector3(0.0f, 0.0f, 0.0f));

		auto* light = componentRegistry->AddComponent<LightComponent>(entity);
	}
	{
		//エンティティを作成し、TransformとModelRendererを追加
		Entity entity = entityRegistry->Create();

		auto* name = componentRegistry->AddComponent<NameComponent>(entity);
		name->name = "SkyBox";

		// TransformComponentを追加
		auto* transform = componentRegistry->AddComponent<TransformComponent>(entity);
		transform->position = Vector3(0.0f, 0.0f, 0.0f);
		transform->scale = Vector3(500.0f, 500.0f, 500.0f);
		transform->SetRotationEuler(Vector3(0.0f, 0.0f, 0.0f));

		auto* texture = componentRegistry->AddComponent<TextureComponent>(entity);
		texture->m_TextureData = m_SceneManagerContext->resource->Load<TextureData>("Asset\\Texture\\Daylight.png");
		texture->Material.Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

		// ModelRendererComponentを追加
		auto* modelRenderer = componentRegistry->AddComponent<ModelRendererComponent>(entity);
		modelRenderer->isBlender = true;
		modelRenderer->model = resource->Load<ModelData>("Asset\\Model\\sky.fbx",true);
		modelRenderer->vertexShader = resource->Load<VertexShaderData>("Asset\\Shader\\commonVS.cso");
		modelRenderer->pixelShader = resource->Load<PixelShaderData>("Asset\\Shader\\unlitUVTexturePS.cso");
	}
	{
		//エンティティを作成し、TransformとModelRendererを追加
		Entity entity = entityRegistry->Create();

		auto* name = componentRegistry->AddComponent<NameComponent>(entity);
		name->name = "Player";

		auto* texture = componentRegistry->AddComponent<TextureComponent>(entity);
		texture->m_TextureData = m_SceneManagerContext->resource->Load<TextureData>("Asset\\Texture\\white.tga");
		texture->Material.Diffuse = DirectX::XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);

		// TransformComponentを追加
		auto* transform = componentRegistry->AddComponent<TransformComponent>(entity);
		transform->position = Vector3(0.0f, 0.2f, 0);
		transform->scale = Vector3(1.0f, 1.0f, 1.0f);
		transform->SetRotationEuler(Vector3(0.0f, 0.0f, 0.0f));


		// ModelRendererComponentを追加
		auto* modelRenderer = componentRegistry->AddComponent<ModelRendererComponent>(entity);
		modelRenderer->model = resource->Load<ModelData>("Asset\\Model\\player.obj", false);
		modelRenderer->vertexShader = resource->Load<VertexShaderData>("Asset\\Shader\\commonVS.cso");
		modelRenderer->pixelShader = resource->Load<PixelShaderData>("Asset\\Shader\\ToonShaderPS.cso");

		// OutLineComponentを追加
		//auto* outline = componentRegistry->AddComponent<OutlineComponent>(entity);

		//auto* script = componentRegistry->AddComponent<ScriptComponent>(entity);
		//script->SetScriptName("PlayerScript"); // スクリプトクラス名
	}

	{
		//エンティティを作成し、TransformとCameraを追加
		Entity entity = entityRegistry->Create();

		auto* name = componentRegistry->AddComponent<NameComponent>(entity);
		name->name = "Camera";

		// TransformComponentを追加
		auto* transform = componentRegistry->AddComponent<TransformComponent>(entity);
		transform->position = Vector3(0.0f, 20.0f, -15.0f);
		transform->scale = Vector3(1.0f, 1.0f, 1.0f);
		transform->SetRotationEuler(Vector3(0.0f, 0.0f, 0.0f));


		// CameraComponentを追加
		auto* camera = componentRegistry->AddComponent<CameraComponent>(entity);
	}

	//{
	//	Entity entity = entityRegistry->Create();

	//	auto* name = componentRegistry->AddComponent<NameComponent>(entity);
	//	name->name = "2DSprite";

	//	auto* transform = componentRegistry->AddComponent<TransformComponent>(entity);
	//	transform->position.x = 0.0f;
	//	transform->position.y = 0.0f;
	//	transform->position.z = 0.0f;
	//	transform->scale = Vector3(0.25f, 0.25f, 1.0f);

	//	auto* sprite = componentRegistry->AddComponent<SpriteRendererComponent>(entity);
	//	sprite->anchor = Vector2(0.0f, 0.0f);
	//	sprite->pivot = Vector2(0.5f, 0.5f);

	//	auto* texture = componentRegistry->AddComponent<TextureComponent>(entity);
	//	texture->m_TextureData = m_SceneManagerContext->resource->Load<TextureData>("Asset\\Texture\\texture.jpg");
	//	texture->Material.Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	//}

	int Sample = 5;
	float Distance = 20.0f;
	for(int i = 0; i < Sample; i++){

		//エンティティを作成し、TransformとModelRendererを追加
		Entity entity = entityRegistry->Create();

		auto* name = componentRegistry->AddComponent<NameComponent>(entity);
		name->name = "Enemy" + std::to_string(i + 1);

		auto* texture = componentRegistry->AddComponent<TextureComponent>(entity);
		texture->m_TextureData = m_SceneManagerContext->resource->Load<TextureData>("Asset\\Texture\\white.tga");
		texture->Material.Diffuse = DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);

		// TransformComponentを追加
		auto* transform = componentRegistry->AddComponent<TransformComponent>(entity);
		transform->position = Vector3(cosf(float(i) / Sample * DirectX::XM_2PI) * Distance, 0.2f, sinf(float(i) / Sample * DirectX::XM_2PI) * Distance);
		transform->scale = Vector3(1.0f, 1.0f, 1.0f);
		transform->SetRotationEuler(Vector3(0.0f, float(i) / Sample * -DirectX::XM_2PI - DirectX::XM_PI * 0.5f, 0.0f));


		// ModelRendererComponentを追加
		auto* modelRenderer = componentRegistry->AddComponent<ModelRendererComponent>(entity);
		modelRenderer->model = resource->Load<ModelData>("Asset\\Model\\player.obj", false);
		modelRenderer->vertexShader = resource->Load<VertexShaderData>("Asset\\Shader\\commonVS.cso");
		modelRenderer->pixelShader = resource->Load<PixelShaderData>("Asset\\Shader\\ToonShaderPS.cso");

		// OutLineComponentを追加
		//auto* outline = componentRegistry->AddComponent<OutlineComponent>(entity);
	}

	
}

bool Scene::LoadFromYAMLFile(){
	SetTaskBarState(TBPF_INDETERMINATE); // タスクバーの状態をインジケーターに設定
	std::string filepath = LoadSceneFileDialog();
	if (filepath != "") {
		ResetAll();
		LoadSceneFromYAML(filepath);
		SetTaskBarState(TBPF_NOPROGRESS); // タスクバーの状態を通常に戻す
		return true;
	}
	SetTaskBarState(TBPF_NOPROGRESS); // タスクバーの状態を通常に戻す
	return false;
}

void Scene::Save(){
	SetTaskBarState(TBPF_INDETERMINATE); // タスクバーの状態をインジケーターに設定

	std::wstring savePath;
	if(!SaveSceneFileDialog(savePath)){
		SetTaskBarState(TBPF_NOPROGRESS); // タスクバーの状態を通常に戻す
		m_SceneManagerContext->debug->LOG_INFO("ユーザーがキャンセルしました。");
		return;
	}
	
	YAML::Node root;
	YAML::Node entitiesNode = YAML::Node(YAML::NodeType::Sequence);
	const auto& entities = m_entityRegistry->GetAllAlive();
	m_SceneManagerContext->debug->LOG_INFO(("保存対象エンティティ数: " + std::to_string(entities.size())).c_str());

	for (Entity e : entities) {
		YAML::Node entityNode;
		entityNode["Entity"] = static_cast<int>(e);
		YAML::Node componentsNode = YAML::Node(YAML::NodeType::Sequence);
		for (IComponent* comp : m_componentRegistry->GetAllComponentsOfEntitySorted(e)) {
			if (comp) {
				YAML::Node compNode;

				// 型情報を取得
				std::type_index ti(typeid(*comp));
				// 型IDを取得
				auto compId = m_componentRegistry->GetComponentIDByTypeIndex(ti);
				// ID→名前マップを取得
				const auto& idToName = m_componentRegistry->GetComponentIDToNameMap();
				auto it = idToName.find(compId);
				if(it != idToName.end()){
					const std::string& compName = it->second;
					YAML::Node compNode;
					if(it != idToName.end()){
						const std::string& compName = it->second;
						compNode["Component"] = compName;
					}
					YAML::Node encoded = comp->encode();
					// encodedの内容をcompNodeにマージ
					if(encoded && encoded.IsMap()){
						for(auto it = encoded.begin(); it != encoded.end(); ++it){
							compNode[it->first.as<std::string>()] = it->second;
						}
					}
					if(compNode && compNode.IsMap()){
						componentsNode.push_back(compNode);
					}
				}
				YAML::Node compNode2 = comp->encode();

				compNode.push_back(compNode2);  // 各コンポーネントがTypeキーを含むマップを返す
				if (compNode && compNode.IsMap()) {
					componentsNode.push_back(compNode);
				}
			}
		}

		entityNode["Components"] = componentsNode;
		entitiesNode.push_back(entityNode);
	}

	root["Entities"] = entitiesNode;

    std::string utf8Path = std::filesystem::path(savePath).string();


	std::ofstream fout(utf8Path, std::ios::binary);
	if (!fout.is_open()) {
		m_SceneManagerContext->debug->LOG_ERROR(("ファイルの保存に失敗しました: " + utf8Path).c_str());
		SetTaskBarState(TBPF_NOPROGRESS); // タスクバーの状態を通常に戻す

		return;
	}
	fout << root;

	SetTaskBarState(TBPF_NOPROGRESS); // タスクバーの状態を通常に戻す
}

void Scene::TempSave(){
	std::wstring savePath =L"TempSave.yaml";

	YAML::Node root;
	YAML::Node entitiesNode = YAML::Node(YAML::NodeType::Sequence);
	const auto& entities = m_entityRegistry->GetAllAlive();
	m_SceneManagerContext->debug->LOG_INFO(("保存対象エンティティ数: " + std::to_string(entities.size())).c_str());

	for(Entity e : entities){
		YAML::Node entityNode;
		entityNode["Entity"] = static_cast<int>(e);
		YAML::Node componentsNode = YAML::Node(YAML::NodeType::Sequence);
		for(IComponent* comp : m_componentRegistry->GetAllComponentsOfEntitySorted(e)){
			if(comp){
				YAML::Node compNode;

				// 型情報を取得
				std::type_index ti(typeid(*comp));
				// 型IDを取得
				auto compId = m_componentRegistry->GetComponentIDByTypeIndex(ti);
				// ID→名前マップを取得
				const auto& idToName = m_componentRegistry->GetComponentIDToNameMap();
				auto it = idToName.find(compId);
				if(it != idToName.end()){
					const std::string& compName = it->second;
					YAML::Node compNode;
					if(it != idToName.end()){
						const std::string& compName = it->second;
						compNode["Component"] = compName;
					}
					YAML::Node encoded = comp->encode();
					// encodedの内容をcompNodeにマージ
					if(encoded && encoded.IsMap()){
						for(auto it = encoded.begin(); it != encoded.end(); ++it){
							compNode[it->first.as<std::string>()] = it->second;
						}
					}
					if(compNode && compNode.IsMap()){
						componentsNode.push_back(compNode);
					}
				}
				YAML::Node compNode2 = comp->encode();

				compNode.push_back(compNode2);  // 各コンポーネントがTypeキーを含むマップを返す
				if(compNode && compNode.IsMap()){
					componentsNode.push_back(compNode);
				}
			}
		}

		entityNode["Components"] = componentsNode;
		entitiesNode.push_back(entityNode);
	}

	root["Entities"] = entitiesNode;

	std::string utf8Path = std::filesystem::path(savePath).string();


	std::ofstream fout(utf8Path, std::ios::binary);
	if(!fout.is_open()){
		m_SceneManagerContext->debug->LOG_ERROR(("ファイルの保存に失敗しました: " + utf8Path).c_str());
		SetTaskBarState(TBPF_NOPROGRESS); // タスクバーの状態を通常に戻す

		return;
	}
	fout << root;

	SetTaskBarState(TBPF_NOPROGRESS); // タスクバーの状態を通常に戻す
}

void Scene::TempLoad(){
	ResetAll(); // 一時保存の読み込み前に全エンティティをリセット
	LoadSceneFromYAML("TempSave.yaml");
}

void Scene::ResetAll(){
	auto aliveEntities = m_entityRegistry->GetAllAlive();

	for(auto e : aliveEntities){
		m_entityRegistry->Destroy(e);
		m_componentRegistry->OnEntityDestroyed(e);

	}
	m_entityRegistry->ResetAll();
}

void Scene::LoadSceneFromYAML(std::string path) {  
	std::ifstream fin(path);  
	if(!fin.is_open()){  
		// ファイルが開けなかった場合  
		m_SceneContext.manager->debug->LOG_ERROR(("Failed to open file: " + path).c_str());  
		return;  
	}  

	YAML::Node root = YAML::Load(fin);  
	if(!root["Entities"] || !root["Entities"].IsSequence()){  
		m_SceneContext.manager->debug->LOG_ERROR("YAML: 'Entities' node missing or invalid");  
		return;  
	}  

	ScenePath = path; // シーンのパスを設定

	YAML::Node entities = root["Entities"];  
	for(const auto& entityNode : entities){  

		if(!entityNode["Entity"]){  
			continue;  
		}  
		Entity entity = m_entityRegistry->Create();  

		if(!entityNode["Components"] || !entityNode["Components"].IsSequence()){  
			continue;  
		}  
		for(const auto& compNode : entityNode["Components"]){  
			const auto& compType = compNode["Component"].as<std::string>();  
			IComponent* comp = m_componentRegistry->CreateFromYAML(compType, entity, compNode);  
			if(compType == "ModelRendererComponent"){  
				auto* modelRenderer = dynamic_cast<ModelRendererComponent*>(comp);  
				if (modelRenderer) {
					if (compNode["isBlender"]) {
						const auto& isBlender = compNode["isBlender"].as<bool>();
						modelRenderer->isBlender = isBlender;
					}

					if(compNode["FilePath"]){
						const auto& FilePath = compNode["FilePath"].as<std::string>();
						modelRenderer->model = m_SceneManagerContext->resource->Load<ModelData>(FilePath.c_str(), modelRenderer->isBlender);
					}
					if(compNode["VertexShader"]){

						const auto& VertexShader = compNode["VertexShader"].as<std::string>();
						modelRenderer->vertexShader = m_SceneManagerContext->resource->Load<VertexShaderData>(VertexShader.c_str());
					}
					if(compNode["PixelShader"]){

						const auto& PixelShader = compNode["PixelShader"].as<std::string>();
						modelRenderer->pixelShader = m_SceneManagerContext->resource->Load<PixelShaderData>(PixelShader.c_str());
					}

					if (compNode["AnimationTime"]) {
						modelRenderer->animationTime = compNode["AnimationTime"].as<float>();
					}

					for (const auto& animNode : compNode["Animations"]) {
						std::string animName = animNode.first.as<std::string>();
						std::string animFile = animNode.second.as<std::string>();
						modelRenderer->model->LoadAnimation(animFile.c_str(), animName.c_str());
					}
				}
			}
			if(compType == "TextureComponent"){
				auto* texture = dynamic_cast<TextureComponent*>(comp);
				if(texture){
					if(compNode["FilePath"]){
						const auto& FilePath = compNode["FilePath"].as<std::string>();
						texture->m_TextureData = m_SceneManagerContext->resource->Load<TextureData>(FilePath.c_str());
					}
				}
			}
			if(compType == "BumpMapComponent"){
				auto* texture = dynamic_cast<BumpMapComponent*>(comp);
				if(texture){
					if(compNode["FilePath"]){
						const auto& FilePath = compNode["FilePath"].as<std::string>();
						texture->m_TextureData = m_SceneManagerContext->resource->Load<TextureData>(FilePath.c_str());
					}
				}
			}
		}
	}
}

std::string Scene::LoadSceneFileDialog() {
	char filename[MAX_PATH] = "";

	OPENFILENAMEA ofn = {}; // ANSI版（UNICODEなら OPENFILENAMEW）
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = nullptr; // ウィンドウハンドル（必要なら自分のウィンドウ）
	ofn.lpstrFilter = "YAML Files (*.yaml)\0*.yaml\0All Files (*.*)\0*.*\0";
	ofn.lpstrFile = filename;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
	ofn.Flags |= OFN_NOCHANGEDIR;

	ofn.lpstrDefExt = "yaml";


	if (GetOpenFileNameA(&ofn)) {
		return std::string(filename);
	}

	return std::string("");
}

bool Scene::SaveSceneFileDialog(std::wstring& outPath){
	WCHAR szFile[MAX_PATH] = L"scene.yaml";  // デフォルトファイル名
	OPENFILENAME ofn = {sizeof(ofn)};
	ofn.hwndOwner = nullptr;                 // 親ウィンドウハンドルを渡す場合は指定
	ofn.lpstrFilter = L"YAML Files (*.yaml)\0*.yaml\0All Files (*.*)\0*.*\0";
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
	ofn.Flags |= OFN_NOCHANGEDIR;

	if(GetSaveFileName(&ofn)){
		outPath = szFile;
		return true;
	}
	return false;
}
