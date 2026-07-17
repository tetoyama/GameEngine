// =======================================================================
//
// AgentOSPanel.cpp
//
// =======================================================================
#include "AgentOSPanel.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <initializer_list>
#include <sstream>
#include <string>

#include "Backends/ImGui/imgui.h"

#include "Editor/editorService.h"

#include "../Core/Json.h"
#include "../Service/AgentOSService.h"

namespace agentos {

namespace {

const ImVec4 kErrorColor(1.00f, 0.34f, 0.38f, 1.00f);
const ImVec4 kWarningColor(1.00f, 0.72f, 0.24f, 1.00f);
const ImVec4 kAccentColor(0.25f, 0.78f, 1.00f, 1.00f);
const ImVec4 kAccentSoftColor(0.16f, 0.45f, 0.62f, 1.00f);
const ImVec4 kSuccessColor(0.36f, 0.90f, 0.60f, 1.00f);
const ImVec4 kPendingColor(0.43f, 0.47f, 0.55f, 1.00f);
const ImVec4 kSkippedColor(0.52f, 0.48f, 0.64f, 1.00f);
const ImVec4 kMutedColor(0.56f, 0.61f, 0.69f, 1.00f);
const ImVec4 kUserColor(0.62f, 0.78f, 1.00f, 1.00f);
const ImVec4 kAssistantColor(0.55f, 0.95f, 0.73f, 1.00f);
const ImVec4 kCardColor(0.075f, 0.090f, 0.120f, 1.00f);
const ImVec4 kCardAltColor(0.095f, 0.112f, 0.148f, 1.00f);
const ImVec4 kBorderColor(0.20f, 0.24f, 0.31f, 1.00f);
const ImVec4 kCodeColor(0.83f, 0.86f, 0.91f, 1.00f);
const ImVec4 kCodeKeywordColor(0.50f, 0.72f, 1.00f, 1.00f);
const ImVec4 kCodeCommentColor(0.45f, 0.68f, 0.49f, 1.00f);
const ImVec4 kCodeLiteralColor(0.91f, 0.72f, 0.43f, 1.00f);

struct AgentPhase {
	const char* stage;
	const char* label;
	const char* caption;
};

constexpr std::array<AgentPhase, 7> kAgentPhases{{
	{"intake", "INTAKE", "Understand request"},
	{"plan", "PLANNER", "Build task graph"},
	{"retrieve", "WORKER", "Run engine tools"},
	{"reason", "REASON", "Build hypotheses"},
	{"critic", "CRITIC", "Verify evidence"},
	{"repair", "REPAIR", "Fill evidence gaps"},
	{"synthesize", "REPORT", "Generate response"},
}};

enum class PhaseState {
	Pending,
	Active,
	Complete,
	Skipped,
	Failed,
};

std::string LowerAscii(std::string text) {
	std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return text;
}

bool IsErrorStage(const AgentOSService::StateSnapshot& snapshot) {
	const std::string stage = LowerAscii(snapshot.stage);
	return !snapshot.errorMessage.empty() || stage == "error";
}

bool IsStoppedStage(const AgentOSService::StateSnapshot& snapshot) {
	const std::string stage = LowerAscii(snapshot.stage);
	return stage == "stopped" || stage == "cancelled";
}

bool IsCompletedStage(const AgentOSService::StateSnapshot& snapshot) {
	return LowerAscii(snapshot.stage) == "completed";
}

bool IsFastRouteStage(const AgentOSService::StateSnapshot& snapshot) {
	const std::string stage = LowerAscii(snapshot.stage);
	return stage == "direct" || stage == "direct_route" || stage == "generate_reply";
}

int ResolvePhaseIndex(const std::string& rawStage) {
	const std::string stage = LowerAscii(rawStage);
	for(std::size_t i = 0; i < kAgentPhases.size(); ++i) {
		if(stage == kAgentPhases[i].stage) return static_cast<int>(i);
	}
	if(stage == "reply" || stage == "generate_reply" || stage == "direct" || stage == "direct_route") {
		return static_cast<int>(kAgentPhases.size() - 1);
	}
	if(stage == "completed") return static_cast<int>(kAgentPhases.size());
	return -1;
}

const char* CurrentAgentLabel(const AgentOSService::StateSnapshot& snapshot) {
	const std::string stage = LowerAscii(snapshot.stage);
	if(stage == "starting") return "SESSION ROUTER";
	if(stage == "loading_model") return "MODEL RUNTIME";
	if(stage == "direct" || stage == "direct_route") return "DETERMINISTIC ROUTER";
	if(stage == "generate_reply" || stage == "reply") return "RESPONSE GENERATOR";
	if(stage == "completed") return "COMPLETE";
	if(stage == "stopped") return "STOPPED";
	if(stage == "cancelled") return "CANCELLED";
	if(stage == "error") return "ERROR";

	const int phaseIndex = ResolvePhaseIndex(stage);
	if(phaseIndex >= 0 && phaseIndex < static_cast<int>(kAgentPhases.size())) {
		return kAgentPhases[static_cast<std::size_t>(phaseIndex)].label;
	}
	return snapshot.running ? "AGENTOS" : "IDLE";
}

float SessionProgress(const AgentOSService::StateSnapshot& snapshot) {
	if(IsCompletedStage(snapshot)) return 1.0f;
	const int phaseIndex = ResolvePhaseIndex(snapshot.stage);
	if(phaseIndex >= 0) {
		return std::clamp(
			(static_cast<float>(phaseIndex) + 0.48f) / static_cast<float>(kAgentPhases.size()),
			0.0f,
			0.98f);
	}
	if(snapshot.running) return 0.035f;
	return 0.0f;
}

ImVec4 PhaseColor(PhaseState state, float pulse = 1.0f) {
	switch(state) {
	case PhaseState::Active:
		return ImVec4(kAccentColor.x, kAccentColor.y, kAccentColor.z, 0.72f + 0.28f * pulse);
	case PhaseState::Complete:
		return kSuccessColor;
	case PhaseState::Skipped:
		return kSkippedColor;
	case PhaseState::Failed:
		return kErrorColor;
	case PhaseState::Pending:
	default:
		return kPendingColor;
	}
}

const char* PhaseStatusText(PhaseState state) {
	switch(state) {
	case PhaseState::Active: return "ACTIVE";
	case PhaseState::Complete: return "DONE";
	case PhaseState::Skipped: return "SKIP";
	case PhaseState::Failed: return "FAILED";
	case PhaseState::Pending:
	default: return "WAIT";
	}
}

void DrawPill(const char* label, const ImVec4& color) {
	const ImVec2 textSize = ImGui::CalcTextSize(label);
	const ImVec2 pos = ImGui::GetCursorScreenPos();
	const ImVec2 size(textSize.x + 18.0f, textSize.y + 8.0f);
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	ImVec4 background = color;
	background.w = 0.18f;
	drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), ImGui::GetColorU32(background), 999.0f);
	drawList->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), ImGui::GetColorU32(color), 999.0f, 0, 1.0f);
	drawList->AddText(ImVec2(pos.x + 9.0f, pos.y + 4.0f), ImGui::GetColorU32(color), label);
	ImGui::Dummy(size);
}

