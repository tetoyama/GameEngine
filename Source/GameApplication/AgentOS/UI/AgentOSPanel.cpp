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
	{"intake", "INTAKE", "要求と会話文脈を整理"},
	{"plan", "PLANNER", "実行するTask DAGを構成"},
	{"retrieve", "WORKER", "Engine ToolからEvidenceを取得"},
	{"reason", "REASON", "Evidenceから仮説を統合"},
	{"critic", "CRITIC", "根拠とCoverageを検証"},
	{"repair", "REPAIR", "不足Evidenceを再調査"},
	{"synthesize", "REPORT", "最終応答を生成"},
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

bool StringContains(const std::string& text, const char* needle) {
	return LowerAscii(text).find(LowerAscii(needle)) != std::string::npos;
}

bool JsonContainsRouteMarker(const Json& value) {
	if(value.is_string()) {
		const std::string text = LowerAscii(value.get<std::string>());
		return text.find("direct") != std::string::npos ||
			text.find("deterministic") != std::string::npos ||
			text.find("fast_path") != std::string::npos ||
			text.find("fastpath") != std::string::npos;
	}
	if(value.is_array()) {
		for(const Json& child : value) {
			if(JsonContainsRouteMarker(child)) return true;
		}
		return false;
	}
	if(value.is_object()) {
		for(const auto& item : value.items()) {
			if(JsonContainsRouteMarker(item.value())) return true;
		}
	}
	return false;
}

bool IsErrorStage(const AgentOSService::StateSnapshot& snapshot) {
	return !snapshot.errorMessage.empty() || LowerAscii(snapshot.stage) == "error";
}

bool IsStoppedStage(const AgentOSService::StateSnapshot& snapshot) {
	const std::string stage = LowerAscii(snapshot.stage);
	return stage == "stopped" || stage == "cancelled";
}

bool IsCompletedStage(const AgentOSService::StateSnapshot& snapshot) {
	return LowerAscii(snapshot.stage) == "completed";
}

bool IsFinalGenerationStage(const AgentOSService::StateSnapshot& snapshot) {
	const std::string stage = LowerAscii(snapshot.stage);
	return stage == "synthesize" || stage == "generate_reply" || stage == "reply";
}

bool IsFastRouteStage(const AgentOSService::StateSnapshot& snapshot) {
	const std::string stage = LowerAscii(snapshot.stage);
	return stage == "direct" || stage == "direct_route" || stage == "generate_reply" ||
		JsonContainsRouteMarker(snapshot.progressDetail);
}

int ResolvePhaseIndex(const std::string& rawStage) {
	const std::string stage = LowerAscii(rawStage);
	for(std::size_t i = 0; i < kAgentPhases.size(); ++i) {
		if(stage == kAgentPhases[i].stage) return static_cast<int>(i);
	}
	if(stage == "reply" || stage == "generate_reply" ||
		stage == "direct" || stage == "direct_route") {
		return static_cast<int>(kAgentPhases.size() - 1);
	}
	if(stage == "completed") return static_cast<int>(kAgentPhases.size());
	return -1;
}

const char* CurrentAgentLabel(const AgentOSService::StateSnapshot& snapshot) {
	const std::string stage = LowerAscii(snapshot.stage);
	if(stage == "starting") return "SESSION ROUTER";
	if(stage == "loading_model") return "MODEL RUNTIME";
	if(stage == "direct" || stage == "direct_route") return "ROUTER";
	if(stage == "generate_reply" || stage == "reply") return "REPORT";
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
	if(IsFastRouteStage(snapshot)) {
		const std::string stage = LowerAscii(snapshot.stage);
		if(stage == "direct" || stage == "direct_route") return 0.45f;
		if(stage == "generate_reply" || stage == "reply") return 0.86f;
	}
	const int phaseIndex = ResolvePhaseIndex(snapshot.stage);
	if(phaseIndex >= 0) {
		return std::clamp(
			(static_cast<float>(phaseIndex) + 0.48f) /
				static_cast<float>(kAgentPhases.size()),
			0.0f,
			0.98f);
	}
	return snapshot.running ? 0.05f : 0.0f;
}

