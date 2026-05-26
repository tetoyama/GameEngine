#include "CB41.h"
#include "MenuBar.h"

#include "Backends/ImGui/imgui.h"

void CB41::Initialize(EditorService* editor) {
	editor = editor;
}

void CB41::Finalize() {}

void CB41::Draw(const EditorDrawContext ctx) {
	bool* show =
		&m_editor->GetUI<MenuBar>()->showCB41;
	if (!show || !*show) return;

	ImGui::Begin("CB41", show);
	ImGui::Text("Log:");

	float chatLogHeight = ImGui::GetContentRegionAvail().y - 48.0f;

	ImGui::BeginChild(
		"ChatLogChild",
		ImVec2(-1, chatLogHeight),
		true
	);

	ImGui::EndChild();
	ImGui::Separator();

	// -------------------
	// Prompt
	// -------------------
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
	ImGui::InputText(
		"##Prompt",
		m_InputBuffer,
		sizeof(m_InputBuffer)
	);

	ImGui::End();
}