void DrawSectionLabel(const char* title, const char* subtitle = nullptr) {
	ImGui::TextColored(kAccentColor, "%s", title);
	if(subtitle && subtitle[0] != '\0') {
		ImGui::SameLine();
		ImGui::TextDisabled("%s", subtitle);
	}
	ImGui::Separator();
}

void DrawMissionHeader(
	const AgentOSService::StateSnapshot& snapshot,
	const char* childId,
	bool compact) {

	const float height = compact ? 76.0f : 94.0f;
	ImGui::PushStyleColor(ImGuiCol_ChildBg, kCardColor);
	ImGui::PushStyleColor(ImGuiCol_Border, kBorderColor);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
	ImGui::BeginChild(childId, ImVec2(0.0f, height), true, ImGuiWindowFlags_NoScrollbar);

	ImGui::TextColored(kMutedColor, "AGENT OPERATING SYSTEM");
	ImGui::SameLine();

	const bool error = IsErrorStage(snapshot);
	const bool stopped = IsStoppedStage(snapshot);
	const bool complete = IsCompletedStage(snapshot);
	const ImVec4 statusColor = error
		? kErrorColor
		: (stopped ? kWarningColor : (complete ? kSuccessColor : (snapshot.running ? kAccentColor : kPendingColor)));
	const char* statusText = error
		? "ERROR"
		: (stopped ? "STOPPED" : (complete ? "COMPLETE" : (snapshot.running ? "RUNNING" : "IDLE")));

	const float pillWidth = ImGui::CalcTextSize(statusText).x + 18.0f;
	ImGui::SetCursorPosX((std::max)(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - pillWidth));
	DrawPill(statusText, statusColor);

	ImGui::Text("%s", CurrentAgentLabel(snapshot));
	ImGui::SameLine();
	ImGui::TextDisabled("/ %s", snapshot.stage.empty() ? "idle" : snapshot.stage.c_str());

	const float progress = SessionProgress(snapshot);
	const std::string progressText = std::to_string(static_cast<int>(progress * 100.0f)) + "%";
	ImGui::ProgressBar(progress, ImVec2(-1.0f, compact ? 7.0f : 9.0f), progressText.c_str());

	ImGui::EndChild();
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(2);
}