PhaseState ResolvePhaseState(
	const AgentOSService::StateSnapshot& snapshot,
	std::size_t phaseIndex) {

	const bool fastRoute = IsFastRouteStage(snapshot);
	if(IsCompletedStage(snapshot)) {
		if(fastRoute && phaseIndex + 1 < kAgentPhases.size()) return PhaseState::Skipped;
		return PhaseState::Complete;
	}

	if(fastRoute) {
		if(phaseIndex + 1 < kAgentPhases.size()) return PhaseState::Skipped;
		return snapshot.running ? PhaseState::Active : PhaseState::Complete;
	}

	const int current = ResolvePhaseIndex(snapshot.stage);
	if(IsErrorStage(snapshot) && current >= 0 &&
		phaseIndex == static_cast<std::size_t>(current)) {
		return PhaseState::Failed;
	}
	if(current < 0) return PhaseState::Pending;
	if(phaseIndex < static_cast<std::size_t>(current)) return PhaseState::Complete;
	if(phaseIndex == static_cast<std::size_t>(current)) {
		return snapshot.running ? PhaseState::Active : PhaseState::Complete;
	}
	return PhaseState::Pending;
}

ImVec4 PhaseColor(PhaseState state, float pulse = 1.0f) {
	switch(state) {
	case PhaseState::Active:
		return ImVec4(kAccentColor.x, kAccentColor.y, kAccentColor.z,
			0.72f + 0.28f * pulse);
	case PhaseState::Complete: return kSuccessColor;
	case PhaseState::Skipped: return kSkippedColor;
	case PhaseState::Failed: return kErrorColor;
	case PhaseState::Pending:
	default: return kPendingColor;
	}
}

const char* PhaseStatusText(PhaseState state) {
	switch(state) {
	case PhaseState::Active: return "実行中";
	case PhaseState::Complete: return "完了";
	case PhaseState::Skipped: return "省略";
	case PhaseState::Failed: return "失敗";
	case PhaseState::Pending:
	default: return "待機";
	}
}

std::string JsonValueText(const Json& object, const char* key) {
	if(!object.is_object() || !object.contains(key)) return {};
	const Json& value = object.at(key);
	if(value.is_string()) return value.get<std::string>();
	if(value.is_number_integer()) return std::to_string(value.get<std::int64_t>());
	if(value.is_number_unsigned()) return std::to_string(value.get<std::uint64_t>());
	if(value.is_number_float()) return std::to_string(value.get<double>());
	return value.dump();
}

std::string FirstDetailValue(
	const Json& detail,
	std::initializer_list<const char*> keys) {
	for(const char* key : keys) {
		const std::string value = JsonValueText(detail, key);
		if(!value.empty()) return value;
	}
	return {};
}

std::string LatestToolName(const Json& audit) {
	if(!audit.is_array()) return {};
	for(auto it = audit.rbegin(); it != audit.rend(); ++it) {
		if(!it->is_object()) continue;
		const std::string tool = it->value("tool", std::string());
		if(!tool.empty()) return tool;
	}
	return {};
}

std::string CurrentOperationLabel(
	const AgentOSService::StateSnapshot& snapshot,
	const Json& audit) {

	std::string operation = FirstDetailValue(
		snapshot.progressDetail,
		{"tool", "toolName", "currentTool", "command", "operation"});
	if(operation.empty() && LowerAscii(snapshot.stage) == "retrieve") {
		operation = LatestToolName(audit);
	}
	if(!operation.empty()) return operation;

	const std::string stage = LowerAscii(snapshot.stage);
	if(stage == "loading_model") return "モデル読込";
	if(stage == "intake") return "会話文脈を解析";
	if(stage == "plan") return "Task DAGを構成";
	if(stage == "retrieve") return "Engine Toolを実行";
	if(stage == "reason") return "Evidenceを統合";
	if(stage == "critic") return "根拠を検証";
	if(stage == "repair") return "不足Evidenceを再調査";
	if(IsFinalGenerationStage(snapshot)) return "最終応答を生成";
	return {};
}

