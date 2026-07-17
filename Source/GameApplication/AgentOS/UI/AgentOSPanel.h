// =======================================================================
//
// AgentOSPanel.h
//
// AgentOSのEditor UI。チャットだけでなく、回答生成中のAgentパイプライン、
// 現在のTask、Tool活動をMission Control形式で可視化する。
//
// EditorServiceはAgentOSServiceへのポインタを持たないため、SetService()で
// 登録時に明示的に注入する（Docs/AgentOS/02_VS_Integration.md参照）。
//
// =======================================================================
#pragma once

#include <cctype>
#include <cstdint>

#include "Editor/InterFace/IEditorUI.h"

class EditorService;
struct EditorDrawContext;

namespace agentos {

class AgentOSService;

class AgentOSPanel : public IEditorUI {
public:
	void Initialize(EditorService* editor) override;
	void Draw(const EditorDrawContext ctx) override;
	void Finalize() override;

	// EditorService::Initialize()の中ではAgentOSServiceを解決できない
	// （EditorServiceContextにAgentOSServiceが無いため）。
	// 登録側（lead側の統合コード）がEditorService::Initialize()の後に呼ぶ。
	void SetService(AgentOSService* service) { m_service = service; }

private:
	void DrawChatTab();
	void DrawFlowTab();
	void DrawHypothesesTab();
	void DrawAuditTab();
	void DrawStatusTab();

	EditorService* m_editor = nullptr;
	AgentOSService* m_service = nullptr;

	bool m_show = true;
	char m_inputBuffer[4096]{};
	bool m_scrollToBottom = false;
	std::int64_t m_frameCounter = 0;
};

} // namespace agentos