PhaseState ResolvePhaseState(
	const AgentOSService::StateSnapshot& snapshot,
	std::size_t phaseIndex) {

	if(IsCompletedStage(snapshot)) return PhaseState::Complete;
	if(IsErrorStage(snapshot)) {
		const int current = ResolvePhaseIndex(snapshot.stage);
		if(current >= 0 && phaseIndex == static_cast<std::size_t>(current)) return PhaseState::Failed;
	}

	const int current = ResolvePhaseIndex(snapshot.stage);
	if(IsFastRouteStage(snapshot)) {
		if(phaseIndex + 1 < kAgentPhases.size()) return PhaseState::Skipped;
		return snapshot.running ? PhaseState::Active : PhaseState::Complete;
	}

	if(current < 0) return PhaseState::Pending;
	if(phaseIndex < static_cast<std::size_t>(current)) return PhaseState::Complete;
	if(phaseIndex == static_cast<std::size_t>(current)) {
		return snapshot.running ? PhaseState::Active : PhaseState::Complete;
	}
	return PhaseState::Pending;
}

void DrawAgentPipeline(
	const AgentOSService::StateSnapshot& snapshot,
	const char* childId,
	bool compact) {

	const float cardWidth = compact ? 112.0f : 142.0f;
	const float cardHeight = compact ? 62.0f : 88.0f;
	const float gap = 12.0f;
	const float totalWidth = cardWidth * static_cast<float>(kAgentPhases.size()) +
		gap * static_cast<float>(kAgentPhases.size() - 1);
	const float childHeight = cardHeight + 22.0f;

	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.045f, 0.055f, 0.075f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_Border, kBorderColor);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
	ImGui::BeginChild(
		childId,
		ImVec2(0.0f, childHeight),
		true,
		ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	const ImVec2 origin = ImGui::GetCursorScreenPos();
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const float pulse = 0.5f + 0.5f * static_cast<float>(std::sin(ImGui::GetTime() * 4.2));

	for(std::size_t i = 0; i + 1 < kAgentPhases.size(); ++i) {
		const PhaseState leftState = ResolvePhaseState(snapshot, i);
		const ImVec4 lineColor = leftState == PhaseState::Complete ? kSuccessColor : kBorderColor;
		const float y = origin.y + cardHeight * 0.5f;
		const float x0 = origin.x + cardWidth * static_cast<float>(i + 1) + gap * static_cast<float>(i);
		const float x1 = x0 + gap;
		drawList->AddLine(ImVec2(x0, y), ImVec2(x1, y), ImGui::GetColorU32(lineColor), 2.0f);
	}

	for(std::size_t i = 0; i < kAgentPhases.size(); ++i) {
		const AgentPhase& phase = kAgentPhases[i];
		const PhaseState state = ResolvePhaseState(snapshot, i);
		const ImVec4 stateColor = PhaseColor(state, pulse);
		const float x = origin.x + (cardWidth + gap) * static_cast<float>(i);
		const ImVec2 cardMin(x, origin.y);
		const ImVec2 cardMax(x + cardWidth, origin.y + cardHeight);

		ImVec4 background = state == PhaseState::Active ? kCardAltColor : kCardColor;
		if(state == PhaseState::Skipped) background = ImVec4(0.085f, 0.075f, 0.115f, 1.0f);
		drawList->AddRectFilled(cardMin, cardMax, ImGui::GetColorU32(background), 8.0f);

		if(state == PhaseState::Active) {
			ImVec4 glow = stateColor;
			glow.w = 0.16f + pulse * 0.12f;
			drawList->AddRect(
				ImVec2(cardMin.x - 2.0f, cardMin.y - 2.0f),
				ImVec2(cardMax.x + 2.0f, cardMax.y + 2.0f),
				ImGui::GetColorU32(glow),
				10.0f,
				0,
				3.0f);
		}
		drawList->AddRect(cardMin, cardMax, ImGui::GetColorU32(stateColor), 8.0f, 0, 1.2f);

		const std::string number = (i + 1 < 10 ? "0" : "") + std::to_string(i + 1);
		drawList->AddText(ImVec2(cardMin.x + 10.0f, cardMin.y + 8.0f), ImGui::GetColorU32(kMutedColor), number.c_str());

		const ImVec2 statusSize = ImGui::CalcTextSize(PhaseStatusText(state));
		drawList->AddText(
			ImVec2(cardMax.x - statusSize.x - 10.0f, cardMin.y + 8.0f),
			ImGui::GetColorU32(stateColor),
			PhaseStatusText(state));

		drawList->AddText(ImVec2(cardMin.x + 10.0f, cardMin.y + 29.0f), ImGui::GetColorU32(ImVec4(0.90f, 0.94f, 1.0f, 1.0f)), phase.label);
		if(!compact) {
			drawList->AddText(ImVec2(cardMin.x + 10.0f, cardMin.y + 53.0f), ImGui::GetColorU32(kMutedColor), phase.caption);
		}
	}

	ImGui::Dummy(ImVec2(totalWidth, cardHeight));
	ImGui::EndChild();
	ImGui::PopStyleVar();
	ImGui::PopStyleColor(2);
}

