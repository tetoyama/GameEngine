// =======================================================================
// 
// scene.cpp
// 
// =======================================================================

#include "scene.h"

#include <commdlg.h> // GetOpenFileName
#include <filesystem>
#include <string>

#include "Backends/Taskbar/taskbar.h"
#include "Backends/convertWString.h"

#include "gameApplication.h"

#include "DebugTools/debugSystem.h"

#include "Editor/editorService.h"
#include "Editor/UI/Hierarchy.h"

#include "Graphics/mainRenderer.h"

#include "Platform/InputSystem/InputSystem.h"

#include "Resources/resourceService.h"
#include "Resources/Data/modelData.h"
#include "Resources/Loader/modelLoader.h"
#include "Resources/Loader/shaderLoader.h"
#include "Resources/Loader/textureLoader.h"
#include "Resources/Data/vertexShaderData.h"
#include "Resources/Data/pixelShaderData.h"

#include "sceneManager.h"

#include "Registry/entityRegistry.h"
#include "Registry/componentRegistry.h"
#include "Registry/systemRegistry.h"
#include "Prefab/PrefabSystem.h"

#include "Component/componentList.h"

Scene::Scene(){
}

Scene::~Scene(){
}

void Scene::Initialize(SceneManagerContext* set){
	
	m_SceneManagerContext = set;
	m_SceneManagerContext->pDebug->LOG_INFO(("Scene[" + SceneName + "]を初期化中...").c_str());

	m_SceneContext.pSystem = m_SceneManagerContext->pSystemRegistry;


	m_entityRegistry = std::make_shared<EntityRegistry>();
	m_componentRegistry = std::make_shared<ComponentRegistry>(m_entityRegistry.get(),&m_SceneContext);
	m_prefabSystem = std::make_shared<PrefabSystem>();


	// コンポーネントの登録
#define REGISTER(T, STORAGE) \
    m_componentRegistry->RegisterYAMLComponent<T>(#T, STORAGE == COMPONENT_ARCHETYPE);
	COMPONENT_LIST(REGISTER)
#undef REGISTER

	// シーンコンテキストの初期化
	auto m_Renderer= m_SceneManagerContext->pRenderer;
	
	m_SceneContext.pManager = m_SceneManagerContext;

	m_SceneContext.pEntity = m_entityRegistry.get();
	m_SceneContext.pComponent = m_componentRegistry.get();
	m_SceneContext.pPrefab = m_prefabSystem.get();

	auto m_GraphicsContext= Renderer->GetGraphicsContext();

	graphicsContext->SetDepthMode(DepthMode::Write);

	// デフォルトのシーンを構築
	if(ScenePath.empty()){
		BuildDefaultScene();
	} else{
		LoadSceneFromYAML(ScenePath);
	}
	m_SceneManagerContext->pDebug->LOG_INFO(("Scene[" + SceneName + "]を開始します").c_str());
}

void Scene::Update(float deltaTime){
}

void Scene::FixedUpdate(float fixedDeltaTime){
}

void Scene::Draw(){
}

void Scene::Shutdown(){
	m_SceneManagerContext->pDebug->LOG_INFO(("Scene[" + SceneName + "]を終了中...").c_str());

	if(m_SceneManagerContext->pEditor){
		auto m_Hierarchy= m_SceneManagerContext->pEditor->GetUI<Hierarchy>();
		if(hierarchy && hierarchy->pSceneContext == &m_SceneContext){
			hierarchy->pSceneContext = nullptr;
			hierarchy->selectedEntity = 0;
		}
	}

	ResetAll();

	// レジストリの終了処理
	m_entityRegistry.reset();
	m_componentRegistry.reset();
	m_prefabSystem.reset();
	m_SceneManagerContext->pDebug->LOG_INFO(("Scene[" + SceneName + "]を終了しました").c_str());
}

void Scene::BuildDefaultScene(){
	auto m_EntityRegistry= m_SceneContext.pEntity;
	auto m_ComponentRegistry= m_SceneContext.pComponent;

	auto m_Renderer= m_SceneManagerContext->pRenderer;
	auto m_GraphicsContext= pRenderer->GetGraphicsContext();

	graphicsContext->SetDepthMode(DepthMode::Write);

	{
		Entity m_Entity= entityRegistry->Create();

		auto* name = componentRegistry->AddComponent<NameComponent>(entity);
		name->name = "Field";

		// TransformComponentを追加
		auto* transform = componentRegistry->AddComponent<TransformComponent>(entity);
		transform->scale = Vector3(50.0f, 50.0f, 50.0f);
		transform->position = Vector3(0.0f, -transform->scale.y * 0.5f, 0);
		transform->SetRotationEuler(Vector3(0.0f, 0.0f, 0.0f));

		// ModelRendererComponentを追加
		auto* modelRenderer = componentRegistry->AddComponent<ModelRendererComponent>(entity);
		modelRenderer->modelFilePath = "Asset\\Model\\cube.obj";

		// TextureComponentを追加
		auto* texture = componentRegistry->AddComponent<TextureComponent>(entity);
		texture->m_TextureData = m_SceneManagerContext->pResource->Load<TextureData>("Asset\\Texture\\mesh.png");
		texture->UV_Slice_X = 0.04f;
		texture->UV_Slice_Y = 0.04f;

		// MaterialComponentを追加
		auto* material = componentRegistry->AddComponent<MaterialComponent>(entity);
		material->ShaderID = 1;
		material->Material.BaseColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		material->Material.Metallic = 0.25f;
		material->Material.Roughness = 0.5f;
		material->Material.MaterialFlags |= MATERIAL_FLAG_USE_ENVIRONMENT_MAP;

		// ColliderComponentを追加
		auto* collider = componentRegistry->AddComponent<ColliderComponent>(entity);

		// ボックスコライダーを追加
		ColliderShape m_Col;
		col.type = ColliderType::Box;
		col.offset = Vector3(0, 0, 0);
		col.size = Vector3(1, 1, 1);
		col.staticFriction = 0.0f;
		col.dynamicFriction = 0.0f;
		collider->colliders.push_back(col);
	}
	{
		Entity m_Entity= entityRegistry->Create();

		auto* name = componentRegistry->AddComponent<NameComponent>(entity);
		name->name = "Light";

		// TransformComponentを追加
		auto* transform = componentRegistry->AddComponent<TransformComponent>(entity);
		transform->position = Vector3(0.0f, 0.0f,0.0f);
		transform->scale = Vector3(1.0f, 1.0f, 1.0f);
		transform->SetRotationEuler(Vector3(0.75f, 1.0f, 0.0f));

		// LightComponentを追加
		auto* light = componentRegistry->AddComponent<LightComponent>(entity);
		light->light.LightType = LIGHT_TYPE_DIRECTIONAL_CSM;
		light->light.Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		light->light.Ambient = DirectX::XMFLOAT4(0.05f, 0.05f, 0.05f, 1.0f);
		light->light.Param.x = 500.0f;
	}
	{
		Entity m_Entity= entityRegistry->Create();

		auto* name = componentRegistry->AddComponent<NameComponent>(entity);
		name->name = "SkyBox";

		// TransformComponentを追加
		auto* transform = componentRegistry->AddComponent<TransformComponent>(entity);
		transform->position = Vector3(0.0f, 0.0f, 0.0f);
		transform->scale = Vector3(500.0f, 500.0f, 500.0f);
		transform->SetRotationEuler(Vector3(0.0f, 0.0f, 0.0f));

		auto* texture = componentRegistry->AddComponent<TextureComponent>(entity);
		texture->m_TextureData = m_SceneManagerContext->pResource->Load<TextureData>("Asset\\Texture\\Daylight.png");

		auto* material = componentRegistry->AddComponent<MaterialComponent>(entity);

		material->Material.BaseColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

		// ModelRendererComponentを追加
		auto* modelRenderer = componentRegistry->AddComponent<ModelRendererComponent>(entity);
		modelRenderer->isBlender = true;
		modelRenderer->model = m_SceneManagerContext->pResource->Load<ModelData>("Asset\\Model\\sky.fbx", true);

		auto* env = componentRegistry->AddComponent<EnvironmentMapComponent>(entity);
		env->enabled = true;
	}
	{
		Entity m_Entity= entityRegistry->Create();

		auto* name = componentRegistry->AddComponent<NameComponent>(entity);
		name->name = "Player";

		// TransformComponentを追加
		auto* transform = componentRegistry->AddComponent<TransformComponent>(entity);
		transform->position = Vector3(0.0f, 0.0f, 0.0f);
		transform->scale = Vector3(0.01f, 0.01f, 0.01f);
		transform->SetRotationEuler(Vector3(0.0f, 0.0f, 0.0f));


		// ModelRendererComponentを追加
		auto* modelRenderer = componentRegistry->AddComponent<ModelRendererComponent>(entity);
		modelRenderer->modelFilePath = "Asset\\Model\\Y Bot.fbx";
		modelRenderer->isBlender = false;

		modelRenderer->animations.push_back(std::make_pair("Idle", "Asset\\Model\\Akai_Idle.fbx"));
		modelRenderer->animations.push_back(std::make_pair("Run", "Asset\\Model\\Akai_Run.fbx"));
		modelRenderer->CreateModel(&m_SceneContext);

		// ColliderComponentを追加
		auto* collider = componentRegistry->AddComponent<ColliderComponent>(entity);
		collider->isDynamic = true;

		// カプセルコライダーを追加
		ColliderShape m_Col;
		col.type = ColliderType::Capsule;
		col.offset = Vector3(0, 75, 0);
		col.height = 100.0f;
		col.radius = 25.0f;
		col.rotationOffset = Vector3(0, 0, 90.0f);
		col.staticFriction = 0.0f;
		col.dynamicFriction = 0.0f;
		col.lockRotX = true;
		col.lockRotY = true;
		col.lockRotZ = true;
		col.collisionLayer = 1u << 1;	// Player レイヤー
		collider->colliders.push_back(col);

		// PlayerControllerスクリプトを追加
		auto* player = componentRegistry->AddComponent<CharacterController>(entity);

		// マテリアルコンポーネントを追加
		auto* material = componentRegistry->AddComponent<MaterialComponent>(entity);
		material->Material.BaseColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		material->ShaderID = 1;
	}

	{
		Entity m_Entity= entityRegistry->Create();

		auto* name = componentRegistry->AddComponent<NameComponent>(entity);
		name->name = "Camera";

		// TransformComponentを追加
		auto* transform = componentRegistry->AddComponent<TransformComponent>(entity);
		transform->position = Vector3(0.0f, 5.0f, -15.0f);
		transform->scale = Vector3(1.0f, 1.0f, 1.0f);
		transform->SetRotationEuler(Vector3(0.0f, 0.0f, 0.0f));

		// CameraComponentを追加
		auto* CameraBuffer = componentRegistry->AddComponent<CameraComponent>(entity);

		// CameraControllerスクリプトを追加
		auto* cameraController = componentRegistry->AddComponent<CameraController>(entity);
	}
}

bool Scene::LoadFromYAMLFile(){
	SetTaskBarState(TBPF_INDETERMINATE); // タスクバーの状態をインジケーターに設定
	std::string m_Filepath= LoadSceneFileDialog();
	if (filepath != "") {
		ResetAll();
		std::filesystem::path path(filepath);
		SceneName = path.stem().string();  // 拡張子を除いたファイル名
		LoadSceneFromYAML(filepath);
		SetTaskBarState(TBPF_NOPROGRESS); // タスクバーの状態を通常に戻す
		return m_True;
	}
	SetTaskBarState(TBPF_NOPROGRESS); // タスクバーの状態を通常に戻す
	return m_False;
}

void Scene::Save(){
	SetTaskBarState(TBPF_INDETERMINATE); // タスクバーの状態をインジケーターに設定
	std::wstring m_SavePath;

	if (ScenePath == "") {

		if (!SaveSceneFileDialog(savePath)) {
			SetTaskBarState(TBPF_NOPROGRESS); // タスクバーの状態を通常に戻す
			m_SceneManagerContext->pDebug->LOG_INFO("ユーザーがキャンセルしました。");
			return;
		}
	} else {
		savePath = std::filesystem::path(ScenePath);
	}
	std::filesystem::path m_Filepath= std::filesystem::path(savePath);
	ScenePath = filepath.string();
	SceneName = filepath.stem().string();  // 拡張子を除いたファイル名

	YAML::Node m_Root;
	YAML::Node m_EntitiesNode= YAML::Node(YAML::NodeType::Sequence);
	const auto& entities = m_entityRegistry->GetAllAlive();
	m_SceneManagerContext->pDebug->LOG_INFO(("Scene[" + SceneName + "]を保存します").c_str());
	m_SceneManagerContext->pDebug->LOG_INFO(("保存対象エンティティ数: " + std::to_string(entities.size())).c_str());

	for (Entity e : entities) {
		YAML::Node m_EntityNode;
		entityNode["Entity"] = static_cast<int>(e);
		YAML::Node m_ComponentsNode= YAML::Node(YAML::NodeType::Sequence);
		for (IComponent* comp : m_componentRegistry->GetAllComponentsOfEntitySorted(e)) {
			if (comp) {
				// 型情報を取得
				std::type_index ti(typeid(*comp));
				// 型IDを取得
				auto m_CompId= m_componentRegistry->GetComponentIDByTypeIndex(ti);
				// ID→名前マップを取得
				const auto& idToName = m_componentRegistry->GetComponentIDToNameMap();
				auto m_It= idToName.find(compId);
				if(it != idToName.end()){
					YAML::Node m_CompNode;
					compNode["Component"] = it->second;

					YAML::Node m_Encoded= comp->encode();
					// encodedの内容をcompNodeにマージ
					if(encoded && encoded.IsMap()){
						for(auto encIt = encoded.begin(); encIt != encoded.end(); ++encIt){
							compNode[encIt->first.as<std::string>()] = encIt->second;
						}
					}
					if(compNode && compNode.IsMap()){
						componentsNode.push_back(compNode);
					}
				}
			}
		}

		entityNode["Components"] = componentsNode;
		entitiesNode.push_back(entityNode);
	}

	root["Entities"] = entitiesNode;

	std::string m_Utf8Path= std::filesystem::path(savePath).string();


	std::ofstream fout(utf8Path, std::ios::binary);
	if (!fout.is_open()) {
		m_SceneManagerContext->pDebug->LOG_ERROR(("ファイルの保存に失敗しました: " + utf8Path).c_str());
		SetTaskBarState(TBPF_NOPROGRESS); // タスクバーの状態を通常に戻す

		return;
	}
	fout << root;

	SetTaskBarState(TBPF_NOPROGRESS); // タスクバーの状態を通常に戻す
}

void Scene::TempSave(){

	std::wstring m_SavePath= StringToWString(TEMP_SAVE_PATH) + L"Temp_" + StringToWString(SceneName) + L".yaml";

	YAML::Node m_Root;
	YAML::Node m_EntitiesNode= YAML::Node(YAML::NodeType::Sequence);
	const auto& entities = m_entityRegistry->GetAllAlive();
	m_SceneManagerContext->pDebug->LOG_INFO(("保存対象エンティティ数: " + std::to_string(entities.size())).c_str());

	for(Entity e : entities){
		YAML::Node m_EntityNode;
		entityNode["Entity"] = static_cast<int>(e);
		YAML::Node m_ComponentsNode= YAML::Node(YAML::NodeType::Sequence);
		for(IComponent* comp : m_componentRegistry->GetAllComponentsOfEntitySorted(e)){
			if(comp){
				// 型情報を取得
				std::type_index ti(typeid(*comp));
				// 型IDを取得
				auto m_CompId= m_componentRegistry->GetComponentIDByTypeIndex(ti);
				// ID→名前マップを取得
				const auto& idToName = m_componentRegistry->GetComponentIDToNameMap();
				auto m_It= idToName.find(compId);
				if(it != idToName.end()){
					YAML::Node m_CompNode;
					compNode["Component"] = it->second;

					YAML::Node m_Encoded= comp->encode();
					// encodedの内容をcompNodeにマージ
					if(encoded && encoded.IsMap()){
						for(auto encIt = encoded.begin(); encIt != encoded.end(); ++encIt){
							compNode[encIt->first.as<std::string>()] = encIt->second;
						}
					}
					if(compNode && compNode.IsMap()){
						componentsNode.push_back(compNode);
					}
				}
			}
		}

		entityNode["Components"] = componentsNode;
		entitiesNode.push_back(entityNode);
	}

	root["Entities"] = entitiesNode;

	std::string m_Utf8Path= std::filesystem::path(savePath).string();


	std::ofstream fout(utf8Path, std::ios::binary);
	if(!fout.is_open()){
		m_SceneManagerContext->pDebug->LOG_ERROR(("ファイルの保存に失敗しました: " + utf8Path).c_str());
		SetTaskBarState(TBPF_NOPROGRESS); // タスクバーの状態を通常に戻す

		return;
	}
	fout << root;

	SetTaskBarState(TBPF_NOPROGRESS); // タスクバーの状態を通常に戻す
}

void Scene::ResetAll(){
	auto m_AliveEntities= m_entityRegistry->GetAllAlive();

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
		m_SceneContext.pManager->pDebug->LOG_ERROR(("Failed to open file: " + path).c_str());  
		return;  
	}

	YAML::Node m_Root= YAML::Load(fin);  
	if(!root["Entities"] || !root["Entities"].IsSequence()){  
		m_SceneContext.pManager->pDebug->LOG_ERROR("YAML: 'Entities' node missing or invalid");  
		return;
	}

	ScenePath = path; // シーンのパスを設定
	m_SceneContext.pManager->pDebug->LOG_DEBUG(("Sceneをファイルから読み込みます:FilePath[" + ScenePath + "]").c_str());

	std::filesystem::path fpath(ScenePath);
	SceneName = fpath.stem().string();  // 拡張子を除いたファイル名

	YAML::Node m_Entities= root["Entities"];  
	for(const auto& entityNode : entities){  

		if(!entityNode["Entity"]){  
			continue;  
		}  
		Entity m_Entity= m_entityRegistry->Create();  

		if(!entityNode["Components"] || !entityNode["Components"].IsSequence()){  
			continue;  
		}
		for(const auto& compNode : entityNode["Components"]){  
			const auto& compType = compNode["Component"].as<std::string>();  
			IComponent* comp = m_componentRegistry->CreateFromYAML(compType, entity, compNode);  
		}
	}

	// 全エンティティのロードが完了した後で children リストを再構築する
	RebuildTransformChildren();
}

