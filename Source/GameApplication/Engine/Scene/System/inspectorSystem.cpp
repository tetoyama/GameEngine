#include "inspectorSystem.h"
#include "Scene.h"
#include "sceneManager.h"

#include "Backends/ImGui/ImGui.h"
#include "Backends/ImGui/imgui_stdlib.h"

#include "Entity/Entity.h"
#include "Registry/componentRegistry.h"

#include "Component/transformComponent.h"
#include "Component/entityNameComponent.h"
#include "Component/cameraComponent.h"
#include "Component/playerComponent.h"

#include "Engine/DebugTools/debugSystem.h"

void InspectorSystem::Initialize(){
	m_context->manager->debug->LOG_DEBUG(u8"TransformSystemを初期化中...");
}

void InspectorSystem::Draw(){
	// コンポーネントを持つエンティティの検索
	const auto& entities = m_context->entity->GetAllAlive();
	if(entities.empty()){
		return;
	} else{
		std::string Title = "InspectorWindow";
		if(ImGui::Begin(Title.c_str())){

			for(Entity entity : entities){
				if(ImGui::TreeNodeEx(("EntityID:" + std::to_string(entity)).c_str(), ImGuiTreeNodeFlags_DefaultOpen)){

					NameComponent* name = m_context->component->GetComponent<NameComponent>(entity);
					if(name){
						if(ImGui::TreeNodeEx("name", ImGuiTreeNodeFlags_DefaultOpen)){

							if(ImGui::InputText("##Name", &name->name, ImGuiInputTextFlags_EnterReturnsTrue)){

							}
							ImGui::TreePop();
						}

					}

					TransformComponent* transform = m_context->component->GetComponent<TransformComponent>(entity);
					if(transform){
						if(ImGui::TreeNodeEx("transform", ImGuiTreeNodeFlags_DefaultOpen)){
							ImGui::Columns(2, nullptr, false);
							ImGui::SetColumnWidth(0, 75); // 左カラム幅調整

							ImGui::Text("Position");
							ImGui::NextColumn();
							ImGui::SetNextItemWidth(-1);
							ImGui::DragFloat3("##Position", &transform->position.x, transform->scale.length() * 0.01f);
							ImGui::NextColumn();

							ImGui::Text("Rotation");
							ImGui::NextColumn();
							ImGui::SetNextItemWidth(-1);
							ImGui::DragFloat3("##Rotation", &transform->rotation.x, 0.01f);
							ImGui::NextColumn();

							ImGui::Text("Scale");
							ImGui::NextColumn();
							ImGui::SetNextItemWidth(-1);
							ImGui::DragFloat3("##Scale", &transform->scale.x, transform->scale.length() * 0.01f);
							ImGui::Columns(1);

							ImGui::TreePop();
						}

					}

					CameraComponent* camera = m_context->component->GetComponent<CameraComponent>(entity);
					if(camera){
						if(ImGui::TreeNodeEx("camera", ImGuiTreeNodeFlags_DefaultOpen)){
							ImGui::TreePop();
						}
					}

					PlayerComponent* player = m_context->component->GetComponent<PlayerComponent>(entity);
					if(player){
						if(ImGui::TreeNodeEx("player", ImGuiTreeNodeFlags_DefaultOpen)){
							ImGui::TreePop();
						}
					}

					ImGui::TreePop();
				}
			}
		}

		ImGuiIO& io = ImGui::GetIO();
		if(io.DisplaySize.x > 0){
			ImGuiStyle& style = ImGui::GetStyle();
			ImVec2 pad = style.DisplayWindowPadding;

			ImVec2 pos = ImGui::GetWindowPos();
			ImVec2 size = ImGui::GetWindowSize();
			ImVec2 newPos = pos;
			bool need = false;
			if(size.x > io.DisplaySize.x - pad.x * 2){
				size.x = io.DisplaySize.x - pad.x * 2;
				need = true;
			}
			if(size.y > io.DisplaySize.y - pad.y * 2){
				size.y = io.DisplaySize.y - pad.y * 2;
				need = true;
			}
			ImVec2 newSize = size;

			if(pos.x < pad.x){
				newPos.x = pad.x;
				need = true;
			}
			if(pos.y < pad.y){
				newPos.y = pad.y;
				need = true;
			}
			if(pos.x + size.x > io.DisplaySize.x - pad.x){
				newPos.x = io.DisplaySize.x - pad.x - size.x;
				need = true;
			}
			if(pos.y + size.y > io.DisplaySize.y - pad.y){
				newPos.y = io.DisplaySize.y - pad.y - size.y;
				need = true;
			}

			if(need){
				ImGui::SetWindowSize(newSize, ImGuiCond_Always);
				ImGui::SetWindowPos(newPos, ImGuiCond_Always);
			}
		}
		ImGui::End();

	}
}