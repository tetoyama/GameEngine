#include "ImGuiFunc.h"
#include <ImGui/imgui.h>
#include <string>
#include <myVector3.h>

void ImGui::DragVec3(const char* label, Vector3& Vec3, bool readOnly){

	float labelWidth = 120.0f + ImGui::GetCursorPosX();
	int axisCount = 3;
	float spacing = ImGui::GetStyle().ItemSpacing.x;
	float totalRegion = ImGui::GetContentRegionAvail().x;
	float fieldRegion = totalRegion - labelWidth - spacing * 2 * (axisCount);
	float fieldWidth = fieldRegion / axisCount;

	ImVec4 colorX = ImVec4(0.7f, 0.4f, 0.4f, 0.3f);
	ImVec4 colorY = ImVec4(0.4f, 0.7f, 0.4f, 0.3f);
	ImVec4 colorZ = ImVec4(0.4f, 0.4f, 0.7f, 0.3f);

	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(label);
	ImGui::SameLine();
	ImGui::SetCursorPosX(labelWidth); // 確実にラベル後ろに揃える

	auto DrawComponent = [&](const char* id, float& value, const ImVec4& borderColor, const char* uniqueId, bool isLast){
		ImGui::PushID(uniqueId);
		ImGui::PushStyleColor(ImGuiCol_Border, borderColor);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
		ImGui::PushItemWidth(fieldWidth - 10.0f);

		// 軸名（X/Y/Z）
		ImGui::Text("%s", id);
		ImGui::SameLine(0.0f, 10.0f);

		ImGui::DragFloat("##", &value, 0.01f, -1000.0f, 1000.0f);

		ImGui::PopItemWidth();
		ImGui::PopStyleVar();
		ImGui::PopStyleColor();
		ImGui::PopID();

		if(!isLast) ImGui::SameLine();
		};

	DrawComponent("X", Vec3.x, colorX, (std::string(label) + "X").c_str(), false);
	DrawComponent("Y", Vec3.y, colorY, (std::string(label) + "Y").c_str(), false);
	DrawComponent("Z", Vec3.z, colorZ, (std::string(label) + "Z").c_str(), true);

}
