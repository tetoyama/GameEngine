// Engine/Scene/scene.cpp

#include "scene.h"
#include <commdlg.h> // GetOpenFileName
#include <filesystem>
#include "Backends/Taskbar/taskbar.h"

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

#include "System/C#ScriptSystem.h"

#include "Component/entityNameComponent.h"
#include "Component/transformComponent.h"
#include "Component/cameraComponent.h"
#include "Component/modelRendererComponent.h"
#include "Component/meshRendererComponent.h"
#include "Component/BillBoardRendererComponent.h"
#include "Component/textureComponent.h"
#include "Component/CustomScriptComponent.h"

#include "Component/C#ScriptComponent.h"

#include "Component/playerComponent.h"
#include "Component/bulletComponent.h"
#include "Component/enemyComponent.h"
#include "Component/explosionEffectComponent.h"
#include <Component/bumpMapComponent.h>
#include <Component/2DspriteRendererComponent.h>
#include <Component/RenderLayerComponent.h>
#include <System/CustomScriptSystem.h>
#include <Script/SetScene.h>

Scene::Scene(){

}

Scene::~Scene(){
}

void Scene::Initialize(SceneManagerContext* set){

	m_SceneManagerContext = set;
	m_SceneManagerContext->debug->LOG_INFO("Sceneを初期化中...");

	m_OldState = m_SceneContext.state;

	m_entityRegistry = std::make_shared<EntityRegistry>();
	m_componentRegistry = std::make_shared<ComponentRegistry>(m_entityRegistry.get(),&m_SceneContext);
	m_systemRegistry = std::make_shared<SystemRegistry>();

	// コンポーネントを登録（Archetype or Sparse を選択）
	// ボトルネックが見つかったコンポーネントから ArchetypeStorage<T> に移行
	m_componentRegistry->RegisterYAMLComponent<NameComponent>("NameComponent", false);
	m_componentRegistry->RegisterYAMLComponent<TransformComponent>("TransformComponent", false);

	m_componentRegistry->RegisterYAMLComponent<TextureComponent>("TextureComponent", false);
	m_componentRegistry->RegisterYAMLComponent<BumpMapComponent>("BumpMapComponent", false);

	m_componentRegistry->RegisterYAMLComponent<RenderLayerComponent>("RenderLayerComponent", false);
	m_componentRegistry->RegisterYAMLComponent<OrderInLayerComponent>("OrderInLayerComponent", false);

	m_componentRegistry->RegisterYAMLComponent<MeshRendererComponent>("MeshRendererComponent", false);
	m_componentRegistry->RegisterYAMLComponent<ModelRendererComponent>("ModelRendererComponent", false);
	m_componentRegistry->RegisterYAMLComponent<BillBoardRendererComponent>("BillBoardRendererComponent", false);
	m_componentRegistry->RegisterYAMLComponent<SpriteRendererComponent>("SpriteRendererComponent", false);

	m_componentRegistry->RegisterYAMLComponent<CameraComponent>("CameraComponent", false);

	m_componentRegistry->RegisterYAMLComponent<CustomScriptComponent>("CustomScriptComponent", false);
	m_componentRegistry->RegisterYAMLComponent<CSharpScriptComponent>("CSharpScriptComponent", false);

	m_componentRegistry->RegisterYAMLComponent<PlayerComponent>("PlayerComponent", false);
	m_componentRegistry->RegisterYAMLComponent<BulletComponent>("BulletComponent", false);
	m_componentRegistry->RegisterYAMLComponent<EnemyComponent>("EnemyComponent", false);
	m_componentRegistry->RegisterYAMLComponent<ExplosionEffectComponent>("ExplosionEffectComponent", false);

	m_componentRegistry->RegisterYAMLComponent<SetScene>("SetScene", false);

	// システムを登録
	m_systemRegistry->RegisterSystem(std::make_unique<TransformSystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<CameraSystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<RenderSystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<InspectorSystem>(&m_SceneContext));

	m_systemRegistry->RegisterSystem(std::make_unique<PlayerSystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<BulletSystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<EnemySystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<ExplosionEffectSystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<CSharpScriptSystem>(&m_SceneContext));
	m_systemRegistry->RegisterSystem(std::make_unique<CustomScriptSystem>(&m_SceneContext));

	auto Renderer = m_SceneManagerContext->renderer;
	auto graphicsContext = Renderer->GetGraphicsContext();

	m_SceneContext.entity = m_entityRegistry.get();
	m_SceneContext.component = m_componentRegistry.get();
	m_SceneContext.system = m_systemRegistry.get();
	m_SceneContext.manager = m_SceneManagerContext;

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

	if(ScenePath.empty()){
		BuildDefaultScene();
	} else{
		OpenSceneYAML(ScenePath);
	}

	m_SceneManagerContext->debug->LOG_INFO("Sceneを開始します");

	m_systemRegistry->StartAll();
}

