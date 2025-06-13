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

#include "Component/textureComponent.h"

#include "Engine/DebugTools/debugSystem.h"

void InspectorSystem::Initialize(){
	m_context->manager->debug->LOG_DEBUG(u8"TransformSystemを初期化中...");
}

void InspectorSystem::Draw() {
	// コンポーネントを持つエンティティの検索
	const auto& entities = m_context->entity->GetAllAlive();
	if (entities.empty()) {
		return;
	} else {
		std::string Title = "Inspector";
		if (ImGui::Begin("Inspector")) {
		}
		ImGui::End();

		for (Entity entity : entities) {
			if (ImGui::Begin("SceneEditor")) {

				std::string showName = "EntityID:" + std::to_string(entity);
				NameComponent* name = m_context->component->GetComponent<NameComponent>(entity);
				if (name) {

					showName = "EntityID:" + std::to_string(entity) + "     " + name->name;


				}
				if (ImGui::Selectable(showName.c_str())) {
					SelectEntity = entity;
				}
				if (ImGui::IsItemFocused()) {
					SelectEntity = entity;
				}
			}
			ImGui::End();

			if (SelectEntity == entity && ImGui::Begin("Inspector")) {

				if (ImGui::TreeNodeEx(("EntityID:" + std::to_string(entity)).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {

					NameComponent* name = m_context->component->GetComponent<NameComponent>(entity);
					if (name) {
						ImGui::Columns(2, nullptr, false);
						ImGui::SetColumnWidth(0, 75); // 左カラム幅調整
						ImGui::Text("Name");
						ImGui::NextColumn();
						ImGui::SetNextItemWidth(-1);
						if (ImGui::InputText("##Name", &name->name, ImGuiInputTextFlags_EnterReturnsTrue)) {

						}
						ImGui::Columns(1);

					}

					TransformComponent* transform = m_context->component->GetComponent<TransformComponent>(entity);
					if (transform) {

						ImGui::Text("Transform");

						ImGui::DragFloat3("Position", &transform->position.x, transform->scale.length() * 0.01f);

						ImGui::DragFloat3("Rotation", &transform->rotation.x, 0.01f);

						ImGui::DragFloat3("Scale", &transform->scale.x, transform->scale.length() * 0.01f);
					}

					TextureComponent* texture = m_context->component->GetComponent<TextureComponent>(entity);
					if (texture) {
						ImGui::Text("TextureComponent");

						ImGui::ColorEdit4("Diffuse", &texture->Material.Diffuse.x);

						ImGui::DragInt("Slice:X", &texture->UV_Slice_X, 0.1f, 1, 256);
						ImGui::DragInt("Slice:Y", &texture->UV_Slice_Y, 0.1f, 1, 256);

						ImGui::DragInt("Frame:", &texture->AnimationNum, 0.1f, 0, texture->UV_Slice_X * texture->UV_Slice_Y - 1);


					}
					
					CameraComponent* camera = m_context->component->GetComponent<CameraComponent>(entity);
					if (camera) {
						if (ImGui::TreeNodeEx("CameraComponent", ImGuiTreeNodeFlags_DefaultOpen)) {

							ImGui::DragFloat("FOV", &camera->FOV, 0.01f, 0.02f, DirectX::XM_PI);

							ImGui::DragFloat("NearClip", &camera->NearClip, 0.01f, 0.01f, camera->FarClip);
							ImGui::DragFloat("FarClip", &camera->FarClip, 0.01f, 0.01f);

							ImGui::TreePop();
						}
					}

					PlayerComponent* player = m_context->component->GetComponent<PlayerComponent>(entity);
					if (player) {
						if (ImGui::TreeNodeEx("PlayerComponent", ImGuiTreeNodeFlags_DefaultOpen)) {


							ImGui::DragFloat("MoveSpeed", &player->moveSpeed, 0.1f);

							ImGui::DragFloat("CameraDistance", &player->cameraDistance, 0.1f);

							ImGui::DragFloat("CameraLerp", &player->cameraLerp, 0.01f);

							ImGui::DragFloat("CameraRotate", &player->cameraRotate, 0.01f);


							ImGui::TreePop();
						}
					}
					ImGui::TreePop();
				}
				ImGui::End();
			}
		}
	}

	//ImGuiIO& io = ImGui::GetIO();
	//if(io.DisplaySize.x > 0){
	//	ImGuiStyle& style = ImGui::GetStyle();
	//	ImVec2 pad = style.DisplayWindowPadding;

	//	ImVec2 pos = ImGui::GetWindowPos();
	//	ImVec2 size = ImGui::GetWindowSize();
	//	ImVec2 newPos = pos;
	//	bool need = false;
	//	if(size.x > io.DisplaySize.x - pad.x * 2){
	//		size.x = io.DisplaySize.x - pad.x * 2;
	//		need = true;
	//	}
	//	if(size.y > io.DisplaySize.y - pad.y * 2){
	//		size.y = io.DisplaySize.y - pad.y * 2;
	//		need = true;
	//	}
	//	ImVec2 newSize = size;

	//	if(pos.x < pad.x){
	//		newPos.x = pad.x;
	//		need = true;
	//	}
	//	if(pos.y < pad.y){
	//		newPos.y = pad.y;
	//		need = true;
	//	}
	//	if(pos.x + size.x > io.DisplaySize.x - pad.x){
	//		newPos.x = io.DisplaySize.x - pad.x - size.x;
	//		need = true;
	//	}
	//	if(pos.y + size.y > io.DisplaySize.y - pad.y){
	//		newPos.y = io.DisplaySize.y - pad.y - size.y;
	//		need = true;
	//	}

	//	if(need){
	//		ImGui::SetWindowSize(newSize, ImGuiCond_Always);
	//		ImGui::SetWindowPos(newPos, ImGuiCond_Always);
	//	}
	//}


}
