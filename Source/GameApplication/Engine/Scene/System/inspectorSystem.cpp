// GameEngine/Source/GameApplication/Engine/Scene/System/inspectorSystem.cpp
#define _CRT_SECURE_NO_WARNINGS // for strncpy

#include "inspectorSystem.h"
#include "../engine.h"
#include "../engineContext.h"
#include "../Registry/entityRegistry.h"
#include "../../../Backends/ImGui/imgui.h"
#include "../../../Backends/ImGui/imgui_internal.h" // for DockSpace
#include "../../../Backends/ImGui/ImGuizmo.h"

#include <filesystem>
#include <set>
#include <Registry/componentRegistry.h>
#include <Component/transformComponent.h>
#include <Component/entityNameComponent.h>
#include <Component/meshRendererComponent.h>

#include "../scene.h"
#include "../sceneManager.h"
#include "Engine/DebugTools/ImGuiSystem.h"
#include "Engine/EditorUI/ImGuiMainManuBar.h"
#include <Component/textureComponent.h>
#include <Component/cameraComponent.h>
#include <Component/playerComponent.h>
#include <Component/enemyComponent.h>

// メインの更新関数
void InspectorSystem::Draw(){
	CreateDockSpace();

	showSceneHierarchy = &m_context->manager->imgui->GetManubar()->showSceneHierarchy;
	showInspector = &m_context->manager->imgui->GetManubar()->showInspector;
	showAssetsBrowser = &m_context->manager->imgui->GetManubar()->showAssetsBrowser;


	if(*showSceneHierarchy) DrawSceneHierarchy(m_context);
	if(*showInspector) DrawInspector(m_context);
	if(*showAssetsBrowser) DrawAssetsBrowser();
}

// ドッキングスペースを作成
void InspectorSystem::CreateDockSpace(){
	static bool dockspaceOpen = true;
	static ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_None;

	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("DockSpace", &dockspaceOpen, windowFlags);
	ImGui::PopStyleVar(3);

	ImGuiIO& io = ImGui::GetIO();
	if(io.ConfigFlags & ImGuiConfigFlags_DockingEnable){
		ImGuiID dockspaceId = ImGui::GetID("MyDockSpace");
		ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockspaceFlags);
	}
	ImGui::End();
}

// ヒエラルキーの再帰描画ヘルパー
void InspectorSystem::DrawEntityNode(ComponentRegistry* registry, Entity entity){
	auto nameComp = registry->GetComponent<NameComponent>(entity);
	auto transformComp = registry->GetComponent<TransformComponent>(entity);

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
	//if(transformComp.children.empty()){
	//	flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	//}
	if(entity == selectedEntity){
		flags |= ImGuiTreeNodeFlags_Selected;
	}

	// アイコンを名前の前に表示
	const char* icon = "📦"; // デフォルトアイコン

	bool node_open = ImGui::TreeNodeEx((void*)(uint64_t)entity, flags, "%s %s", icon, nameComp->name.c_str());

	if(ImGui::IsItemClicked()){
		selectedEntity = entity;
	}

	//if(node_open && !transformComp.children.empty()){
	//	for(auto child : transformComp.children){
	//		DrawEntityNode(registry, child);
	//	}
	//	ImGui::TreePop();
	//}
}