std::string CompactLabel(const std::string& text, std::size_t maxChars) {
	if(text.size() <= maxChars) return text;
	if(maxChars <= 3) return text.substr(0, maxChars);
	return text.substr(0, maxChars - 3) + "...";
}

std::string TruncateText(const std::string& text, std::size_t maxChars) {
	if(text.size() <= maxChars) return text;
	return text.substr(0, maxChars) + "\n...(truncated)...";
}

void DrawPhaseGlyph(
	ImDrawList* drawList,
	const ImVec2& center,
	PhaseState state,
	const ImVec4& color,
	float pulse) {

	const ImU32 colorU32 = ImGui::GetColorU32(color);
	if(state == PhaseState::Pending || state == PhaseState::Skipped) {
		drawList->AddCircle(center, 5.0f, colorU32, 16, 1.5f);
		if(state == PhaseState::Skipped) {
			drawList->AddCircleFilled(center, 2.0f, colorU32);
		}
		return;
	}

	const float radius = state == PhaseState::Active ? 4.5f + pulse : 5.0f;
	drawList->AddCircleFilled(center, radius, colorU32);
	if(state == PhaseState::Complete) {
		drawList->AddLine(
			ImVec2(center.x - 2.5f, center.y),
			ImVec2(center.x - 0.5f, center.y + 2.2f),
			IM_COL32(18, 31, 38, 255),
			1.3f);
		drawList->AddLine(
			ImVec2(center.x - 0.5f, center.y + 2.2f),
			ImVec2(center.x + 3.0f, center.y - 2.5f),
			IM_COL32(18, 31, 38, 255),
			1.3f);
	} else if(state == PhaseState::Failed) {
		drawList->AddLine(
			ImVec2(center.x - 2.2f, center.y - 2.2f),
			ImVec2(center.x + 2.2f, center.y + 2.2f),
			IM_COL32(28, 20, 25, 255),
			1.3f);
		drawList->AddLine(
			ImVec2(center.x + 2.2f, center.y - 2.2f),
			ImVec2(center.x - 2.2f, center.y + 2.2f),
			IM_COL32(28, 20, 25, 255),
			1.3f);
	}
}

