// =======================================================================
// 
// BRAIN.h
// 
// =======================================================================
#pragma once

#include <vector>
#include <string>
#include <queue>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <wrl/client.h>
#include <d3d11.h>

#include "../../InterFace/IEditorUI.h"

#define BRAIN_MODEL_PATH "Asset/BRAIN/model/Qwen3.5-9B-Q4_K_M.gguf"


// ---------------------------------
// Forward declarations
// ---------------------------------
class EditorService;
class LLAMAService;
class LLAMAAgent;

struct EditorDrawContext;
struct TextureData;
struct LLAMAModelData;
struct AgentConfig;

// ---------------------------------
// Chat log entry
// ---------------------------------
struct ChatEntry {
	enum class Role {
		User,
		Assistant
	};

	Role role;
	std::string text;
};

// ---------------------------------
// LLM Job
// ---------------------------------
struct LLMJob {
	std::string prompt;
};

// ---------------------------------
// BRAIN UI
// ---------------------------------
class BRAIN: public IEditorUI {

public:
	void Initialize(EditorService* editor) override;
	void Finalize() override;
	void Draw(const EditorDrawContext ctx) override;

private:
	// -----------------------------
	// Editor
	// -----------------------------
	EditorService* m_editor = nullptr;

	// -----------------------------
	// Logo
	// -----------------------------
	std::shared_ptr<TextureData> logoTexture;

	// -----------------------------
	// Chat / Output
	// -----------------------------
	std::vector<ChatEntry> m_chatLog;
	std::string m_asyncOutput;

	mutable std::mutex m_outputMutex;

	// -----------------------------
	// Job queue
	// -----------------------------
	std::queue<LLMJob> m_jobQueue;
	std::mutex m_jobMutex;
	std::condition_variable m_jobCV;

	// ----------------------------
	// ワーカースレッド
	// ----------------------------
	std::thread m_workerThread;
	void WorkerLoop();

	// -----------------------------
	// Runtime state
	// -----------------------------
	std::atomic<bool> m_isRunning{false};
	std::atomic<bool> m_stopRequested{false};
	std::atomic<bool> m_exitRequested{false};
	std::atomic<bool> m_requestReset{false};

	// -------------------------
	// LLM
	// -------------------------
	std::shared_ptr<LLAMAModelData> m_llamaModel;
	std::shared_ptr<AgentConfig>    m_agentConfig;
	std::shared_ptr<LLAMAAgent>     m_mainAgent;

	// -------------------------
	// UI state
	// -------------------------
	char inputBuffer[2048]{};
	bool m_scrollToBottom = false;

	// -----------------------------
	// Token state (表示用)
	// -----------------------------
	uint32_t m_nPast = 0;

	// [修正] 以前は MAX_CONTEXT_TOKEN という名前なのに実際は max_tokens（1回の応答の
	// 生成上限）に使われていて、n_ctx 自体は別の場所でハードコード(8192)されているという
	// 名前と実態が食い違った状態だった。役割ごとに名前を分けて、値も
	// 「応答上限 < コンテキストサイズ」の関係を満たすようにした。
	static constexpr uint32_t CONTEXT_SIZE = 8192;         // n_ctx に使う値
	static constexpr uint32_t MAX_RESPONSE_TOKENS = 2048;  // max_tokens に使う値（CONTEXT_SIZE未満であること）
};
