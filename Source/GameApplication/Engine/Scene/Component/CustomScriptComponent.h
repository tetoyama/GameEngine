#pragma once
#include "Interface/IComponent.h"
#include "BackEnds/YAMLConverters.h"
#include "DebugTools/ImGuiSystem.h"
#include <string>
//#include "System/CustomScriptSystem.h" // For ScriptFactory

#include "Scene.h"
#include "SceneManager.h"
#include "Entity/Entity.h"
#include "Entity/EntityRef.h"
#include "Entity/ComponentRef.h"
#include "Registry/ComponentRegistry.h"
#include "Platform/InputSystem/InputSystem.h"

class ComponentRegistry;

class CustomScriptComponent: public IComponent {

public:
	CustomScriptComponent() = default;
	CustomScriptComponent(std::string name){
		scriptName = name;
	}
	virtual ~CustomScriptComponent() = default;

	void Initialize(){
		OnInitialize();
	}

	void Start(){
		if(!isInitialized){
			OnStart();
			isInitialized = true;
		}
	}
	void Update(float dt){
		if(isInitialized){
			OnUpdate(dt);
		}
	}
	void FixedUpdate(float dt){
		if(isInitialized){
			OnFixedUpdate(dt);
		}
	}
	void Draw(){
		if(isInitialized){
			OnDraw();
		}
	}
	void EditorUpdate(float dt){
		if(isInitialized){
			OnEditorUpdate(dt);
		}
	}
	void Stop(){
		if(isInitialized){
			OnStop();
			isInitialized = false;
		}
	}

	// 派生クラスでオーバーライド可能な仮想関数
	virtual void OnInitialize() {}
	virtual void OnStart() {}
	virtual void OnUpdate(float dt){}
	virtual void OnFixedUpdate(float dt){}
	virtual void OnDraw(){}
	virtual void OnEditorUpdate(float dt){}
	virtual void OnStop(){}

	// 共通のプロパティやメソッド
	virtual YAML::Node encode() override{
		YAML::Node node;
		node["ScriptName"] = scriptName;
		return node;
	}
	bool decode(SceneContext* context, const YAML::Node& node) override{
		if(node["ScriptName"])
			scriptName = node["ScriptName"].as<std::string>();
		return true;
	}
	virtual void inspector(SceneContext* context) override{
		ImGui::Text(scriptName.c_str());
	}

	void SetScriptName(const std::string& name){
		scriptName = name;
	}
	const std::string& GetScriptName() const{
		return scriptName;
	}

	bool IsInitialized() const{
		return isInitialized;
	}

	// UnityライクなAPI
	template<typename T>
	T* GetComponent(){
		if(!m_context || !m_context->component) return nullptr;
		return m_context->component->GetComponent<T>(m_entity);
	}

	template<typename T, typename... Args>
	T* AddComponent(Args&&... args){
		if(!m_context || !m_context->component) return nullptr;
		return m_context->component->AddComponent<T>(m_entity, std::forward<Args>(args)...);
	}

	// 自分自身の Entity への安全なリファレンスを返す
	// マルチシーンでも SceneContext で識別されるため一意に扱える
	EntityRef GetEntityRef() const {
		return EntityRef(m_entity, m_context);
	}

	// 自分自身の指定コンポーネントへの安全なリファレンスを返す
	template<typename T>
	ComponentRef<T> GetComponentRef() const {
		return ComponentRef<T>(m_entity, m_context);
	}

	// 任意のエンティティへの安全なリファレンスを作成する（同一シーン内）
	EntityRef GetEntityRefFor(Entity e) const {
		return EntityRef(e, m_context);
	}

	// 任意のエンティティのコンポーネントへの安全なリファレンスを作成する（同一シーン内）
	template<typename T>
	ComponentRef<T> GetComponentRefFor(Entity e) const {
		return ComponentRef<T>(e, m_context);
	}

	void LoadScene(const std::string& scenePath){
		auto setScene = m_context->manager->sceneManager->LoadFromFilePath(scenePath);
		m_context->manager->sceneManager->DeferredLoadScene(setScene);
	}

	bool GetKeyUp(int keyCode) const{
		if(!m_context || !m_context->manager || !m_context->manager->input) return false;
		return m_context->manager->input->IsKeyUp(m_context->manager->hwnd, keyCode);
	}

	bool GetKeyDown(int keyCode) const {
		if(!m_context || !m_context->manager || !m_context->manager->input) return false;
		return m_context->manager->input->IsKeyDown(m_context->manager->hwnd, keyCode);
	}

	bool GetKey(int keyCode) const{
		if(!m_context || !m_context->manager || !m_context->manager->input) return false;
		return m_context->manager->input->IsKey(m_context->manager->hwnd, keyCode);
	}

	// 所属エンティティとコンテキストのセット
	void SetContext(SceneContext* context, Entity entity){
		m_context = context;
		m_entity = entity;
	}

protected:
	std::string scriptName = "CustomScript";
	bool isInitialized = false; // 初期化フラグ
	SceneContext* m_context = nullptr;
	Entity m_entity = 0;

private:

};