void Scene::Update(float deltaTime){

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
			TempLoad(); // 一時読み込み
			//m_systemRegistry->FinalizeAll();
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

void Scene::Render(){

	m_systemRegistry->DrawAll();
}

void Scene::Shutdown(){
	m_SceneManagerContext->debug->LOG_INFO("Sceneを終了中...");
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
		modelRenderer->vertexShader = resource->GetShaderLoader()->LoadVertexShader("Asset\\Shader\\commonVS.cso");
		modelRenderer->pixelShader = resource->GetShaderLoader()->LoadPixelShader("Asset\\Shader\\PixelShader.cso");

		auto* bumpMap = componentRegistry->AddComponent<BumpMapComponent>(entity);
		bumpMap->m_TextureData = m_SceneManagerContext->resource->GetTextureLoader()->LoadTexture("Asset\\Texture\\BumpMap/Normal.bmp");

	}

	{
		//エンティティを作成し、TransformとModelRendererを追加
		Entity entity = entityRegistry->Create();

		auto* name = componentRegistry->AddComponent<NameComponent>(entity);
		name->name = "Player";

		auto* texture = componentRegistry->AddComponent<TextureComponent>(entity);
		texture->m_TextureData = m_SceneManagerContext->resource->GetTextureLoader()->LoadTexture("Asset\\Texture\\white.tga");
		texture->Material.Diffuse = DirectX::XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);

		// TransformComponentを追加
		auto* transform = componentRegistry->AddComponent<TransformComponent>(entity);
		transform->position = Vector3(0.0f, 0.2f, 0);
		transform->scale = Vector3(1.0f, 1.0f, 1.0f);
		transform->rotation = Vector3(0.0f, 0.0f, 0.0f);


		// ModelRendererComponentを追加
		auto* modelRenderer = componentRegistry->AddComponent<ModelRendererComponent>(entity);
		modelRenderer->model = resource->GetModelLoader()->LoadModel("Asset\\Model\\player.obj");
		modelRenderer->vertexShader = resource->GetShaderLoader()->LoadVertexShader("Asset\\Shader\\commonVS.cso");
		modelRenderer->pixelShader = resource->GetShaderLoader()->LoadPixelShader("Asset\\Shader\\limLightPS.cso");

		auto player = componentRegistry->AddComponent<PlayerComponent>(entity);

		//auto* script = componentRegistry->AddComponent<ScriptComponent>(entity);
		//script->SetScriptName("PlayerScript"); // ★スクリプトクラス名
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
		transform->rotation = Vector3(-1.0f, 0.0f, 0.0f);


		// CameraComponentを追加
		auto* camera = componentRegistry->AddComponent<CameraComponent>(entity);

	}
	{
		Entity entity = entityRegistry->Create();

		auto* name = componentRegistry->AddComponent<NameComponent>(entity);
		name->name = "2DSprite";

		auto* transform = componentRegistry->AddComponent<TransformComponent>(entity);
		transform->position.x = 0.0f;
		transform->position.y = 0.0f;
		transform->position.z = 0.0f;
		transform->scale = Vector3(0.25f, 0.25f, 1.0f);

		auto* sprite = componentRegistry->AddComponent<SpriteRendererComponent>(entity);
		sprite->anchor = Vector2(0.0f, 0.0f);
		sprite->pivot = Vector2(0.5f, 0.5f);

		auto* texture = componentRegistry->AddComponent<TextureComponent>(entity);
		texture->m_TextureData = m_SceneManagerContext->resource->GetTextureLoader()->LoadTexture("Asset\\Texture\\texture.jpg");
		texture->Material.Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	}

	int Sample = 20;
	float Distance = 20.0f;
	for(int i = 0; i < Sample; i++){

		//エンティティを作成し、TransformとModelRendererを追加
		Entity entity = entityRegistry->Create();

		auto* name = componentRegistry->AddComponent<NameComponent>(entity);
		name->name = "Enemy" + std::to_string(i + 1);

		auto* texture = componentRegistry->AddComponent<TextureComponent>(entity);
		texture->m_TextureData = m_SceneManagerContext->resource->GetTextureLoader()->LoadTexture("Asset\\Texture\\white.tga");
		texture->Material.Diffuse = DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);

		// TransformComponentを追加
		auto* transform = componentRegistry->AddComponent<TransformComponent>(entity);
		transform->position = Vector3(cosf(float(i) / Sample * DirectX::XM_2PI) * Distance, 0.2f, sinf(float(i) / Sample * DirectX::XM_2PI) * Distance);
		transform->scale = Vector3(1.0f, 1.0f, 1.0f);
		transform->rotation = Vector3(0.0f, 0.0f, 0.0f);


		// ModelRendererComponentを追加
		auto* modelRenderer = componentRegistry->AddComponent<ModelRendererComponent>(entity);
		modelRenderer->model = resource->GetModelLoader()->LoadModel("Asset\\Model\\player.obj");
		modelRenderer->vertexShader = resource->GetShaderLoader()->LoadVertexShader("Asset\\Shader\\commonVS.cso");
		modelRenderer->pixelShader = resource->GetShaderLoader()->LoadPixelShader("Asset\\Shader\\limLightPS.cso");

		auto player = componentRegistry->AddComponent<EnemyComponent>(entity);
	}

	
}