std::string JsonValueText(const Json& object, const char* key) {
	if(!object.is_object() || !object.contains(key)) return {};
	const Json& value = object.at(key);
	if(value.is_string()) return value.get<std::string>();
	if(value.is_number_integer()) return std::to_string(value.get<std::int64_t>());
	if(value.is_number_unsigned()) return std::to_string(value.get<std::uint64_t>());
	return value.dump();
}

void DrawCurrentWorkCard(
	const AgentOSService::StateSnapshot& snapshot,
	const ImVec2& size) {

	ImGui::PushStyleColor(ImGuiCol_ChildBg, kCardColor);
	ImGui::PushStyleColor(ImGuiCol_Border, kBorderColor);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 9.0f);
	ImGui::BeginChild("##AgentOSCurrentWork", size, true);

	DrawSectionLabel("CURRENT WORK", "active agent and task");
	DrawPill(CurrentAgentLabel(snapshot), snapshot.running ? kAccentColor : kPendingColor);
	ImGui::Spacing();
	ImGui::TextDisabled("Stage");
	ImGui::TextWrapped("%s", snapshot.stage.empty() ? "idle" : snapshot.stage.c_str());

	const Json& detail = snapshot.progressDetail;
	const std::array<const char*, 4> keys{{"planTaskId", "taskId", "type", "round"}};
	bool displayedDetail = false;
	for(const char* key : keys) {
		const std::string value = JsonValueText(detail, key);
		if(value.empty()) continue;
		displayedDetail = true;
		ImGui::TextColored(kMutedColor, "%s", key);
		ImGui::SameLine(105.0f);
		ImGui::TextWrapped("%s", value.c_str());
	}

	if(!displayedDetail) {
		ImGui::Spacing();
		ImGui::TextDisabled(snapshot.running
			? "Waiting for structured task detail..."
			: "No active task.");
	}

	if(detail.is_object() && !detail.empty() && ImGui::TreeNode("Raw progress payload")) {
		ImGui::TextWrapped("%s", detail.dump(2).c_str());
		ImGui::TreePop();
	}

	ImGui::EndChild();
	ImGui::PopStyleVar();
	ImGui::PopStyleColor(2);
}

void DrawActivityCard(const Json& audit, const ImVec2& size) {
	std::size_t total = 0;
	std::size_t succeeded = 0;
	std::size_t failed = 0;
	std::string lastTool;
	std::string lastStatus;

	if(audit.is_array()) {
		total = audit.size();
		for(const Json& entry : audit) {
			const std::string status = LowerAscii(entry.value("status", std::string()));
			const std::string error = entry.value("error", std::string());
			if(!error.empty() || status.find("fail") != std::string::npos || status.find("denied") != std::string::npos) {
				++failed;
			} else {
				++succeeded;
			}
			lastTool = entry.value("tool", std::string());
			lastStatus = entry.value("status", std::string());
		}
	}

	ImGui::PushStyleColor(ImGuiCol_ChildBg, kCardColor);
	ImGui::PushStyleColor(ImGuiCol_Border, kBorderColor);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 9.0f);
	ImGui::BeginChild("##AgentOSActivity", size, true);

	DrawSectionLabel("TOOL ACTIVITY", "validated engine access");
	ImGui::TextDisabled("Commands");
	ImGui::SameLine(110.0f);
	ImGui::Text("%zu", total);

	ImGui::TextColored(kSuccessColor, "Succeeded");
	ImGui::SameLine(110.0f);
	ImGui::Text("%zu", succeeded);

	ImGui::TextColored(failed > 0 ? kErrorColor : kMutedColor, "Failed");
	ImGui::SameLine(110.0f);
	ImGui::Text("%zu", failed);

	ImGui::Spacing();
	ImGui::TextDisabled("Latest command");
	if(lastTool.empty()) {
		ImGui::TextDisabled("No tool calls yet.");
	} else {
		ImGui::TextWrapped("%s", lastTool.c_str());
		ImGui::TextColored(failed > 0 ? kWarningColor : kMutedColor, "%s", lastStatus.c_str());
	}

	ImGui::EndChild();
	ImGui::PopStyleVar();
	ImGui::PopStyleColor(2);
}

std::string CompactLabel(const std::string& text, std::size_t maxChars) {
	if(text.size() <= maxChars) return text;
	if(maxChars <= 3) return text.substr(0, maxChars);
	return text.substr(0, maxChars - 3) + "...";
}

std::string TrimLeft(std::string value) {
	value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char c) {
		return !std::isspace(c);
	}));
	return value;
}

bool StartsWithAny(const std::string& text, std::initializer_list<const char*> prefixes) {
	for(const char* prefix : prefixes) {
		if(text.rfind(prefix, 0) == 0) return true;
	}
	return false;
}

