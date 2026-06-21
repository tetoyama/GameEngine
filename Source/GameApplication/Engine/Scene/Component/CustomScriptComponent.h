// =======================================================================
//
// CustomScriptComponent.h
//
// =======================================================================
#pragma once

#include "Interface/IComponent.h"
#include "Backends/YAMLConverters.h"
#include "DebugTools/ImGuiSystem.h"
#include "../System/Script/ScriptExecution.h"
#include "../Command/EntityCommandBuffer.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>

#include "Scene.h"
#include "SceneManager.h"
#include "Entity/Entity.h"
#include "../Reference/EntityRef.h"
#include "../Reference/ComponentRef.h"
#include "Registry/ComponentRegistry.h"
#include "Platform/InputSystem/InputSystem.h"
#include "../System/Physic/HitInfo.h"

class ComponentRegistry;

// ユーザー定義スクリプトの基底コンポーネント
class CustomScriptComponent: public IComponent {
public:
	CustomScriptComponent(){
		executionSettings.registrationOrder =
			s_nextRegistrationOrder.fetch_add(1, std::memory_order_relaxed);
	}

	CustomScriptComponent(std::string name): CustomScriptComponent(){
		scriptName = std::move(name);
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

	void CollisionEnter(const HitInfo& hit){
		if(isInitialized) OnCollisionEnter(hit);
	}

	void CollisionStay(const HitInfo& hit){
		if(isInitialized) OnCollisionStay(hit);
	}

	void CollisionExit(const HitInfo& hit){
		if(isInitialized) OnCollisionExit(hit);
	}

	void TriggerEnter(const HitInfo& hit){
		if(isInitialized) OnTriggerEnter(hit);
	}

	void TriggerExit(const HitInfo& hit){
		if(isInitialized) OnTriggerExit(hit);
	}

	virtual void OnInitialize() {}
	virtual void OnStart() {}
	virtual void OnUpdate(float dt){}
	virtual void OnFixedUpdate(float dt){}
	virtual void OnDraw(){}
	virtual void OnEditorUpdate(float dt){}
	virtual void OnStop(){}

	virtual void OnCollisionEnter(const HitInfo& hit) {}
	virtual void OnCollisionStay(const HitInfo& hit)  {}
	virtual void OnCollisionExit(const HitInfo& hit)  {}
	virtual void OnTriggerEnter(const HitInfo& hit)   {}
	virtual void OnTriggerExit(const HitInfo& hit)    {}

	virtual YAML::Node encode() override{
		YAML::Node node;
		node["ScriptName"] = scriptName;
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override{
		if(node["ScriptName"]){
			scriptName = node["ScriptName"].as<std::string>();
		}
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

	void SetExecutionOrder(
		SystemTaskDomain domain,
		SystemPhase phase,
		int32_t priority
	){
		executionSettings.SetOrder(domain, phase, priority);
	}

	SystemTaskOrder GetExecutionOrder(SystemTaskDomain domain) const{
		return executionSettings.GetOrder(domain);
	}

	bool IsInitialized() const{
		return isInitialized;
	}

	// 既存互換の即時取得・追加API。
	template<typename T>
	T* GetComponent(){
		if(!m_ref.IsValid()) return nullptr;
		return m_ref.GetScene()->component->GetComponent<T>(m_ref.GetEntityID());
	}

	template<typename T, typename... Args>
	T* AddComponent(Args&&... args){
		if(!m_ref.IsValid()) return nullptr;
		return m_ref.GetScene()->component->AddComponent<T>(
			m_ref.GetEntityID(),
			std::forward<Args>(args)...
		);
	}

	// Schedule実行中に使用する遅延構造変更API。
	CommandEntity QueueCreateEntity(){
		EntityCommandBuffer* commands = GetCommandBuffer();
		return commands ? commands->CreateEntity() : CommandEntity{};
	}

	bool QueueDestroyEntity(Entity entity){
		EntityCommandBuffer* commands = GetCommandBuffer();
		if(!commands || !entity) return false;
		commands->DestroyEntity(entity);
		return true;
	}

	bool QueueDestroySelf(){
		return QueueDestroyEntity(m_ref.GetEntityID());
	}

	template<typename T, typename... Args>
	bool QueueAddComponent(Entity entity, Args&&... args){
		EntityCommandBuffer* commands = GetCommandBuffer();
		if(!commands || !entity) return false;
		commands->AddComponent<T>(entity, std::forward<Args>(args)...);
		return true;
	}

	template<typename T, typename... Args>
	bool QueueAddComponent(CommandEntity entity, Args&&... args){
		EntityCommandBuffer* commands = GetCommandBuffer();
		if(!commands) return false;
		commands->AddComponent<T>(entity, std::forward<Args>(args)...);
		return true;
	}

	template<typename T>
	bool QueueRemoveComponent(Entity entity){
		EntityCommandBuffer* commands = GetCommandBuffer();
		if(!commands || !entity) return false;
		commands->RemoveComponent<T>(entity);
		return true;
	}

	template<typename T>
	bool QueueRemoveComponent(CommandEntity entity){
		EntityCommandBuffer* commands = GetCommandBuffer();
		if(!commands) return false;
		commands->RemoveComponent<T>(entity);
		return true;
	}

	bool QueueEntitySetup(
		CommandEntity entity,
		std::function<void(Entity, SceneContext&)> callback
	){
		EntityCommandBuffer* commands = GetCommandBuffer();
		if(!commands || !callback) return false;
		commands->Execute(entity, std::move(callback));
		return true;
	}

	EntityRef GetEntityRef() const {
		return m_ref;
	}

	template<typename T>
	ComponentRef<T> GetComponentRef() const {
		return ComponentRef<T>(m_ref.GetEntityID(), m_ref.GetScene());
	}

	EntityRef GetEntityRefFor(Entity e) const {
		return EntityRef(e, m_ref.GetScene());
	}

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

	void SetContext(SceneContext* context, Entity entity){
		m_ref = EntityRef(entity, context);
	}

protected:
	EntityCommandBuffer* GetCommandBuffer() const {
		SceneContext* context = m_ref.GetScene();
		return context ? context->commands : nullptr;
	}

	std::string scriptName = "CustomScript";
	ScriptExecutionSettings executionSettings;
	bool isInitialized = false;
	EntityRef m_ref;

private:
	inline static std::atomic<uint64_t> s_nextRegistrationOrder{1};
};