void DrawAgentStatusRail(
	const AgentOSService::StateSnapshot& snapshot,
	const Json& audit,
	const char* id) {

	ImGui::PushID(id);
	const float pulse =
		0.5f + 0.5f * static_cast<float>(std::sin(ImGui::GetTime() * 4.2));
	const float width = (std::max)(170.0f, ImGui::GetContentRegionAvail().x);
	const float height = 38.0f;
	const float nodeWidth = 16.0f;
	const int activeIndex = ResolvePhaseIndex(snapshot.stage);
	const bool hasActive = snapshot.running && activeIndex >= 0 &&
		activeIndex < static_cast<int>(kAgentPhases.size());
	const float activeWidth = hasActive
		? std::clamp(width * 0.30f, 82.0f, 118.0f)
		: nodeWidth;
	const float itemWidths = hasActive
		? activeWidth + nodeWidth * static_cast<float>(kAgentPhases.size() - 1)
		: nodeWidth * static_cast<float>(kAgentPhases.size());
	const float gap = kAgentPhases.size() > 1
		? (std::max)(2.0f,
			(width - itemWidths) / static_cast<float>(kAgentPhases.size() - 1))
		: 0.0f;

	const ImVec2 origin = ImGui::GetCursorScreenPos();
	const float centerY = origin.y + height * 0.5f;
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddLine(
		ImVec2(origin.x + 8.0f, centerY),
		ImVec2(origin.x + width - 8.0f, centerY),
		ImGui::GetColorU32(kBorderColor),
		2.0f);

	float x = origin.x;
	for(std::size_t i = 0; i < kAgentPhases.size(); ++i) {
		const AgentPhase& phase = kAgentPhases[i];
		const PhaseState state = ResolvePhaseState(snapshot, i);
		const ImVec4 color = PhaseColor(state, pulse);
		const bool active = hasActive && static_cast<int>(i) == activeIndex;
		const float itemWidth = active ? activeWidth : nodeWidth;
		const ImVec2 itemMin(x, origin.y + 4.0f);
		const ImVec2 itemMax(x + itemWidth, origin.y + height - 4.0f);

		if(active) {
			ImVec4 background = color;
			background.w = 0.16f + 0.05f * pulse;
			drawList->AddRectFilled(
				itemMin,
				itemMax,
				ImGui::GetColorU32(background),
				999.0f);
			drawList->AddRect(
				itemMin,
				itemMax,
				ImGui::GetColorU32(color),
				999.0f,
				0,
				1.2f);
			const ImVec2 glyphCenter(itemMin.x + 12.0f, centerY);
			DrawPhaseGlyph(drawList, glyphCenter, state, color, pulse);
			drawList->AddText(
				ImVec2(itemMin.x + 23.0f, origin.y + 11.0f),
				ImGui::GetColorU32(ImVec4(0.93f, 0.97f, 1.0f, 1.0f)),
				phase.label);
		} else {
			DrawPhaseGlyph(
				drawList,
				ImVec2(itemMin.x + nodeWidth * 0.5f, centerY),
				state,
				color,
				pulse);
		}

		// Rail instance IDに加えてPhase固有IDを積み、各Hit Targetを一意にする。
		ImGui::PushID(phase.stage);
		ImGui::SetCursorScreenPos(itemMin);
		ImGui::InvisibleButton("##HitTarget", ImVec2(itemWidth, height - 8.0f));
		if(ImGui::IsItemHovered()) {
			ImGui::BeginTooltip();
			ImGui::TextColored(color, "%s", phase.label);
			ImGui::SameLine();
			ImGui::TextDisabled("%s", PhaseStatusText(state));
			ImGui::TextWrapped("%s", phase.caption);
			if(active) {
				const std::string operation = CurrentOperationLabel(snapshot, audit);
				if(!operation.empty()) {
					ImGui::Separator();
					ImGui::Text("現在: %s", operation.c_str());
				}
				if(snapshot.progressDetail.is_object() &&
					!snapshot.progressDetail.empty()) {
					ImGui::TextDisabled("%s", TruncateText(
						snapshot.progressDetail.dump(2), 700).c_str());
				}
			}
			ImGui::EndTooltip();
		}
		ImGui::PopID();

		x += itemWidth + gap;
	}

	ImGui::SetCursorScreenPos(origin);
	ImGui::Dummy(ImVec2(width, height));
	ImGui::PopID();
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
	const float codeHeight = (std::min)(
		180.0f,
		(std::max)(52.0f,
			static_cast<float>(lineCount) *
				ImGui::GetTextLineHeightWithSpacing() + 10.0f));

	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.045f, 0.052f, 0.070f, 1.0f));
	ImGui::BeginChild(
		"Code",
		ImVec2(-1.0f, codeHeight),
		true,
		ImGuiWindowFlags_HorizontalScrollbar);
	std::istringstream stream(code);
	std::string line;
	while(std::getline(stream, line)) {
		ImGui::TextColored(CodeLineColor(line), "%s", line.c_str());
	}
	ImGui::EndChild();
	ImGui::PopStyleColor();
	if(ImGui::SmallButton("Copy")) ImGui::SetClipboardText(code.c_str());
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
	if(inCode) flushCode();
	else flushPlain();
}

std::string DecodePartialJsonString(const std::string& text, const char* key) {
	const std::string marker = std::string("\"") + key + "\"";
	const std::size_t keyPos = text.find(marker);
	if(keyPos == std::string::npos) return {};
	const std::size_t colon = text.find(':', keyPos + marker.size());
	if(colon == std::string::npos) return {};
	const std::size_t quote = text.find('"', colon + 1);
	if(quote == std::string::npos) return {};

	std::string decoded;
	decoded.reserve(text.size() - quote);
	bool escaped = false;
	for(std::size_t i = quote + 1; i < text.size(); ++i) {
		const char c = text[i];
		if(escaped) {
			switch(c) {
			case 'n': decoded += '\n'; break;
			case 'r': break;
			case 't': decoded += '\t'; break;
			case '"': decoded += '"'; break;
			case '\\': decoded += '\\'; break;
			default:
				decoded += '\\';
				decoded += c;
				break;
			}
			escaped = false;
			continue;
		}
		if(c == '\\') {
			escaped = true;
			continue;
		}
		if(c == '"') break;
		decoded += c;
	}
	return decoded;
}