bool Scene::Load(){
	SetTaskBarState(TBPF_INDETERMINATE); // タスクバーの状態をインジケーターに設定
	std::string filepath = OpenYALM();
	if (filepath != "") {
		ResetAll();
		OpenSceneYAML(filepath);
		SetTaskBarState(TBPF_NOPROGRESS); // タスクバーの状態を通常に戻す
		return true;
	}
	SetTaskBarState(TBPF_NOPROGRESS); // タスクバーの状態を通常に戻す
	return false;
}

void Scene::Save(){
	SetTaskBarState(TBPF_INDETERMINATE); // タスクバーの状態をインジケーターに設定

	std::wstring savePath;
	if(!ShowSaveFileDialog(savePath)){
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
	OpenSceneYAML("TempSave.yaml");
}

void Scene::ResetAll(){
	auto aliveEntities = m_entityRegistry->GetAllAlive();

	for(auto e : aliveEntities){
		m_entityRegistry->Destroy(e);
		m_componentRegistry->OnEntityDestroyed(e);

	}
	m_entityRegistry->ResetAll();
}

void Scene::OpenSceneYAML(std::string path) {  
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
						modelRenderer->model = m_SceneManagerContext->resource->GetModelLoader()->LoadModel(FilePath.c_str(), modelRenderer->isBlender);
					}
					if(compNode["VertexShader"]){

						const auto& VertexShader = compNode["VertexShader"].as<std::string>();
						modelRenderer->vertexShader = m_SceneManagerContext->resource->GetShaderLoader()->LoadVertexShader(VertexShader.c_str());
					}
					if(compNode["PixelShader"]){

						const auto& PixelShader = compNode["PixelShader"].as<std::string>();
						modelRenderer->pixelShader = m_SceneManagerContext->resource->GetShaderLoader()->LoadPixelShader(PixelShader.c_str());
					}
				}
			}
			if(compType == "TextureComponent"){
				auto* texture = dynamic_cast<TextureComponent*>(comp);
				if(texture){
					if(compNode["FilePath"]){
						const auto& FilePath = compNode["FilePath"].as<std::string>();
						texture->m_TextureData = m_SceneManagerContext->resource->GetTextureLoader()->LoadTexture(FilePath.c_str());
					}
				}
			}
			if(compType == "BumpMapComponent"){
				auto* texture = dynamic_cast<BumpMapComponent*>(comp);
				if(texture){
					if(compNode["FilePath"]){
						const auto& FilePath = compNode["FilePath"].as<std::string>();
						texture->m_TextureData = m_SceneManagerContext->resource->GetTextureLoader()->LoadTexture(FilePath.c_str());
					}
				}
			}
		}
	}
}

std::string Scene::OpenYALM() {
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

bool Scene::ShowSaveFileDialog(std::wstring& outPath){
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