ImVec4 CodeLineColor(const std::string& line) {
	const std::string trimmed = TrimLeft(line);
	if(StartsWithAny(trimmed, {"//", "/*", "*", "#"})) return kCodeCommentColor;
	if(trimmed.find('"') != std::string::npos || trimmed.find('\'') != std::string::npos) {
		return kCodeLiteralColor;
	}
	if(StartsWithAny(trimmed, {
		"class ", "struct ", "namespace ", "template", "using ", "enum ",
		"if(", "if (", "for(", "for (", "while(", "while (", "switch(",
		"return ", "const ", "auto ", "void ", "bool ", "int ", "float "})) {
		return kCodeKeywordColor;
	}
	return kCodeColor;
}

void RenderPlainText(const std::string& text) {
	constexpr std::size_t kChunkSize = 4096;
	for(std::size_t offset = 0; offset < text.size(); offset += kChunkSize) {
		const std::size_t count = (std::min)(kChunkSize, text.size() - offset);
		ImGui::TextWrapped("%.*s", static_cast<int>(count), text.c_str() + offset);
	}
}

void RenderCodeBlock(const std::string& code, const std::string& language, int blockId) {
	ImGui::PushID(blockId);
	if(!language.empty()) ImGui::TextColored(kMutedColor, "%s", language.c_str());

	std::size_t lineCount = 1;
	for(char c : code) if(c == '\n') ++lineCount;
	const float codeHeight = (std::min)(180.0f,
		(std::max)(52.0f, static_cast<float>(lineCount) * ImGui::GetTextLineHeightWithSpacing() + 10.0f));

	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.045f, 0.052f, 0.070f, 1.0f));
	ImGui::BeginChild("Code", ImVec2(-1.0f, codeHeight), true, ImGuiWindowFlags_HorizontalScrollbar);
	std::istringstream stream(code);
	std::string line;
	while(std::getline(stream, line)) ImGui::TextColored(CodeLineColor(line), "%s", line.c_str());
	ImGui::EndChild();
	ImGui::PopStyleColor();

	if(ImGui::SmallButton("Copy")) ImGui::SetClipboardText(code.c_str());
	ImGui::SameLine();
	ImGui::BeginDisabled(true);
	ImGui::SmallButton("Apply");
	ImGui::EndDisabled();
	if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
		ImGui::SetTooltip("Apply requires the audited patch/rollback pipeline.");
	}
	ImGui::PopID();
}

void RenderRichText(const std::string& text, int messageId) {
	std::istringstream stream(text);
	std::string line;
	std::string plain;
	std::string code;
	std::string language;
	bool inCode = false;
	int blockId = 0;

	auto flushPlain = [&]() {
		if(plain.empty()) return;
		RenderPlainText(plain);
		plain.clear();
	};
	auto flushCode = [&]() {
		RenderCodeBlock(code, language, messageId * 100 + blockId++);
		code.clear();
		language.clear();
	};

	while(std::getline(stream, line)) {
		if(line.rfind("```", 0) == 0) {
			if(inCode) {
				flushCode();
				inCode = false;
			} else {
				flushPlain();
				language = line.substr(3);
				inCode = true;
			}
			continue;
		}
		std::string& target = inCode ? code : plain;
		if(!target.empty()) target += '\n';
		target += line;
	}
	if(inCode) flushCode(); else flushPlain();
}

std::string ProcessLabel(
	std::int64_t elapsedMillis,
	std::int64_t promptTokens,
	std::int64_t completionTokens) {
	std::ostringstream out;
	out.setf(std::ios::fixed);
	out.precision(1);
	out << "Process log (" << (static_cast<double>(elapsedMillis) / 1000.0)
		<< "s / " << (promptTokens + completionTokens) << " tk)";
	return out.str();
}

} // namespace

