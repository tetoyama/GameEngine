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

#include "../InterFace/IEditorUI.h"

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
		USER,
		ASSISTANT
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
	EditorService* editor = nullptr;

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
	std::mutex jobMutex;
	std::condition_variable jobCV;

	// ----------------------------
	// ワーカースレッド
	// ----------------------------
	std::thread workerThread;
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
	bool scrollToBottom = false;

	// -----------------------------
	// Token state (表示用)
	// -----------------------------
	uint32_t m_nPast = 0;
	static constexpr uint32_t MAX_CONTEXT_TOKEN = 8192;
};
