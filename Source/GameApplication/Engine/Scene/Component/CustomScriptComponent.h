#pragma once
#include "Interface/IComponent.h"
#include "BackEnds/YAMLConverters.h"
#include "DebugTools/ImGuiSystem.h"
#include <string>
//#include "System/CustomScriptSystem.h" // For ScriptFactory

#include "Scene.h"
#include "SceneManager.h"
#include "Entity/Entity.h"
#include "../Reference/EntityRef.h"
#include "../Reference/ComponentRef.h"
#include "Registry/ComponentRegistry.h"
#include "Platform/InputSystem/InputSystem.h"
#include "HitInfo.h"

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

	// PhysicSystem から呼び出されるコリジョン / トリガーイベントディスパッチャ
	void CollisionEnter(const HitInfo& hit) { if(isInitialized) OnCollisionEnter(hit); }
	void CollisionStay(const HitInfo& hit)  { if(isInitialized) OnCollisionStay(hit); }
	void CollisionExit(const HitInfo& hit)  { if(isInitialized) OnCollisionExit(hit); }
	void TriggerEnter(const HitInfo& hit)   { if(isInitialized) OnTriggerEnter(hit); }
	void TriggerExit(const HitInfo& hit)    { if(isInitialized) OnTriggerExit(hit); }

	// 派生クラスでオーバーライド可能な仮想関数
	virtual void OnInitialize() {}
	virtual void OnStart() {}
	virtual void OnUpdate(float dt){}
	virtual void OnFixedUpdate(float dt){}
	virtual void OnDraw(){}
	virtual void OnEditorUpdate(float dt){}
	virtual void OnStop(){}

	// コリジョン / トリガーイベント（PhysicSystem が呼び出す）
	virtual void OnCollisionEnter(const HitInfo& hit) {}
	virtual void OnCollisionStay(const HitInfo& hit)  {}
	virtual void OnCollisionExit(const HitInfo& hit)  {}
	virtual void OnTriggerEnter(const HitInfo& hit)   {}
	virtual void OnTriggerExit(const HitInfo& hit)    {}

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
		if(!m_ref.IsValid()) return nullptr;
		return m_ref.GetScene()->component->GetComponent<T>(m_ref.GetEntityID());
	}

	template<typename T, typename... Args>
	T* AddComponent(Args&&... args){
		if(!m_ref.IsValid()) return nullptr;
		return m_ref.GetScene()->component->AddComponent<T>(m_ref.GetEntityID(), std::forward<Args>(args)...);
	}

	// 自分自身の Entity への安全なリファレンスを返す
	// マルチシーンでも SceneContext で識別されるため一意に扱える
	EntityRef GetEntityRef() const {
		return m_ref;
	}

	// 自分自身の指定コンポーネントへの安全なリファレンスを返す
	template<typename T>
	ComponentRef<T> GetComponentRef() const {
		return ComponentRef<T>(m_ref.GetEntityID(), m_ref.GetScene());
	}

	// 任意のエンティティへの安全なリファレンスを作成する（同一シーン内）
	EntityRef GetEntityRefFor(Entity e) const {
		return EntityRef(e, m_ref.GetScene());
	}

	// 任意のエンティティのコンポーネントへの安全なリファレンスを作成する（同一シーン内）
	template<typename T>
	ComponentRef<T> GetComponentRefFor(Entity e) const {
		return ComponentRef<T>(e, m_ref.GetScene());
	}

	void LoadScene(const std::string& scenePath){
		auto* ctx = m_ref.GetScene();
		if(!ctx || !ctx->manager || !ctx->manager->sceneManager) return;
		auto setScene = ctx->manager->sceneManager->LoadFromFilePath(scenePath);
		ctx->manager->sceneManager->DeferredLoadScene(setScene);
	}

	bool GetKeyUp(int keyCode) const{
		auto* ctx = m_ref.GetScene();
		if(!ctx || !ctx->manager || !ctx->manager->input) return false;
		return ctx->manager->input->IsKeyUp(ctx->manager->hwnd, keyCode);
	}

	bool GetKeyDown(int keyCode) const {
		auto* ctx = m_ref.GetScene();
		if(!ctx || !ctx->manager || !ctx->manager->input) return false;
		return ctx->manager->input->IsKeyDown(ctx->manager->hwnd, keyCode);
	}

	bool GetKey(int keyCode) const{
		auto* ctx = m_ref.GetScene();
		if(!ctx || !ctx->manager || !ctx->manager->input) return false;
		return ctx->manager->input->IsKey(ctx->manager->hwnd, keyCode);
	}

	// 所属エンティティとコンテキストのセット
	void SetContext(SceneContext* context, Entity entity){
		m_ref = EntityRef(entity, context);
	}

protected:
	std::string scriptName = "CustomScript";
	bool isInitialized = false; // 初期化フラグ
	EntityRef m_ref;

private:

};