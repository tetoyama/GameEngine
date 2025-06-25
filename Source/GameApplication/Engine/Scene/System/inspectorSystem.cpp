#include "inspectorSystem.h"
#include "Scene.h"
#include "sceneManager.h"

#include "Backends/ImGui/ImGui.h"
#include "Backends/ImGui/ImGuizmo.h"
#include "Backends/ImGui/imgui_stdlib.h"

#include "engine/resources/data/texturedata.h"

#include "Entity/Entity.h"
#include "Registry/componentRegistry.h"

#include "Component/transformComponent.h"
#include "Component/entityNameComponent.h"
#include "Component/cameraComponent.h"
#include "Component/playerComponent.h"

#include "Component/textureComponent.h"

#include "Engine/DebugTools/debugSystem.h"
#include "Engine/DebugTools/imGuiSystem.h"

void InspectorSystem::Initialize(){
	m_context->manager->debug->LOG_DEBUG("TransformSystemを初期化中...");
}

void InspectorSystem::Finalize(){

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
				if (ImGui::Selectable(showName.c_str(), SelectEntity == entity)) {
					if (SelectEntity == entity) {
						SelectEntity = -1;
					} else {
						SelectEntity = entity;
					}
				}
				//if (ImGui::IsItemFocused()) {
				//	SelectEntity = entity;
				//}
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
						if(texture->m_TextureData){
							ImGui::Image(
								(ImTextureID)texture->m_TextureData->pTexture.Get(),
								ImVec2(256.0f,256.0f)
							);
						}
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
					if(transform){
						ImGui::End();

						DirectX::XMMATRIX Rotation = DirectX::XMMatrixRotationRollPitchYaw(transform->rotation.x, transform->rotation.y, transform->rotation.z);
						DirectX::XMMATRIX Scale = DirectX::XMMatrixScaling(transform->scale.x, transform->scale.y, transform->scale.z);
						DirectX::XMMATRIX Translation = DirectX::XMMatrixTranslation(transform->position.x, transform->position.y, transform->position.z);

						DirectX::XMMATRIX World = Scale * Rotation * Translation;

						DirectX::XMMATRIX modelMatrix = m_context->manager->imgui->RenderGizmo(World);
						if(ImGuizmo::IsUsing()){
							// スケール、回転、並進を格納する変数
							DirectX::XMVECTOR scale, rotationQuat, translation;

							// 行列を分解
							DirectX::XMMatrixDecompose(&scale, &rotationQuat, &translation, modelMatrix);

							// XMVECTOR から XMFLOAT3 に変換
							DirectX::XMFLOAT3 scale3, translation3;
							DirectX::XMStoreFloat3(&scale3, scale);
							DirectX::XMStoreFloat3(&translation3, translation);

							DirectX::XMFLOAT3 axis3;
							// クォータニオンをオイラー角に変換（roll, pitch, yaw）
							float qw = DirectX::XMVectorGetW(rotationQuat);
							float qx = DirectX::XMVectorGetX(rotationQuat);
							float qy = DirectX::XMVectorGetY(rotationQuat);
							float qz = DirectX::XMVectorGetZ(rotationQuat);
							// clampで安全にasin
							auto safe_asin = [](float v) -> float{
								if(v > 1.0f) v = 1.0f;
								else if(v < -1.0f) v = -1.0f;
								return asinf(v);
								};
							float yaw = atan2f(2.0f * (qw * qz + qx * qy), 1.0f - 2.0f * (qy * qy + qz * qz));
							float pitch = safe_asin(2.0f * (qw * qy - qz * qx));
							float roll = atan2f(2.0f * (qw * qx + qy * qz), 1.0f - 2.0f * (qx * qx + qy * qy));

							float cosP = cosf(pitch);
							if(fabsf(cosP) < 1e-6f){

								float r11 = 1.0f - 2.0f * (qx * qx + qz * qz);
								float r20 = 2.0f * (qx * qz + qw * qy);
								roll = atan2f(-r20, r11);
								yaw = 0.0f;
							}
							if(roll >= DirectX::XM_PI){
								roll -= DirectX::XM_2PI;
							}
							if(yaw >= DirectX::XM_PI){
								yaw -= DirectX::XM_2PI;
							}
							axis3 = DirectX::XMFLOAT3(roll, pitch, yaw);


							transform->position = translation3;
							transform->rotation = axis3;
							transform->scale = scale3;
						}
						ImGui::Begin("Inspector");
					}
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
