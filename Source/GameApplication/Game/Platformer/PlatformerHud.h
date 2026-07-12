#pragma once

#include "Engine/Scene/Component/CustomScriptComponent.h"
#include "Game/Platformer/PlatformerCharacterController.h"
#include "Game/Platformer/PlatformerGameManager.h"
#include "Game/Platformer/PlatformerSceneAccess.h"

#include <algorithm>
#include <cstdio>

class PlatformerHud : public CustomScriptComponent {
	BEGIN_REFLECT(PlatformerHud)
		REFLECT_FIELD(float, instructionDuration, 8.0f)
		REFLECT_FIELD(bool, showControls, true)

public:
	YAML::Node encode() override {
		YAML::Node node;
		ENCODE_FIELDS(node);
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override {
		DECODE_FIELDS(node);
		return true;
	}

	void inspector(SceneContext* context) override {
		ImGui::Text("Platformer HUD");
		INSPECTOR_FIELDS();
	}

	void OnStart() override {
		manager = PlatformerSceneAccess::FindFirst<PlatformerGameManager>(m_ref.GetScene());
		player = PlatformerSceneAccess::FindFirst<PlatformerCharacterController>(m_ref.GetScene());
		elapsed = 0.0f;
	}

	void OnUpdate(float dt) override {
		elapsed += (std::max)(0.0f, dt);
		if(!manager.IsValid()) manager = PlatformerSceneAccess::FindFirst<PlatformerGameManager>(m_ref.GetScene());
		if(!player.IsValid()) player = PlatformerSceneAccess::FindFirst<PlatformerCharacterController>(m_ref.GetScene());
	}

	void OnDraw() override {
		auto* game = manager.TryGet();
		auto* controller = player.TryGet();
		if(!game || !controller) return;

		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		const ImVec2 workPosition = viewport ? viewport->WorkPos : ImVec2(0.0f, 0.0f);
		const ImVec2 workSize = viewport ? viewport->WorkSize : ImGui::GetIO().DisplaySize;
		const ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoMove;

		ImGui::SetNextWindowBgAlpha(0.72f);
		ImGui::SetNextWindowPos(ImVec2(workPosition.x + 18.0f, workPosition.y + 18.0f), ImGuiCond_Always);
		if(ImGui::Begin("##PlatformerStatus", nullptr, flags)) {
			ImGui::Text("COINS  %02d / %02d", game->GetCollectedCoins(), game->GetCoinTotal());
			ImGui::SameLine(0.0f, 24.0f);
			ImGui::Text("HP  %d / %d", controller->GetHealth(), controller->GetMaxHealth());
			if(game->IsBossBattle()) {
				ImGui::Separator();
				ImGui::TextUnformatted("BOSS");
				const float fraction = game->GetBossMaxHealth() > 0
					? static_cast<float>(game->GetBossHealth()) / static_cast<float>(game->GetBossMaxHealth())
					: 0.0f;
				ImGui::ProgressBar(std::clamp(fraction, 0.0f, 1.0f), ImVec2(240.0f, 18.0f));
			}
		}
		ImGui::End();

		if(showControls && elapsed < instructionDuration && !game->IsCleared()) {
			ImGui::SetNextWindowBgAlpha(0.66f);
			ImGui::SetNextWindowPos(
				ImVec2(workPosition.x + workSize.x * 0.5f, workPosition.y + workSize.y - 24.0f),
				ImGuiCond_Always,
				ImVec2(0.5f, 1.0f));
			if(ImGui::Begin("##PlatformerControls", nullptr, flags)) {
				ImGui::TextUnformatted("WASD  MOVE    SPACE  JUMP");
				ImGui::TextUnformatted("KEEP RUNNING AND JUMP ON EACH LANDING FOR A TRIPLE JUMP");
				ImGui::TextUnformatted("PRESS JUMP JUST BEFORE OR AFTER TOUCHING A WALL TO WALL KICK");
			}
			ImGui::End();
		}

		if(game->IsCleared()) {
			ImGui::SetNextWindowBgAlpha(0.88f);
			ImGui::SetNextWindowPos(
				ImVec2(workPosition.x + workSize.x * 0.5f, workPosition.y + workSize.y * 0.45f),
				ImGuiCond_Always,
				ImVec2(0.5f, 0.5f));
			if(ImGui::Begin("##PlatformerClear", nullptr, flags)) {
				ImGui::SetWindowFontScale(1.55f);
				ImGui::TextUnformatted("COURSE CLEAR");
				ImGui::SetWindowFontScale(1.0f);
				ImGui::Separator();
				ImGui::Text("COINS  %d / %d", game->GetCollectedCoins(), game->GetCoinTotal());
				const int totalSeconds = static_cast<int>(game->GetRunTime());
				ImGui::Text("TIME   %02d:%02d", totalSeconds / 60, totalSeconds % 60);
				ImGui::Separator();
				ImGui::TextUnformatted(game->CanRestart() ? "PRESS R TO RESTART" : "RESULT LOCKED...");
			}
			ImGui::End();
		}
	}

private:
	ComponentRef<PlatformerGameManager> manager;
	ComponentRef<PlatformerCharacterController> player;
	float elapsed = 0.0f;
};
