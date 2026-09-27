// =======================================================================
//
// AgentOSPanel.h
//
// AgentOSのEditor UI。チャットだけでなく、回答生成中のAgentパイプライン、
// 現在のTask、Tool活動をMission Control形式で可視化する。
//
// 表示名は "B.R.A.I.N."（Built-in Recon Artificial Intelligence Navigator）。
// 旧 Editor/UI/BRAIN/ パネルの後継であり、ロゴ演出とMenuBar連動を引き継ぐ
// （Docs/AgentOS/04_Execution_Engine_Roadmap.md §5 / S0）。
//
// EditorServiceはAgentOSServiceへのポインタを持たないため、SetService()で
// 登録時に明示的に注入する（Docs/AgentOS/02_VS_Integration.md参照）。
//
// =======================================================================
#pragma once

#include <cctype>
#include <cstdint>
#include <memory>

#include "Editor/InterFace/IEditorUI.h"

class EditorService;
struct EditorDrawContext;
struct TextureData;

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
	// 表示フラグの実体を返す。MenuBarが取得できる間は MenuBar::showBRAIN を
	// 使い、Windowメニューから再表示できるようにする。取得できない場合
	// （MenuBar未登録など）は内部フラグにフォールバックする。
	bool* ResolveShowFlag();
	void DrawBackgroundLogo();
	void DrawCompactHeader();
	void DrawChatTab();
	void DrawFlowTab();
	void DrawHypothesesTab();
	void DrawAuditTab();
	void DrawStatusTab();

	// コード索引（RAG下層）の進捗表示と再構築ボタン。
	// バックグラウンド構築のため、UIで見えないと動作確認ができない。
	void DrawCodeIndexStatus();

	// 推論バックエンド（CPU / GPU）の状態表示と切り替え。
	// GPUバックエンドのDLLが無ければ切り替えても効果が無いため、
	// 検出状況を併せて表示して切り分けられるようにする。
	void DrawInferenceBackend();

	EditorService* m_editor = nullptr;
	AgentOSService* m_service = nullptr;

	// MenuBarが解決できない場合のみ使うフォールバック表示フラグ。
	bool m_show = true;

	// 旧BRAINパネルから引き継いだ背景ロゴ。
	std::shared_ptr<TextureData> m_logoTexture;

	char m_inputBuffer[4096]{};
	bool m_scrollToBottom = false;
	std::int64_t m_frameCounter = 0;
	std::int64_t m_lastLiveCompletionTokens = -1;
};

} // namespace agentos