std::string LiveFinalResponse(const AgentOSService::StateSnapshot& snapshot) {
	if(!IsFinalGenerationStage(snapshot) || snapshot.liveResponse.empty()) return {};
	std::string report = DecodePartialJsonString(snapshot.liveResponse, "report");
	if(report.empty()) report = DecodePartialJsonString(snapshot.liveResponse, "reply");
	if(!report.empty()) return report;

	const std::string trimmed = TrimLeft(snapshot.liveResponse);
	if(trimmed.rfind("```", 0) == 0 || trimmed.rfind("{", 0) == 0) return {};
	return snapshot.liveResponse;
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

void DrawLiveResponseCard(
	const AgentOSService::StateSnapshot& snapshot,
	const Json& audit) {

	const std::string operation = CurrentOperationLabel(snapshot, audit);
	const std::string liveFinal = LiveFinalResponse(snapshot);
	const bool finalStage = IsFinalGenerationStage(snapshot);
	const int dotCount = static_cast<int>(ImGui::GetTime() * 2.5) % 4;
	std::string status = finalStage ? "最終応答を生成中" : "処理中";
	status.append(static_cast<std::size_t>(dotCount), '.');
	if(!operation.empty() && !finalStage) status += "  " + operation;

	const float availableWidth = (std::max)(180.0f, ImGui::GetContentRegionAvail().x - 18.0f);
	float previewHeight = 0.0f;
	if(finalStage) {
		if(liveFinal.empty()) {
			previewHeight = 38.0f;
		} else {
			previewHeight = std::clamp(
				ImGui::CalcTextSize(liveFinal.c_str(), nullptr, false, availableWidth).y + 34.0f,
				70.0f,
				300.0f);
		}
	}
	const std::string& processText = !snapshot.liveThinking.empty()
		? snapshot.liveThinking
		: snapshot.sessionProcessLog;
	const float logHeight = processText.empty() ? 0.0f : 24.0f;
	const float height = 104.0f + previewHeight + logHeight;

	ImGui::TextColored(kAssistantColor, "AGENTOS");
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.050f, 0.058f, 0.078f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_Border, kBorderColor);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
	ImGui::BeginChild("LiveAgentOSResponse", ImVec2(-1.0f, height), true);

	ImGui::TextColored(kAccentColor, "%s", status.c_str());
	ImGui::ProgressBar(SessionProgress(snapshot), ImVec2(-1.0f, 5.0f), "");
	DrawAgentStatusRail(snapshot, audit, "LiveAgentStatusRail");

	if(finalStage) {
		ImGui::Separator();
		if(liveFinal.empty()) {
			ImGui::TextDisabled("最初の出力トークンを待っています...");
		} else {
			RenderRichText(liveFinal, 900000);
			if(snapshot.generationActive) {
				ImGui::TextColored(kAccentColor, "| ");
			}
		}
	}

	if(!processText.empty() && ImGui::TreeNodeEx(
		"##LiveProcessLog",
		ImGuiTreeNodeFlags_SpanAvailWidth,
		"処理ログ")) {
		ImGui::PushStyleColor(ImGuiCol_Text, kMutedColor);
		RenderPlainText(processText);
		ImGui::PopStyleColor();
		ImGui::TreePop();
	}

	ImGui::EndChild();
	ImGui::PopStyleVar();
	ImGui::PopStyleColor(2);
	ImGui::TextDisabled(
		"%.1fs  |  %lld tk  |  %.1f tk/s",
		static_cast<double>(snapshot.liveElapsedMillis) / 1000.0,
		static_cast<long long>(
			snapshot.livePromptTokens + snapshot.liveCompletionTokens),
		snapshot.tokensPerSecond);
}