void Scene::RebuildTransformChildren() {
	auto m_Entities= m_componentRegistry->FindEntitiesWithComponent<TransformComponent>();
	for (Entity e : entities) {
		auto* tc = m_componentRegistry->GetComponent<TransformComponent>(e);
		if (tc) tc->children.clear();
	}
	for (Entity e : entities) {
		auto* tc = m_componentRegistry->GetComponent<TransformComponent>(e);
		if (!tc || tc->parent == 0) continue;
		auto* parentTc = m_componentRegistry->GetComponent<TransformComponent>(tc->parent);
		if (parentTc) parentTc->children.push_back(e);
	}
}

std::string Scene::LoadSceneFileDialog() {
	char m_Filename[MAX_PATH] = "";

	OPENFILENAMEA m_Ofn= {}; // ANSI版（UNICODEなら OPENFILENAMEW）
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = nullptr; // ウィンドウハンドル（必要なら自分のウィンドウ）
	ofn.lpstrFilter = "Scene Files (*.scene)\0*.scene\0YAML Files (*.yaml)\0*.yaml\0All Files (*.*)\0*.*\0";
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
	WCHAR m_SzFile[MAX_PATH] = L"scene.scene";  // デフォルトファイル名
	OPENFILENAME m_Ofn= {sizeof(ofn)};
	ofn.hwndOwner = nullptr;                 // 親ウィンドウハンドルを渡す場合は指定
	ofn.lpstrFilter = L"Scene Files (*.scene)\0*.scene\0YAML Files (*.yaml)\0*.yaml\0All Files (*.*)\0*.*\0";
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
	ofn.Flags |= OFN_NOCHANGEDIR;

	if(GetSaveFileName(&ofn)){
		outPath = szFile;
		return m_True;
	}
	return m_False;
}

RenderLayer Scene::GetRenderLayerFromEntity(Entity entity) {

	ComponentRegistry* registry = m_componentRegistry.get();

	auto* layerComponent = registry->GetComponent<RenderLayerComponent>(entity);
	if (layerComponent) {
		return layerComponent->layer;
	}
	if (registry->HasComponent<SpriteRendererComponent>(entity)) {
		return RenderLayer::OverlayUI;
	}
	if(registry->HasComponent<EffectComponent>(entity)){
		return RenderLayer::SortTransparent3D;
	}
	if (registry->HasComponent<ParticleComponent>(entity)) {
		return RenderLayer::SortTransparent3D;
	}
	auto* material = registry->GetComponent<MaterialComponent>(entity);
	if (material && material->Material.BaseColor.w < 1.0f) {
		return RenderLayer::SortTransparent3D;
	}
	return RenderLayer::Opaque3D;
}
