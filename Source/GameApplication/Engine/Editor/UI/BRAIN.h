#pragma once

#include "GameApplication.h"

#include "Editor/editorService.h"
#include "Editor/InterFace/IEditorUI.h"

#include <string>
#include <filesystem>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>

// llama forward declarations
struct llama_model;
struct llama_context;
struct llama_sampler;

struct TextureData;

// B.R.A.I.N. = Buddy for Runtime Artificial Intelligence Navigator

struct ChatEntry {
	enum class Role {
		User, Assistant
	};
	Role role;
	std::string text;
};

class BRAIN: public IEditorUI {

public:
	void Initialize(EditorService* editor) override;
	void Finalize() override;
	void Draw(const EditorDrawContext ctx) override;

private:
	// ===== 非同期制御 =====
	void LLMThreadMain();
	void ResetContext();
	void CreateSampler();
	// 逐次生成（ワーカースレッド専用）
	void RunLLM_Internal(const std::string& prompt);

private:
	// ===== Editor =====
	EditorService* m_editor = nullptr;

	// ===== llama =====
	llama_model* m_llamaModel = nullptr;
	llama_context* m_llamaContext = nullptr;
	llama_sampler* m_sampler = nullptr;

	// ===== model path =====
	static inline const std::filesystem::path modelPath =
		"Asset/BRAIN/model/qwen2.5-coder-7b-instruct-q4_k_m.gguf";

	// ===== UI buffers =====
	char        inputBuffer[512]{};
	std::string outputText;

	// ===== マルチスレッド関連 =====
	struct LLMJob {
		std::string prompt;
	};

	std::thread              m_llmThread;
	std::atomic<bool>        m_threadRunning{false};

	std::mutex               m_jobMutex;
	std::condition_variable  m_jobCV;
	std::queue<LLMJob>       m_jobQueue;

	// 逐次出力用（UIと共有）
	std::mutex               m_outputMutex;
	std::string              m_asyncOutput;

	bool m_requestReset = false;
	// --- generate control ---
	std::atomic<bool> m_stopRequested =false;
	std::atomic<bool> m_isRunning = false;

	// 会話管理
	bool m_conversationActive = false;
	int  m_nPast = 0;

	// system prompt
	static constexpr const char* SYSTEM_PROMPT =
		"<|system|>\n"
		"You are a helpful assistant integrated into a game engine editor.\n Your name is B.R.A.I.N. = Buddy for Runtime Artificial Intelligence Navigator";

	std::shared_ptr<TextureData> logoTexture = nullptr;

	// 会話ログ（User / Assistant のペアを順に保持）
	std::vector<ChatEntry> m_chatLog;

};