// --------------------------------------------
// Initialize
// --------------------------------------------
void AgentOSPanel::Initialize(EditorService* editor) {
	m_editor = editor;
	m_show = true;
	m_scrollToBottom = false;
	m_frameCounter = 0;
	m_lastLiveCompletionTokens = -1;
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
	if(m_service) {
		m_service->PumpMainThread(m_frameCounter++);
	}

	ImGui::SetNextWindowSize(ImVec2(380.0f, 680.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2(300.0f, 300.0f), ImVec2(1600.0f, 1600.0f));
	if(!ImGui::Begin("AgentOS // Mission Control", &m_show)) {
		ImGui::End();
		return;
	}

	if(!m_service) {
		ImGui::TextColored(kErrorColor, "AgentOSService is not attached to this panel.");
		ImGui::TextWrapped(
			"Docs/AgentOS/02_VS_Integration.md の手順に従い、"
			"EditorService::Initialize() の後で SetService() を呼び出してください。");
		ImGui::End();
		return;
	}

	DrawCompactHeader();
	if(ImGui::BeginTabBar("AgentOSTabs")) {
		if(ImGui::BeginTabItem("Chat")) {
			DrawChatTab();
			ImGui::EndTabItem();
		}
		if(ImGui::BeginTabItem("Flow")) {
			DrawFlowTab();
			ImGui::EndTabItem();
		}
		if(ImGui::BeginTabItem("Hypotheses")) {
			DrawHypothesesTab();
			ImGui::EndTabItem();
		}
		if(ImGui::BeginTabItem("Audit")) {
			DrawAuditTab();
			ImGui::EndTabItem();
		}
		if(ImGui::BeginTabItem("Status")) {
			DrawStatusTab();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	ImGui::End();
}

void AgentOSPanel::DrawCompactHeader() {
	const AgentOSService::StateSnapshot snapshot = m_service->GetSnapshot();
	const std::string modelLabel = CompactLabel(snapshot.modelName, 18);

	ImGui::PushStyleColor(ImGuiCol_ChildBg, kCardColor);
	ImGui::BeginChild("AgentOSCompactHeader", ImVec2(-1.0f, 34.0f), false,
		ImGuiWindowFlags_HorizontalScrollbar);
	ImGui::SetNextItemWidth(118.0f);
	if(ImGui::BeginCombo("##Model", modelLabel.c_str(), ImGuiComboFlags_HeightSmall)) {
		ImGui::Selectable(snapshot.modelName.c_str(), true);
		ImGui::EndCombo();
	}
	if(ImGui::IsItemHovered()) ImGui::SetTooltip("Model: %s", snapshot.modelName.c_str());
	ImGui::SameLine();
	ImGui::TextDisabled("DX11");
	if(ImGui::IsItemHovered()) ImGui::SetTooltip("Target: %s", snapshot.targetEnvironment.c_str());
	ImGui::SameLine();
	ImGui::Text("%lld tk", static_cast<long long>(
		snapshot.totalPromptTokens + snapshot.totalCompletionTokens));
	ImGui::SameLine();
	ImGui::TextDisabled("API $0");
	ImGui::SameLine();
	ImGui::Text("%.1f tk/s", snapshot.tokensPerSecond);
	ImGui::EndChild();
	ImGui::PopStyleColor();
}

// --------------------------------------------
// DrawChatTab
// --------------------------------------------
void AgentOSPanel::DrawChatTab() {
	const AgentOSService::StateSnapshot snapshot = m_service->GetSnapshot();
	if(snapshot.liveCompletionTokens != m_lastLiveCompletionTokens) {
		m_lastLiveCompletionTokens = snapshot.liveCompletionTokens;
		m_scrollToBottom = snapshot.running;
	}

	if(!snapshot.errorMessage.empty()) {
		ImGui::TextColored(kErrorColor, "%s", snapshot.errorMessage.c_str());
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5.0f, 6.0f));
	ImGui::BeginChild("AgentOSChatTimeline", ImVec2(-1.0f, -116.0f), false);
	for(std::size_t index = 0; index < snapshot.chatLog.size(); ++index) {
		const AgentOSService::ChatEntry& entry = snapshot.chatLog[index];
		const bool isUser = entry.role == "user";
		ImGui::PushID(static_cast<int>(index));
		ImGui::TextColored(isUser ? kUserColor : kAssistantColor,
			isUser ? "YOU" : "AGENTOS");

		if(!isUser && !entry.processLog.empty()) {
			const std::string label = ProcessLabel(
				entry.elapsedMillis, entry.promptTokens, entry.completionTokens);
			if(ImGui::TreeNodeEx("##Process", ImGuiTreeNodeFlags_SpanAvailWidth,
				"%s", label.c_str())) {
				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.055f, 0.063f, 0.083f, 1.0f));
				ImGui::BeginChild("ProcessBody", ImVec2(-1.0f, 110.0f), true);
				ImGui::PushStyleColor(ImGuiCol_Text, kMutedColor);
				RenderPlainText(entry.processLog);
				ImGui::PopStyleColor();
				ImGui::EndChild();
				ImGui::PopStyleColor();
				ImGui::TreePop();
			}
		}

		RenderRichText(entry.text, static_cast<int>(index) + 1);
		if(!isUser) {
			ImGui::TextDisabled("%lld tk  |  %.1fs",
				static_cast<long long>(entry.promptTokens + entry.completionTokens),
				static_cast<double>(entry.elapsedMillis) / 1000.0);
		}
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		ImGui::PopID();
	}

	if(snapshot.running) {
		ImGui::TextColored(kAssistantColor, "AGENTOS  -  %s",
			snapshot.stage.empty() ? "working" : snapshot.stage.c_str());
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.050f, 0.058f, 0.078f, 1.0f));
		ImGui::BeginChild("LiveProcess", ImVec2(-1.0f, 105.0f), true);
		ImGui::TextColored(kMutedColor, "LIVE PROCESS");
		if(!snapshot.liveThinking.empty()) {
			RenderPlainText(snapshot.liveThinking);
		} else if(!snapshot.sessionProcessLog.empty()) {
			RenderPlainText(snapshot.sessionProcessLog);
		} else {
			ImGui::TextDisabled("Preparing the next step...");
		}
		ImGui::EndChild();
		ImGui::PopStyleColor();

		if(!snapshot.liveResponse.empty()) {
			ImGui::TextColored(kMutedColor, "GENERATION PREVIEW");
			RenderRichText(snapshot.liveResponse, 900000);
		}
		ImGui::TextDisabled("%.1fs  |  %lld tk  |  %.1f tk/s",
			static_cast<double>(snapshot.liveElapsedMillis) / 1000.0,
			static_cast<long long>(snapshot.livePromptTokens + snapshot.liveCompletionTokens),
			snapshot.tokensPerSecond);
	}

	if(m_scrollToBottom) {
		ImGui::SetScrollHereY(1.0f);
		m_scrollToBottom = false;
	}
	ImGui::EndChild();
	ImGui::PopStyleVar();

	ImGui::Separator();
	const bool inputEvent = ImGui::InputTextMultiline(
		"##AgentOSInput",
		m_inputBuffer,
		sizeof(m_inputBuffer),
		ImVec2(-1.0f, 66.0f));
	const bool submitShortcut = inputEvent && ImGui::GetIO().KeyCtrl &&
		ImGui::IsKeyPressed(ImGuiKey_Enter, false);
	const bool hasInput = m_inputBuffer[0] != '\0';

	if(snapshot.running) {
		if(ImGui::Button("Stop", ImVec2(76.0f, 0.0f))) m_service->CancelCurrentRequest();
		ImGui::SameLine();
		ImGui::TextDisabled("Ctrl+Enter sends when idle");
	} else {
		ImGui::BeginDisabled(!hasInput);
		const bool sendClicked = ImGui::Button("Send", ImVec2(76.0f, 0.0f));
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::TextDisabled("Ctrl+Enter");
		if((sendClicked || submitShortcut) && hasInput) {
			m_service->SubmitRequest(std::string(m_inputBuffer));
			std::memset(m_inputBuffer, 0, sizeof(m_inputBuffer));
			m_scrollToBottom = true;
		}
	}

	ImGui::SameLine();
	if(ImGui::SmallButton("Copy all")) {
		std::string clip;
		for(const auto& entry : snapshot.chatLog) {
			clip += "[" + entry.role + "]\n" + entry.text + "\n\n";
		}
		ImGui::SetClipboardText(clip.c_str());
	}
}

