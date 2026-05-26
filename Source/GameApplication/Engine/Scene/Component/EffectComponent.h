// =======================================================================
// 
// EffectComponent.h
// 
// =======================================================================
#pragma once
#include "Interface/IComponent.h"
#include "Backends/YAMLConverters.h"
#include "Resources/resourceService.h"
#include "sceneManager.h"
#include <memory>
#include <string>

#include "scene.h"

#include "Resources/Data/effectData.h"
#include "Backends/Effekseer/Effekseer.h"
#include "Backends/Effekseer/EffekseerRendererDX11.h"

// Effekseer エフェクトの再生を管理するコンポーネント
// EffectSystem によって毎フレーム Update が呼ばれ、
// PlayOnStart=true の場合はシーン再生開始時に自動再生する
class EffectComponent: public IComponent {
public:
	// --------------------
	// Resource（リソース）
	// --------------------
	std::shared_ptr<EffectData> effectData;  // ロード済みエフェクトデータ
	std::string FilePath;                       // エフェクトファイルのパス（YAML 保存用）

	// --------------------
	// Playback settings（再生設定）
	// --------------------
	bool Loop = false;        // ループ再生するか
	bool PlayOnStart = false; // シーン再生開始時に自動再生するか
	float Volume = 1.0f;      // エフェクトの音量

	float TimeScale = 1.0f;    // 再生速度倍率（1.0 = 通常速度）
	float MaxPlayTime = 0.0f;  // 最大再生時間（秒、0.0 = 無制限）

	// --------------------
	// Runtime state（実行時状態）
	// --------------------
	bool Playing = false;         // 現在再生中かどうか
	float CurrentPlayTime = 0.0f; // 現在の再生経過時間（秒）

	Effekseer::Handle handle= -1; // Effekseer 再生ハンドル（-1 = 未再生）

	// --------------------
	// Serialize（シリアライズ）
	// --------------------
	YAML::Node encode() override{
		YAML::Node node;
		node["FilePath"] = FilePath;
		node["Loop"] = Loop;
		node["Volume"] = Volume;
		node["PlayOnStart"] = PlayOnStart;
		node["TimeScale"] = TimeScale;
		node["MaxPlayTime"] = MaxPlayTime;
		return node;
	}

	bool decode(SceneContext*, const YAML::Node& node) override{
		if(!node.IsMap()) return false;
		if(node["FilePath"])     FilePath = node["FilePath"].as<std::string>();
		if(node["Loop"])         Loop = node["Loop"].as<bool>();
		if(node["Volume"])       Volume = node["Volume"].as<float>();
		if(node["PlayOnStart"])  PlayOnStart = node["PlayOnStart"].as<bool>();
		if(node["TimeScale"])    TimeScale = node["TimeScale"].as<float>();
		if(node["MaxPlayTime"])  MaxPlayTime = node["MaxPlayTime"].as<float>();
		return true;
	}

	// --------------------
	// Inspector
	// --------------------
	void inspector(SceneContext* context) override{
		ImGui::PushID(this);

		char buffer[256]{};
		strncpy_s(buffer, sizeof(buffer), FilePath.c_str(), _TRUNCATE);

		ImGui::Text("Effect");
		ImGui::SameLine(100);
		ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 24.0f);
		if(ImGui::InputText("##EffectPath", buffer, sizeof(buffer))){
			FilePath = buffer;
		}
		ImGui::PopItemWidth();
		if(ImGui::BeginDragDropTarget()){
			if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")){
				FilePath = (const char*)payload->Data;
				effectData= context->manager->resource->Load<EffectData>(FilePath);
			}
			ImGui::EndDragDropTarget();
		}
		ImGui::SameLine();
		if(ImGui::SmallButton("x")){
			FilePath.clear();
			m_EffectData.reset();
		}



		ImGui::UndoCheckbox("Loop", &Loop);
		ImGui::SameLine();
		ImGui::UndoCheckbox("Play On Start", &PlayOnStart);

		ImGui::Separator();

		ImGui::UndoSliderFloat("Time Scale", &TimeScale, 0.0f, 3.0f);
		ImGui::UndoDragFloat("Max Play Time (sec)", &MaxPlayTime,0.01f, 0.0f, 0.0f);

		if(ImGui::UndoSliderFloat("Current Time", &CurrentPlayTime,0.0f,MaxPlayTime)){
			SeekForEditor(context, CurrentPlayTime);
		}

		ImGui::PopID();
	}

	// --------------------
	// Play
	// --------------------
	bool Play(SceneContext* sceneContext){
		if(!m_EffectData && !FilePath.empty()){
			effectData= sceneContext->manager->resource->Load<EffectData>(FilePath);
		}

		if(!m_EffectData || !sceneContext->manager->graphics)
			return false;

		if(Playing)
			return false;

		auto manager = sceneContext->manager->graphics->GetEffectManager();

		handle= manager->Play(
			m_EffectData->effect,
			0.0f, 0.0f, 0.0f
		);

		manager->SetSpeed(handle, TimeScale);

		CurrentPlayTime = 0.0f;
		Playing = true;

		return true;
	}

	// --------------------
	// Update (毎フレーム呼ぶ)
	// --------------------
	void Update(SceneContext* sceneContext, float deltaTime){
		if(!Playing){
			if(CurrentPlayTime > 0.0f){
				if(MaxPlayTime <= 0.0f || CurrentPlayTime < MaxPlayTime){
					MaxPlayTime = CurrentPlayTime;
				}
			}
			if(Loop){
				Play(sceneContext);
			}
			return;
		}
		auto manager = sceneContext->manager->graphics->GetEffectManager();

		if(manager->Exists(handle)){

			manager->UpdateHandle(handle, deltaTime * 60.0f);

		}else{
			Playing = false;
			return;
		}

		// 再生速度反映
		manager->SetSpeed(handle, TimeScale);

		// エンジン側で時間管理
		CurrentPlayTime += deltaTime * TimeScale;

		if(MaxPlayTime > 0.0f && CurrentPlayTime >= MaxPlayTime){
			Stop(sceneContext);
		}
	}

	// --------------------
	// Stop
	// --------------------
	void Stop(SceneContext* sceneContext){
		auto manager = sceneContext->manager->graphics->GetEffectManager();

		if(Playing && manager->Exists(handle)){
			manager->StopEffect(handle);
		}

		handle= -1;
		Playing = false;
		CurrentPlayTime = 0.0f;
	}
	void SeekForEditor(SceneContext* ctx, float targetTimeSec){
		Stop(ctx);
		Play(ctx);

		auto manager = ctx->manager->graphics->GetEffectManager();

		const float fps = 60.0f;
		const int maxStep = static_cast<int>(targetTimeSec * fps);

		for(int i = 0; i < maxStep; ++i){
			manager->UpdateHandle(handle, 1.0f);
		}

		CurrentPlayTime = targetTimeSec;
	}

};
