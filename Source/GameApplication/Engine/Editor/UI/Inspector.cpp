// =======================================================================
// 
// Inspector.cpp
// 
// =======================================================================
#include "Inspector.h"
#include <ImGui/imgui_internal.h>
#include <cstring>
#include <memory>
#include <sceneManager.h>
#include "Editor/editorService.h"
#include "Editor/UI/MenuBar.h"
#include "Editor/Command/EntityCommand.h"
#include "Editor/Command/ComponentCommand.h"
#include <scene.h>
#include <Component/transformComponent.h>
#include <Component/entityNameComponent.h>
#include <Component/PrefabComponent.h>
#include "Hierarchy.h"

void Inspector::Draw(const EditorDrawContext ctx){

	ImGuiWindowClass window_class;
	window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
	ImGui::SetNextWindowClass(&window_class);
	bool* showInspector = &m_editor->GetUI<MenuBar>()->showInspector;
	Entity selectedEntity = m_editor->GetUI<Hierarchy>()->selectedEntity;
	SceneContext* context = m_editor->GetUI<Hierarchy>()->sceneContext;

	if(!showInspector || !*showInspector){
		return;
	}

	ImGui::Begin("Inspector", showInspector);

	if(selectedEntity == 0 || !context){
		ImGui::Text("No object selected.");
		ImGui::End();
		return;
	} else {
		bool alive = context->entity->IsAlive(selectedEntity); // 選択されたエンティティが生存しているか確認
		if(!alive){
			// ローカル変数だけでなく Hierarchy の selectedEntity も解除する
			m_editor->GetUI<Hierarchy>()->selectedEntity = 0;
			ImGui::End();

			return;
		}
	}

	auto* registry = context->component;

	// オブジェクト情報
	ImGui::AlignTextToFramePadding();
	ImGui::TextDisabled("ID: %u", selectedEntity);
	ImGui::SameLine();

	NameComponent* name = registry->GetComponent<NameComponent>(selectedEntity);
	if(name){
		// 名前編集用に std::string を ImGui の固定長バッファへ変換する
		static char nameBuffer[256];
		strncpy(nameBuffer, name->name.c_str(), sizeof(nameBuffer));
		nameBuffer[sizeof(nameBuffer) - 1] = '\0';

		if(ImGui::InputText("##Name", nameBuffer, sizeof(nameBuffer), ImGuiInputTextFlags_EnterReturnsTrue)){
			// Enter 確定時のみコマンド経由で名前を更新する
			std::string oldName = name->name;
			auto cmd = std::make_unique<RenameCommand>(context, selectedEntity, oldName, nameBuffer);
			m_editor->commandManager.Execute(std::move(cmd));
		}
		// PrefabComponent がある場合はエンティティが Prefab インスタンスであることを明示する
		if(context->component->GetComponent<PrefabComponent>(selectedEntity)){
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "(Prefab)");
		}
	}
	ImGui::SameLine();
	ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 60);

	if(ImGui::Button("- Delete")){
		if(selectedEntity != 0){
			Hierarchy* hierarchy = m_editor->GetUI<Hierarchy>();
			auto cmd = std::make_unique<EntityDeleteCommand>(
				context, selectedEntity,
				[hierarchy, selectedEntity](){
					if(hierarchy && hierarchy->selectedEntity == selectedEntity){
						hierarchy->selectedEntity = 0;
					}
				},
				[hierarchy](Entity e, SceneContext* ctx){
					if(hierarchy){
						hierarchy->selectedEntity = e;
						hierarchy->sceneContext   = ctx;
					}
				});
			m_editor->commandManager.Execute(std::move(cmd));
		}
	}

	ImGui::Dummy(ImVec2(0, 10)); // 間隔を空ける
	ImGui::BeginChild("Child", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

	auto components = registry->GetAllComponentsOfEntitySorted(selectedEntity);
	std::vector<IComponent*> componentsToRemove;
	auto drawList = ImGui::GetWindowDrawList();
	for(auto Component : components){
		std::string compName = typeid(*Component).name();

		// 必要なImGui情報
		ImVec2 cursorPos = ImGui::GetCursorScreenPos();
		float fullWidth = ImGui::GetContentRegionAvail().x;
		float frameHeight = ImGui::GetFrameHeight();

		// 背景色
		ImU32 bgColor = ImGui::GetColorU32(ImGuiCol_Header);
		drawList->AddRectFilled(cursorPos, ImVec2(cursorPos.x + fullWidth, cursorPos.y + frameHeight), bgColor);

		// ツリーノードの展開矢印だけ表示（幅だけ取るためにNoTreePushOnOpen）
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		bool open = ImGui::TreeNodeEx((void*)Component, flags, "%s", compName.c_str());

		// Removeボタンは同じ行の右端に配置
		ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 60);
		if(ImGui::SmallButton(("Remove##" + compName).c_str())){
			componentsToRemove.push_back(Component);
		}
		// 次の行に移動
		if(open){
			Component->inspector(context);
		}
		ImGui::Dummy(ImVec2(0, 2.5f)); // 間隔を空ける

	}

	// 削除は後からまとめてコマンドで実行
	for(IComponent* comp : componentsToRemove){
		auto cmd = std::make_unique<ComponentRemoveCommand>(context, selectedEntity, comp);
		m_editor->commandManager.Execute(std::move(cmd));
	}


	// コンポーネント追加ボタン
	ImGui::Separator();
	if(ImGui::Button("+ Add Component", ImVec2(-1, 0))){
		ImGui::OpenPopup("AddComponentPopup");
	}
	if(ImGui::BeginPopup("AddComponentPopup")){
		static char componentSearchBuffer[128] = "";
		ImGui::InputTextWithHint("##ComponentSearch", "Search component...", componentSearchBuffer, sizeof(componentSearchBuffer));
		ImGui::Separator();

		const auto& addableMap = registry->GetAddableComponentList();

		// --- 名前順に並べ替える ---
		std::vector<std::pair<std::string, std::function<void(Entity)>>> addableListSorted(
			addableMap.begin(), addableMap.end()
		);

		std::sort(addableListSorted.begin(), addableListSorted.end(),
				  [](const auto& a, const auto& b){
					  return a.first < b.first;
				  });

		// --- マスク確認用 ---
		const auto& entityMasks = registry->GetEntityMasks();
		auto it = entityMasks.find(selectedEntity);
		ComponentMask currentMask;
		if(it != entityMasks.end()){
			currentMask = it->second;
		}

		// --- 検索小文字化 ---
		std::string searchLower = componentSearchBuffer;
		std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

		// --- メニュー表示 ---
		for(const auto& [name, func] : addableListSorted){
			ComponentTypeID typeID = registry->GetComponentIDByName(name);
			if(currentMask.test(typeID)) continue;

			std::string nameLower = name;
			std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

			if(!searchLower.empty() && nameLower.find(searchLower) == std::string::npos){
				continue;
			}

			if(ImGui::MenuItem(name.c_str())){
				auto cmd = std::make_unique<ComponentAddCommand>(context, selectedEntity, name, func);
				m_editor->commandManager.Execute(std::move(cmd));
			}
		}

		ImGui::EndPopup();
	}



	TransformComponent* transform = registry->GetComponent<TransformComponent>(selectedEntity);

	//if(transform && m_context->editor->GetUI<MenuBar>()->showEditorView){

	//	DirectX::XMMATRIX World = transform->CalculateWorldMatrix(transform, context->component);

	//	DirectX::XMMATRIX modelMatrix;

	//	auto* sprite = registry->GetComponent<SpriteRendererComponent>(selectedEntity);

	//	if(sprite){
	//		TransformComponent temp = CalculateRectTransform(*sprite, *transform);
	//		DirectX::XMMATRIX Rotation = DirectX::XMMatrixRotationRollPitchYaw(temp.GetRotationEuler().x, temp.GetRotationEuler().y, temp.GetRotationEuler().z);
	//		DirectX::XMMATRIX Scale = DirectX::XMMatrixScaling(temp.scale.x, temp.scale.y, temp.scale.z);
	//		DirectX::XMMATRIX Translation = DirectX::XMMatrixTranslation(temp.position.x, temp.position.y, temp.position.z);

	//		World = Scale * Rotation * Translation;

	//		modelMatrix = m_context->imgui->RenderGizmo2D(World);

	//	} else{
	//		modelMatrix = m_context->imgui->RenderGizmo(World);
	//	}
	//	Entity Parent = transform->parent;
	//	while(Parent != 0){
	//		auto* ParentTransform = registry->GetComponent<TransformComponent>(Parent);
	//		if(ParentTransform){

	//			DirectX::XMMATRIX ParentWorld = ParentTransform->CalculateWorldMatrix(ParentTransform, context->component);

	//			modelMatrix = modelMatrix * DirectX::XMMatrixInverse(nullptr, ParentWorld);

	//			Parent = ParentTransform->parent;
	//		} else{
	//			Parent = 0;
	//		}
	//	}
	//	if(ImGuizmo::IsUsing()){
	//		// スケール、回転、並進を格納する変数
	//		DirectX::XMVECTOR scale, rotationQuat, translation;

	//		// 行列を分解
	//		DirectX::XMMatrixDecompose(&scale, &rotationQuat, &translation, modelMatrix);

	//		// XMVECTOR から XMFLOAT3 に変換
	//		DirectX::XMFLOAT3 scale3, translation3;
	//		DirectX::XMStoreFloat3(&scale3, scale);
	//		DirectX::XMStoreFloat3(&translation3, translation);

	//		// rotationQuat はクォータニオンなのでそのまま XMFLOAT4 に書き出し
	//		DirectX::XMFLOAT4 quat;
	//		DirectX::XMStoreFloat4(&quat, rotationQuat);

	//		if(sprite){
	//			TransformComponent edited;
	//			edited.position = translation3;
	//			edited.SetRotation(quat);     // クォータニオンを代入
	//			edited.scale = scale3;

	//			edited = ReverseCalculateRectTransform(*sprite, edited);

	//			transform->position = edited.position;
	//			transform->SetRotation(edited.GetRotation());
	//			transform->scale = edited.scale;
	//		} else{
	//			transform->position = translation3;
	//			transform->SetRotation(quat); // クォータニオンを保持
	//			transform->scale = scale3;
	//		}


	//	}
	//}
	ImGui::EndChild();
	ImGui::End();
}