void DrawPill(const char* label, const ImVec4& color) {
	const ImVec2 textSize = ImGui::CalcTextSize(label);
	const ImVec2 pos = ImGui::GetCursorScreenPos();
	const ImVec2 size(textSize.x + 18.0f, textSize.y + 8.0f);
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	ImVec4 background = color;
	background.w = 0.18f;
	drawList->AddRectFilled(
		pos,
		ImVec2(pos.x + size.x, pos.y + size.y),
		ImGui::GetColorU32(background),
		999.0f);
	drawList->AddRect(
		pos,
		ImVec2(pos.x + size.x, pos.y + size.y),
		ImGui::GetColorU32(color),
		999.0f,
		0,
		1.0f);
	drawList->AddText(
		ImVec2(pos.x + 9.0f, pos.y + 4.0f),
		ImGui::GetColorU32(color),
		label);
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

void DrawMissionSummary(const AgentOSService::StateSnapshot& snapshot) {
	const bool error = IsErrorStage(snapshot);
	const bool stopped = IsStoppedStage(snapshot);
	const bool complete = IsCompletedStage(snapshot);
	const ImVec4 stateColor = error
		? kErrorColor
		: (stopped
			? kWarningColor
			: (complete
				? kSuccessColor
				: (snapshot.running ? kAccentColor : kPendingColor)));

	ImGui::PushStyleColor(ImGuiCol_ChildBg, kCardColor);
	ImGui::PushStyleColor(ImGuiCol_Border, kBorderColor);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
	ImGui::BeginChild("MissionSummary", ImVec2(-1.0f, 82.0f), true);
	DrawPill(CurrentAgentLabel(snapshot), stateColor);
	ImGui::SameLine();
	ImGui::TextDisabled("%s", snapshot.stage.empty() ? "idle" : snapshot.stage.c_str());
	ImGui::ProgressBar(SessionProgress(snapshot), ImVec2(-1.0f, 7.0f), "");
	const std::string task = FirstDetailValue(
		snapshot.progressDetail,
		{"description", "goal", "task", "planTaskId", "taskId"});
	if(!task.empty()) ImGui::TextWrapped("%s", task.c_str());
	else ImGui::TextDisabled(snapshot.running
		? "タスクを準備しています"
		: "実行中のタスクはありません");
	ImGui::EndChild();
	ImGui::PopStyleVar();
	ImGui::PopStyleColor(2);
}

void DrawToolActivity(const Json& audit) {
	DrawSectionLabel("TOOL ACTIVITY", "Engine access");
	if(!audit.is_array() || audit.empty()) {
		ImGui::TextDisabled("Tool実行はありません。");
		return;
	}
	const std::size_t begin = audit.size() > 8 ? audit.size() - 8 : 0;
	for(std::size_t i = begin; i < audit.size(); ++i) {
		const Json& entry = audit[i];
		const std::string tool = entry.value("tool", std::string("Unknown"));
		const std::string status = entry.value("status", std::string());
		const std::string error = entry.value("error", std::string());
		const bool failed = !error.empty() || StringContains(status, "fail") ||
			StringContains(status, "denied") || StringContains(status, "reject");
		ImGui::PushID(static_cast<int>(i));
		DrawPill(status.empty() ? (failed ? "FAILED" : "DONE") : status.c_str(),
			failed ? kErrorColor : kSuccessColor);
		ImGui::SameLine();
		ImGui::TextWrapped("%s", tool.c_str());
		if(!error.empty()) ImGui::TextColored(kErrorColor, "%s", error.c_str());
		ImGui::PopID();
	}
}

} // namespace

void AgentOSPanel::Initialize(EditorService* editor) {
	m_editor = editor;
	m_show = true;
	m_scrollToBottom = false;
	m_frameCounter = 0;
	m_lastLiveCompletionTokens = -1;
	std::memset(m_inputBuffer, 0, sizeof(m_inputBuffer));
}

void AgentOSPanel::Finalize() {
	m_editor = nullptr;
	m_service = nullptr;
}