// シーンヒエラルキーウィンドウ
void InspectorSystem::DrawSceneHierarchy(SceneContext* context){
	ImGui::Begin("Scene Hierarchy", showSceneHierarchy);
	EntityRegistry* registry = context->entity;

	// ツールバー
	if(ImGui::Button("+ Add")){
		Entity newEntity = registry->Create(); // 新しいエンティティを追加
		selectedEntity = newEntity;
	}
	ImGui::SameLine();
	if(ImGui::Button("- Delete")){
		if(selectedEntity != 0){
			registry->Destroy(selectedEntity);
			context->component->OnEntityDestroyed(selectedEntity);
			selectedEntity = 0;
		}
	}
	static char searchBuffer[256] = "";
	ImGui::SetNextItemWidth(-1);
	ImGui::InputTextWithHint("##search", "Search objects...", searchBuffer, sizeof(searchBuffer));
	ImGui::Separator();

	// コンポーネントを持つエンティティの検索
	const auto& entities = m_context->entity->GetAllAlive();
	if(entities.empty()){
		return;
	} else{

		for(Entity entity : entities){
			std::string showName = "EntityID:" + std::to_string(entity);
			NameComponent* name = m_context->component->GetComponent<NameComponent>(entity);
			if(name && name->name != ""){
				showName = std::format("{:05}", entity) + " : " + name->name;
			}
			if(ImGui::Selectable(showName.c_str(), selectedEntity == entity)){
				if(selectedEntity == entity){
					selectedEntity = -1;
				} else{
					selectedEntity = entity;
				}
			}
		}
	}
	ImGui::End();

}