// --------------------------------------------
// DrawFlowTab
// --------------------------------------------
void AgentOSPanel::DrawFlowTab() {
	const AgentOSService::StateSnapshot snapshot = m_service->GetSnapshot();
	const Json audit = m_service->GetAuditSnapshot();

	DrawMissionHeader(snapshot, "##AgentOSFlowHeader", false);
	ImGui::Spacing();
	DrawSectionLabel("AGENT PIPELINE", "live execution overview");
	DrawAgentPipeline(snapshot, "##AgentOSFlowPipeline", false);
	ImGui::Spacing();

	const ImVec2 available = ImGui::GetContentRegionAvail();
	const float gap = 8.0f;
	const float leftWidth = (std::max)(280.0f, (available.x - gap) * 0.62f);
	const float rightWidth = (std::max)(220.0f, available.x - gap - leftWidth);
	DrawCurrentWorkCard(snapshot, ImVec2(leftWidth, available.y));
	ImGui::SameLine(0.0f, gap);
	DrawActivityCard(audit, ImVec2(rightWidth, available.y));
}

// --------------------------------------------
// DrawHypothesesTab
// --------------------------------------------
void AgentOSPanel::DrawHypothesesTab() {
	const AgentOSService::StateSnapshot snapshot = m_service->GetSnapshot();
	DrawMissionHeader(snapshot, "##AgentOSHypothesisHeader", true);
	ImGui::Spacing();

	const Json& root = snapshot.lastHypotheses;
	const Json hypotheses = root.is_object() ? root.value("hypotheses", Json::array()) : Json::array();

	if(!hypotheses.is_array() || hypotheses.empty()) {
		ImGui::TextDisabled("No hypotheses yet. Reasoning results will appear here.");
		return;
	}

	for(std::size_t i = 0; i < hypotheses.size(); ++i) {
		const Json& hypothesis = hypotheses[i];
		const std::string text = hypothesis.value("text", std::string());
		const double confidence = std::clamp(hypothesis.value("confidence", 0.0), 0.0, 1.0);

		ImGui::PushID(static_cast<int>(i));
		ImGui::PushStyleColor(ImGuiCol_ChildBg, kCardColor);
		ImGui::PushStyleColor(ImGuiCol_Border, kBorderColor);
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
		ImGui::BeginChild("HypothesisCard", ImVec2(0.0f, 126.0f), true);

		ImGui::TextColored(kAccentColor, "HYPOTHESIS %02zu", i + 1);
		ImGui::TextWrapped("%s", text.c_str());
		ImGui::ProgressBar(static_cast<float>(confidence), ImVec2(-1.0f, 8.0f), "confidence");

		const Json supports = hypothesis.value("supports", Json::array());
		if(supports.is_array() && !supports.empty()) {
			std::string ids;
			for(const Json& evidenceId : supports) {
				if(!ids.empty()) ids += ", ";
				ids += evidenceId.is_number_integer()
					? std::to_string(evidenceId.get<std::int64_t>())
					: evidenceId.dump();
			}
			ImGui::TextDisabled("Evidence: %s", ids.c_str());
		}

		ImGui::EndChild();
		ImGui::PopStyleVar();
		ImGui::PopStyleColor(2);
		ImGui::PopID();
		ImGui::Spacing();
	}
}