void AgentOSPanel::Draw(const EditorDrawContext) {
	if(!m_show) return;
	if(m_service) m_service->PumpMainThread(m_frameCounter++);

	ImGui::SetNextWindowSize(ImVec2(380.0f, 680.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(
		ImVec2(300.0f, 300.0f),
		ImVec2(1600.0f, 1600.0f));
	if(!ImGui::Begin("AgentOS", &m_show)) {
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
	const std::string modelLabel = CompactLabel(
		snapshot.modelName.empty() ? "Local model" : snapshot.modelName,
		18);

	ImGui::PushStyleColor(ImGuiCol_ChildBg, kCardColor);
	ImGui::BeginChild(
		"AgentOSCompactHeader",
		ImVec2(-1.0f, 34.0f),
		false,
		ImGuiWindowFlags_HorizontalScrollbar);
	ImGui::SetNextItemWidth(118.0f);
	if(ImGui::BeginCombo("##Model", modelLabel.c_str(), ImGuiComboFlags_HeightSmall)) {
		ImGui::Selectable(
			snapshot.modelName.empty() ? "Local model" : snapshot.modelName.c_str(),
			true);
		ImGui::EndCombo();
	}
	if(ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Model: %s",
			snapshot.modelName.empty() ? "Local model" : snapshot.modelName.c_str());
	}
	ImGui::SameLine();
	ImGui::TextDisabled("DX11");
	ImGui::SameLine();
	ImGui::Text("%lld tk",
		static_cast<long long>(
			snapshot.totalPromptTokens + snapshot.totalCompletionTokens));
	ImGui::SameLine();
	ImGui::Text("%.1f tk/s", snapshot.tokensPerSecond);
	ImGui::EndChild();
	ImGui::PopStyleColor();
}

void AgentOSPanel::DrawChatTab() {
	const AgentOSService::StateSnapshot snapshot = m_service->GetSnapshot();
	const Json audit = m_service->GetAuditSnapshot();

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
				entry.elapsedMillis,
				entry.promptTokens,
				entry.completionTokens);
			if(ImGui::TreeNodeEx(
				"##Process",
				ImGuiTreeNodeFlags_SpanAvailWidth,
				"%s",
				label.c_str())) {
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

	if(snapshot.running) DrawLiveResponseCard(snapshot, audit);
	if(m_scrollToBottom) {
		ImGui::SetScrollHereY(1.0f);
		m_scrollToBottom = false;
	}
	ImGui::EndChild();
	ImGui::PopStyleVar();

	ImGui::Separator();
	ImGui::InputTextMultiline(
		"##AgentOSInput",
		m_inputBuffer,
		sizeof(m_inputBuffer),
		ImVec2(-1.0f, 66.0f));
	const bool submitShortcut = ImGui::IsItemFocused() &&
		ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Enter, false);
	const bool hasInput = m_inputBuffer[0] != '\0';

	if(snapshot.running) {
		if(ImGui::Button("Stop", ImVec2(76.0f, 0.0f))) {
			m_service->CancelCurrentRequest();
		}
		ImGui::SameLine();
		ImGui::TextDisabled("生成を停止");
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
		const std::string liveFinal = LiveFinalResponse(snapshot);
		if(!liveFinal.empty()) clip += "[assistant/streaming]\n" + liveFinal + "\n";
		ImGui::SetClipboardText(clip.c_str());
	}
}

void AgentOSPanel::DrawFlowTab() {
	const AgentOSService::StateSnapshot snapshot = m_service->GetSnapshot();
	const Json audit = m_service->GetAuditSnapshot();
	DrawMissionSummary(snapshot);
	ImGui::Spacing();
	DrawSectionLabel("エージェント状況", "hover for details");
	DrawAgentStatusRail(snapshot, audit, "FlowAgentStatusRail");
	ImGui::Spacing();
	DrawToolActivity(audit);
	if(snapshot.progressDetail.is_object() &&
		!snapshot.progressDetail.empty() &&
		ImGui::CollapsingHeader("Task detail")) {
		ImGui::TextWrapped("%s", snapshot.progressDetail.dump(2).c_str());
	}
}

void AgentOSPanel::DrawHypothesesTab() {
	const AgentOSService::StateSnapshot snapshot = m_service->GetSnapshot();
	const Json& root = snapshot.lastHypotheses;
	const Json hypotheses = root.is_object()
		? root.value("hypotheses", Json::array())
		: Json::array();
	if(!hypotheses.is_array() || hypotheses.empty()) {
		ImGui::TextDisabled("No hypotheses yet.");
		return;
	}

	for(std::size_t i = 0; i < hypotheses.size(); ++i) {
		const Json& hypothesis = hypotheses[i];
		std::string text = hypothesis.value("description", std::string());
		if(text.empty()) text = hypothesis.value("text", std::string());
		const double confidence = std::clamp(
			hypothesis.value("confidence", 0.0), 0.0, 1.0);
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
			ImGui::TextDisabled("Evidence: %s", supports.dump().c_str());
		}
		ImGui::EndChild();
		ImGui::PopStyleVar();
		ImGui::PopStyleColor(2);
		ImGui::PopID();
		ImGui::Spacing();
	}
}

