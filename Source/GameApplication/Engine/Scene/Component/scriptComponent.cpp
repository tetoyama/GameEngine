// =======================================================================
// 
// scriptComponent.cpp
// 
// =======================================================================
#include "scriptComponent.h"
#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "DebugTools/DebugSystem.h"
#include "Interface/IScriptComponent.h"
#include "Backends/ImGuiFunc.h"

// -----------------------------------------------------------------------
// ScriptComponent::decode
// YAML ノードからスクリプトを復元する
// ノードの各キーがスクリプト名、値がスクリプト固有のパラメーター
// -----------------------------------------------------------------------
bool ScriptComponent::decode(SceneContext* context, const YAML::Node& node){
	for(auto it : node){
		const std::string m_Name= it.first.as<std::string>();
		// ScriptSystem::Create でスクリプト名に対応するインスタンスを生成
		auto m_Script= context->manager->systemRegistry->GetSystem<ScriptSystem>()->Create(name.c_str());
		if(!script) continue;

		// スクリプト固有パラメーターを復元
		script->Decode(it.second);
		scripts[name] = std::move(script);
	}
	return m_True;
}

// -----------------------------------------------------------------------
// ScriptComponent::inspector
// インスペクター UI を描画する
// - スクリプト追加ボタンとテキスト入力
// - 各スクリプトのパラメーター（int / float / bool / string）を ImGui で編集
// -----------------------------------------------------------------------
void ScriptComponent::inspector(SceneContext* context) {
	ImGui::Text("Script Component");

	// スクリプト追加 UI
	static char m_ScriptNameInput[128] = {};
	ImGui::InputText("Add Script", scriptNameInput, sizeof(scriptNameInput));
	ImGui::SameLine();
	if (ImGui::Button("Add")) {
		AddScript(scriptNameInput, context);
	}

	// 各スクリプトのパラメーターを TreeNode で展開表示
	for (auto& [name, script] : scripts) {
		if (ImGui::TreeNode(name.c_str())) {

			auto& params = script->GetParams();
			for (auto& p : params) {
				// std::variant を型別に展開して対応する ImGui ウィジェットを表示
				std::visit([&](auto& val) {
					using T = std::decay_t<decltype(val)>;
					if constexpr (std::is_same_v<T, int>) {
						int m_Tmp= val;
						if (ImGui::InputInt(p.name.c_str(), &tmp)) {
							script->SetParam(p.name, tmp);
						}
					} else if constexpr (std::is_same_v<T, float>) {
						float m_Tmp= val;
						if (ImGui::InputFloat(p.name.c_str(), &tmp)) {
							script->SetParam(p.name, tmp);
						}
					} else if constexpr (std::is_same_v<T, bool>) {
						bool m_Tmp= val;
						if (ImGui::UndoCheckbox(p.name.c_str(), &tmp)) {
							script->SetParam(p.name, tmp);
						}
					} else if constexpr (std::is_same_v<T, std::string>) {
						char m_Buffer[256] = {};

						// string → char buffer にコピーして InputText で編集
						std::strncpy(buffer, val.c_str(), 256 - 1);

						if (ImGui::InputText(p.name.c_str(), buffer, 256)) {
							script->SetParam(p.name, std::string(buffer));
						}
					}
				}, p.value);
			}

			ImGui::TreePop();
		}
	}
}

// -----------------------------------------------------------------------
// ScriptComponent::AddScript
// 指定した名前のスクリプトをこのコンポーネントに追加する
// ScriptSystem::Create を通じてスクリプトインスタンスを生成する
// 引数:
//   scriptName - 追加するスクリプトのクラス名
//   context    - シーンコンテキスト（ScriptSystem の取得に使用）
// 戻り値: 追加に成功した場合 true、失敗した場合 false
// -----------------------------------------------------------------------
bool ScriptComponent::AddScript(const char* scriptName, SceneContext* context){
	// スクリプト名が空の場合は追加しない
	if(!scriptName || scriptName[0] == '\0'){
		context->manager->debug->LOG_DEBUG("Script name empty");
		return m_False;
	}

	// 既に同名のスクリプトが存在する場合は重複追加しない
	if(scripts.find(scriptName) != scripts.end()){
		context->manager->debug->LOG_DEBUG("Script already exists");
		return m_False;
	}

	// ScriptSystem を取得してスクリプトインスタンスを生成
	auto* scriptSystem = context->system->GetSystem<ScriptSystem>();
	if(!scriptSystem){
		context->manager->debug->LOG_DEBUG("ScriptSystem not found");
		return m_False;
	}

	IScriptComponent* raw = scriptSystem->Create(scriptName);
	if(!raw){
		context->manager->debug->LOG_DEBUG("Script create failed");
		return m_False;
	}

	IScriptComponent* script(raw);

	scripts.emplace(scriptName, std::move(script));
	return m_True;
}
