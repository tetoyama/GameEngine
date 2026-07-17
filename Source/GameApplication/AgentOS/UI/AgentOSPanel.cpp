// =======================================================================
//
// AgentOSPanel.cpp
//
// =======================================================================
#include "AgentOSPanel.h"

#include <cstring>
#include <string>

#include "Backends/ImGui/imgui.h"

#include "Editor/editorService.h"

#include "../Core/Json.h"
#include "../Service/AgentOSService.h"

namespace agentos {

namespace {
	const ImVec4 kErrorColor(1.0f, 0.4f, 0.4f, 1.0f);
	const ImVec4 kUserColor(0.7f, 0.8f, 1.0f, 1.0f);
	const ImVec4 kAssistantColor(0.8f, 1.0f, 0.8f, 1.0f);
}

// --------------------------------------------
// Initialize
// --------------------------------------------
void AgentOSPanel::Initialize(EditorService* editor) {
	m_editor = editor;
	m_show = true;
	m_scrollToBottom = false;
	m_frameCounter = 0;
	std::memset(m_inputBuffer, 0, sizeof(m_inputBuffer));
}

// --------------------------------------------
// Finalize
// --------------------------------------------
void AgentOSPanel::Finalize() {
	m_editor = nullptr;
	m_service = nullptr;
}

// --------------------------------------------
// Draw
// --------------------------------------------
void AgentOSPanel::Draw(const EditorDrawContext) {
	if(!m_show) return;

	// MainThreadDispatcher::Pump() / WriteTracer::Sample() を毎フレーム進める。
	// AgentOSServiceが未登録でも安全なようにnullチェックする。
	if(m_service){
		m_service->PumpMainThread(m_frameCounter++);
	}

	if(!ImGui::Begin("AgentOS", &m_show)){
		ImGui::End();
		return;
	}

	if(!m_service){
		ImGui::TextColored(kErrorColor, "AgentOSService is not attached to this panel.");
		ImGui::TextWrapped(
			"Docs/AgentOS/02_VS_Integration.md の手順に従い、"
			"EditorService::Initialize() の後で SetService() を呼び出してください。");
		ImGui::End();
		return;
	}

	if(ImGui::BeginTabBar("AgentOSTabs")){
		if(ImGui::BeginTabItem("Chat")){
			DrawChatTab();
			ImGui::EndTabItem();
		}
		if(ImGui::BeginTabItem("Hypotheses")){
			DrawHypothesesTab();
			ImGui::EndTabItem();
		}
		if(ImGui::BeginTabItem("Audit")){
			DrawAuditTab();
			ImGui::EndTabItem();
		}
		if(ImGui::BeginTabItem("Status")){
			DrawStatusTab();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	ImGui::End();
}

// --------------------------------------------
// DrawChatTab
// --------------------------------------------
void AgentOSPanel::DrawChatTab() {
	const AgentOSService::StateSnapshot snapshot = m_service->GetSnapshot();

	ImGui::Text("Stage: %s", snapshot.stage.empty() ? "idle" : snapshot.stage.c_str());
	if(!snapshot.errorMessage.empty()){
		ImGui::TextColored(kErrorColor, "Error: %s", snapshot.errorMessage.c_str());
	}

	ImGui::BeginChild("AgentOSChatLog", ImVec2(-1, -140), true);
	for(const auto& [role, text] : snapshot.chatLog){
		const bool isUser = (role == "user");
		ImGui::PushStyleColor(ImGuiCol_Text, isUser ? kUserColor : kAssistantColor);
		ImGui::Text("%s:", role.c_str());
		ImGui::TextWrapped("%s", text.c_str());
		ImGui::PopStyleColor();
		ImGui::Separator();
	}
	if(m_scrollToBottom){
		ImGui::SetScrollHereY(1.0f);
		m_scrollToBottom = false;
	}
	ImGui::EndChild();

	ImGui::InputTextMultiline("##AgentOSInput", m_inputBuffer, sizeof(m_inputBuffer), ImVec2(-1, 80));

	const bool busy = m_service->IsBusy();
	ImGui::BeginDisabled(busy);
	if(ImGui::Button("Send")){
		const std::string request(m_inputBuffer);
		if(!request.empty()){
			m_service->SubmitRequest(request);
			std::memset(m_inputBuffer, 0, sizeof(m_inputBuffer));
			m_scrollToBottom = true;
		}
	}
	ImGui::EndDisabled();

	// チャット全文をクリップボードへコピーする（共有・不具合報告用）。
	ImGui::SameLine();
	if(ImGui::Button("Copy chat")){
		std::string clip;
		for(const auto& [role, text] : snapshot.chatLog){
			clip += "[" + role + "]\n" + text + "\n\n";
		}
		if(!snapshot.transcriptPath.empty()){
			clip += "(full transcript: " + snapshot.transcriptPath + ")\n";
		}
		ImGui::SetClipboardText(clip.c_str());
	}

	if(busy){
		ImGui::SameLine();
		ImGui::TextDisabled("Running...");
	}

	if(!snapshot.transcriptPath.empty()){
		ImGui::TextDisabled("Transcript: %s", snapshot.transcriptPath.c_str());
	}
}

// --------------------------------------------
// DrawHypothesesTab
// --------------------------------------------
void AgentOSPanel::DrawHypothesesTab() {
	const AgentOSService::StateSnapshot snapshot = m_service->GetSnapshot();

	// OrchestratorResult::rankedHypotheses は LogicGraph::ToJson() の形、
	// すなわち {"hypotheses": [{id, text, confidence, supports, contradicts, missingEvidence}, ...]}
	const Json& root = snapshot.lastHypotheses;
	const Json hypotheses = root.is_object() ? root.value("hypotheses", Json::array()) : Json::array();

	if(!hypotheses.is_array() || hypotheses.empty()){
		ImGui::TextDisabled("No hypotheses yet.");
		return;
	}

	for(const Json& hypothesis : hypotheses){
		const std::string text = hypothesis.value("text", std::string());
		const double confidence = hypothesis.value("confidence", 0.0);

		ImGui::Separator();
		ImGui::TextWrapped("%s", text.c_str());
		ImGui::Text("confidence: %.2f", confidence);

		const Json supports = hypothesis.value("supports", Json::array());
		if(supports.is_array() && !supports.empty()){
			ImGui::TextDisabled("supports (evidence id):");
			ImGui::SameLine();
			std::string ids;
			for(const Json& evidenceId : supports){
				if(!ids.empty()) ids += ", ";
				ids += evidenceId.is_number_integer()
					? std::to_string(evidenceId.get<std::int64_t>())
					: evidenceId.dump();
			}
			ImGui::TextWrapped("%s", ids.c_str());
		}
	}
}

// --------------------------------------------
// DrawAuditTab
// --------------------------------------------
void AgentOSPanel::DrawAuditTab() {
	const Json audit = m_service->GetAuditSnapshot();

	if(!audit.is_array() || audit.empty()){
		ImGui::TextDisabled("No commands recorded yet.");
		return;
	}

	ImGui::BeginChild("AgentOSAuditLog", ImVec2(-1, -1), true);
	for(const Json& entry : audit){
		const std::string tool = entry.value("tool", std::string());
		const std::string status = entry.value("status", std::string());
		const std::string issuer = entry.value("issuer", std::string());

		ImGui::Text("[%s] %s (issuer=%s)", status.c_str(), tool.c_str(), issuer.c_str());

		const std::string error = entry.value("error", std::string());
		if(!error.empty()){
			ImGui::TextColored(kErrorColor, "  error: %s", error.c_str());
		}
		ImGui::Separator();
	}
	ImGui::EndChild();
}

// --------------------------------------------
// DrawStatusTab
// --------------------------------------------
void AgentOSPanel::DrawStatusTab() {
	const AgentOSService::StateSnapshot snapshot = m_service->GetSnapshot();

	ImGui::Text("Busy: %s", snapshot.running ? "true" : "false");
	ImGui::Text("Stage: %s", snapshot.stage.empty() ? "idle" : snapshot.stage.c_str());

	if(!snapshot.errorMessage.empty()){
		ImGui::TextColored(kErrorColor, "Error: %s", snapshot.errorMessage.c_str());
	} else {
		ImGui::TextDisabled("Error: none");
	}

	if(!snapshot.transcriptPath.empty()){
		ImGui::Text("Transcript: %s", snapshot.transcriptPath.c_str());
		ImGui::SameLine();
		if(ImGui::SmallButton("Copy path")){
			ImGui::SetClipboardText(snapshot.transcriptPath.c_str());
		}
	} else {
		ImGui::TextDisabled("Transcript: (no session yet)");
	}

	if(snapshot.progressDetail.is_object() && !snapshot.progressDetail.empty()){
		ImGui::Text("Progress detail:");
		ImGui::TextWrapped("%s", snapshot.progressDetail.dump(2).c_str());
	}
}

} // namespace agentos
