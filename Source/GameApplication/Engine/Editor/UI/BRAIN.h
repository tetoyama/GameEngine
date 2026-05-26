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
	enum class Role{
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
	EditorService* m_pEditor = nullptr;

	// -----------------------------
	// Logo
	// -----------------------------
	std::shared_ptr<TextureData> m_LogoTexture;

	// -----------------------------
	// Chat / Output
	// -----------------------------
	std::vector<ChatEntry> m_ChatLog;
	std::string m_AsyncOutput;

	mutable std::mutex m_OutputMutex;

	// -----------------------------
	// Job queue
	// -----------------------------
	std::queue<LLMJob> m_JobQueue;
	std::mutex m_JobMutex;
	std::condition_variable m_JobCv;

	// ----------------------------
	// ワーカースレッド
	// ----------------------------
	std::thread m_WorkerThread;
	void WorkerLoop();

	// -----------------------------
	// Runtime state
	// -----------------------------
	std::atomic<bool> m_IsRunning{false};
	std::atomic<bool> m_StopRequested{false};
	std::atomic<bool> m_ExitRequested{false};
	std::atomic<bool> m_RequestReset{false};

	// -------------------------
	// LLM
	// -------------------------
	std::shared_ptr<LLAMAModelData> m_LlamaModel;
	std::shared_ptr<AgentConfig> m_AgentConfig;
	std::shared_ptr<LLAMAAgent> m_MainAgent;

	// -------------------------
	// UI state
	// -------------------------
	char m_InputBuffer[2048]{};
	bool m_ScrollToBottom= false;

	// -----------------------------
	// Token state (表示用)
	// -----------------------------
	uint32_t m_NPast= 0;
	static constexpr uint32_t m_MaxContextToken= 8192;
};