void AgentOSPanel::DrawAuditTab() {
	const Json audit = m_service->GetAuditSnapshot();
	if(!audit.is_array() || audit.empty()) {
		ImGui::TextDisabled("No commands recorded yet.");
		return;
	}
	ImGui::BeginChild("AgentOSAuditList", ImVec2(-1.0f, -1.0f), false);
	for(std::size_t i = 0; i < audit.size(); ++i) {
		const Json& entry = audit[i];
		const std::string tool = entry.value("tool", std::string("Unknown"));
		const std::string issuer = entry.value("issuer", std::string());
		const std::string status = entry.value("status", std::string());
		const std::string error = entry.value("error", std::string());
		const bool failed = !error.empty() || StringContains(status, "fail") ||
			StringContains(status, "reject");
		ImGui::PushID(static_cast<int>(i));
		ImGui::TextColored(failed ? kErrorColor : kSuccessColor, "%s", tool.c_str());
		ImGui::SameLine();
		ImGui::TextDisabled("%s", status.c_str());
		if(!issuer.empty()) ImGui::TextDisabled("issuer: %s", issuer.c_str());
		if(entry.contains("arguments")) {
			ImGui::TextWrapped("args: %s", entry.at("arguments").dump().c_str());
		}
		if(!error.empty()) ImGui::TextColored(kErrorColor, "%s", error.c_str());
		if(ImGui::TreeNode("Payload")) {
			if(entry.contains("payload")) {
				ImGui::TextWrapped("%s", entry.at("payload").dump(2).c_str());
			}
			ImGui::TreePop();
		}
		ImGui::Separator();
		ImGui::PopID();
	}
	ImGui::EndChild();
}

void AgentOSPanel::DrawStatusTab() {
	const AgentOSService::StateSnapshot snapshot = m_service->GetSnapshot();
	ImGui::Text("Stage: %s", snapshot.stage.empty() ? "idle" : snapshot.stage.c_str());
	ImGui::Text("Running: %s", snapshot.running ? "true" : "false");
	ImGui::Text("Model: %s", snapshot.modelName.empty() ? "Local model" : snapshot.modelName.c_str());
	ImGui::TextWrapped("Target: %s", snapshot.targetEnvironment.c_str());
	ImGui::Text("Session: %.1fs", static_cast<double>(snapshot.sessionElapsedMillis) / 1000.0);
	ImGui::Text("Tokens: %lld prompt / %lld completion",
		static_cast<long long>(snapshot.sessionPromptTokens),
		static_cast<long long>(snapshot.sessionCompletionTokens));
	ImGui::Text("Speed: %.2f tk/s", snapshot.tokensPerSecond);
	ImGui::TextWrapped("Transcript: %s", snapshot.transcriptPath.c_str());
	if(!snapshot.errorMessage.empty()) {
		ImGui::TextColored(kErrorColor, "%s", snapshot.errorMessage.c_str());
	}
	if(snapshot.progressDetail.is_object() &&
		!snapshot.progressDetail.empty() &&
		ImGui::CollapsingHeader("Progress detail")) {
		ImGui::TextWrapped("%s", snapshot.progressDetail.dump(2).c_str());
	}
	if(!snapshot.lastReport.empty() && ImGui::CollapsingHeader("Last report")) {
		RenderRichText(snapshot.lastReport, 990000);
	}
}

} // namespace agentos