// インスペクターウィンドウ
void InspectorSystem::DrawInspector(SceneContext* context){
	ImGui::Begin("Inspector", showInspector);
	if(selectedEntity == 0){
		ImGui::Text("No object selected.");
		ImGui::End();
		return;
	}

	auto* registry = context->component;

	// オブジェクト名とアクティブ状態
	auto nameComp = registry->GetComponent<NameComponent>(selectedEntity);
	static bool objectActive = true; // TODO: link to actual component property
	ImGui::Checkbox("##active", &objectActive);
	ImGui::SameLine();


	NameComponent* name = m_context->component->GetComponent<NameComponent>(selectedEntity);
	if(name){
        if (name) {
            // Convert std::string to char buffer for ImGui::InputText
            static char nameBuffer[256];
            strncpy(nameBuffer, name->name.c_str(), sizeof(nameBuffer));
            nameBuffer[sizeof(nameBuffer) - 1] = '\0'; // Ensure null termination

            if (ImGui::InputText("##Name", nameBuffer, sizeof(nameBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
                // Update the std::string with the modified buffer
                name->name = nameBuffer;
            }
        }
	}
	ImGui::SameLine();
	if(ImGui::Button("- Delete")){
		if(selectedEntity != 0){
			context->entity->Destroy(selectedEntity);
			context->component->OnEntityDestroyed(selectedEntity);
			selectedEntity = 0;
		}
	}

	ImGui::Dummy(ImVec2(0, 10)); // 間隔を空ける

	auto Components = registry->GetAllComponentsOfEntity(selectedEntity);
	std::vector<IComponent*> componentsToRemove;
	// ツリーノードヘッダー用カラー
	ImVec4 headerBg = ImVec4(0.25f, 0.30f, 0.35f, 1.00f); // 通常時
	ImVec4 headerBgHover = ImVec4(0.35f, 0.45f, 0.55f, 1.00f); // ホバー時
	ImVec4 headerBgActive = ImVec4(0.30f, 0.40f, 0.50f, 1.00f); // 押下時
	ImVec4 headerText = ImVec4(0.90f, 0.90f, 0.90f, 1.00f); // テキスト
	auto& style = ImGui::GetStyle();
	auto drawList = ImGui::GetWindowDrawList();
	for(auto Component : Components){
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
			Component->inspector(m_context);
		}
		ImGui::Dummy(ImVec2(0, 2.5f)); // 間隔を空ける

	}

	// 削除は後からまとめて
	for(IComponent* comp : componentsToRemove){
		std::type_index ti(typeid(*comp));
		ComponentTypeID id = context->component->GetComponentIDByTypeIndex(ti);
		if(id != static_cast<ComponentTypeID>(-1)){
			context->component->RemoveComponentByID(selectedEntity, id);
		}
	}


	// コンポーネント追加ボタン
	ImGui::Separator();
	if(ImGui::Button("+ Add Component", ImVec2(-1, 0))){
		ImGui::OpenPopup("AddComponentPopup");
	}

	if(ImGui::BeginPopup("AddComponentPopup")){
		const auto& addableList = registry->GetAddableComponentList();

		// 対象エンティティのマスクを取得
		const auto& entityMasks = registry->GetEntityMasks();
		auto it = entityMasks.find(selectedEntity);
		ComponentMask currentMask;
		if(it != entityMasks.end()){
			currentMask = it->second;
		}

		for(const auto& [name, func] : addableList){
			// name から typeid を取得して、すでに追加済みならスキップ
			// 補足: ComponentTypeID にアクセスするための補助が必要
			ComponentTypeID typeID = registry->GetComponentIDByName(name);
			if(currentMask.test(typeID)) continue;

			if(ImGui::MenuItem(name.c_str())){
				func(selectedEntity);
			}
		}
		ImGui::EndPopup();
	}
	TransformComponent* transform = registry->GetComponent<TransformComponent>(selectedEntity);

	if(transform){

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
	}

	ImGui::End();
}

void DrawDirectoryTree(const std::filesystem::path& directory, std::string& selectedPath){
	if(!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory))
		return;

	for(const auto& entry : std::filesystem::directory_iterator(directory)){
		const auto& path = entry.path();
		if(!entry.is_directory()) continue;
		if(!std::filesystem::exists(path)) continue;

		std::string name = path.filename().string();
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;

		bool opened = ImGui::TreeNodeEx(name.c_str(), flags);
		if(ImGui::IsItemClicked()){
			selectedPath = path.string(); // フォルダ選択
		}

		if(opened){
			DrawDirectoryTree(path, selectedPath); // 再帰的に描画
			ImGui::TreePop();
		}
	}
}void DrawAssetsInDirectory(const std::string& folderPath){
	std::filesystem::path path = folderPath;
	if(!std::filesystem::exists(path) || !std::filesystem::is_directory(path))
		return;

	float itemSize = 70.0f;
	float panelWidth = ImGui::GetContentRegionAvail().x;
	int columnsCount = static_cast<int>(panelWidth / (itemSize + 5));
	if(columnsCount < 1) columnsCount = 1;

	int index = 0;

	for(const auto& entry : std::filesystem::directory_iterator(path)){
		if(!entry.is_regular_file()) continue;

		std::string filename = entry.path().filename().string();

		if(index % columnsCount != 0)
			ImGui::SameLine();

		ImGui::PushID(index++);

		ImGui::BeginGroup(); // アイコンとテキストを1つのアイテムにまとめる

		ImGui::Button("ICON", ImVec2(itemSize, itemSize));

		// ファイル名の描画（ラベルの横幅を制限）
		ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + itemSize);
		ImGui::TextWrapped("%s", filename.c_str());
		ImGui::PopTextWrapPos();

		ImGui::EndGroup();

		ImGui::PopID();
	}
}
void InspectorSystem::DrawAssetsBrowser(){
	std::filesystem::path ASSETS_ROOT = "Asset"; // ルートディレクトリ

	static std::string selectedPath = ASSETS_ROOT.string(); // 選択中のパスを保持

	ImGui::Begin("Assets Browser", showAssetsBrowser);
	ImGui::Columns(2, "AssetColumns", true);
	ImGui::SetColumnWidth(0, 200);

	// 左コラム（フォルダツリー） Begin
	ImGui::BeginChild("LeftPane", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

	if(std::filesystem::exists(ASSETS_ROOT) && std::filesystem::is_directory(ASSETS_ROOT)){
		if(ImGui::TreeNodeEx("Assets", ImGuiTreeNodeFlags_DefaultOpen)){
			if(ImGui::IsItemClicked()){
				selectedPath = ASSETS_ROOT.string(); // ルートクリック時
			}
			DrawDirectoryTree(ASSETS_ROOT, selectedPath);
			ImGui::TreePop();
		}
	} else{
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "Assets directory not found!");
	}

	ImGui::EndChild(); // 左コラム End

	ImGui::NextColumn();

	// 右コラム（アセット一覧） Begin
	ImGui::BeginChild("RightPane", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

	ImGui::Text("Content: %s", selectedPath.c_str());
	ImGui::Separator();
	DrawAssetsInDirectory(selectedPath);

	ImGui::EndChild(); // 右コラム End

	ImGui::Columns(1);
	ImGui::End();
}