// --------------------------------------------
// DrawAuditTab
// --------------------------------------------
void AgentOSPanel::DrawAuditTab() {
	const Json audit = m_service->GetAuditSnapshot();

	if(!audit.is_array() || audit.empty()) {
		ImGui::TextDisabled("No commands recorded yet.");
		return;
	}

	ImGui::BeginChild("AgentOSAuditLog", ImVec2(-1.0f, -1.0f), true);
	for(std::size_t i = 0; i < audit.size(); ++i) {
		const Json& entry = audit[i];
		const std::string tool = entry.value("tool", std::string());
		const std::string status = entry.value("status", std::string());
		const std::string issuer = entry.value("issuer", std::string());
		const std::string error = entry.value("error", std::string());
		const bool failed = !error.empty() || LowerAscii(status).find("fail") != std::string::npos;

		ImGui::PushID(static_cast<int>(i));
		DrawPill(status.empty() ? "UNKNOWN" : status.c_str(), failed ? kErrorColor : kSuccessColor);
		ImGui::SameLine();
		ImGui::Text("%s", tool.c_str());
		ImGui::SameLine();
		ImGui::TextDisabled("by %s", issuer.c_str());

		if(!error.empty()) {
			ImGui::TextColored(kErrorColor, "error: %s", error.c_str());
		}
		if(ImGui::TreeNode("Command detail")) {
			ImGui::TextWrapped("%s", entry.dump(2).c_str());
			ImGui::TreePop();
		}
		ImGui::Separator();
		ImGui::PopID();
	}
	ImGui::EndChild();
}

// --------------------------------------------
// DrawStatusTab
// --------------------------------------------
void AgentOSPanel::DrawStatusTab() {
	const AgentOSService::StateSnapshot snapshot = m_service->GetSnapshot();
	DrawMissionHeader(snapshot, "##AgentOSStatusHeader", false);
	ImGui::Spacing();

	if(ImGui::CollapsingHeader("Runtime state", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("Busy: %s", snapshot.running ? "true" : "false");
		ImGui::Text("Stage: %s", snapshot.stage.empty() ? "idle" : snapshot.stage.c_str());
		ImGui::Text("Active agent: %s", CurrentAgentLabel(snapshot));
		ImGui::Text("Model: %s", snapshot.modelName.c_str());
		ImGui::Text("Target: %s", snapshot.targetEnvironment.c_str());
		ImGui::Text("Tokens: %lld prompt / %lld completion",
			static_cast<long long>(snapshot.totalPromptTokens),
			static_cast<long long>(snapshot.totalCompletionTokens));
		ImGui::Text("Generation: %.1f tk/s", snapshot.tokensPerSecond);
	}

	if(!snapshot.errorMessage.empty()) {
		ImGui::TextColored(kErrorColor, "Error: %s", snapshot.errorMessage.c_str());
	} else {
		ImGui::TextDisabled("Error: none");
	}

	if(!snapshot.transcriptPath.empty()) {
		ImGui::TextWrapped("Transcript: %s", snapshot.transcriptPath.c_str());
		ImGui::SameLine();
		if(ImGui::SmallButton("Copy path")) {
			ImGui::SetClipboardText(snapshot.transcriptPath.c_str());
		}
	} else {
		ImGui::TextDisabled("Transcript: no session yet");
	}

	if(snapshot.progressDetail.is_object() && !snapshot.progressDetail.empty() &&
	   ImGui::CollapsingHeader("Progress payload")) {
		ImGui::TextWrapped("%s", snapshot.progressDetail.dump(2).c_str());
	}
}

} // namespace agentos
