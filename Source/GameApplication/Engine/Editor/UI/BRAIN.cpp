#include "BRAIN.h"

#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <cstring>
#include <cassert>

#include "Backends/ImGui/imgui.h"

#include "../EditorService.h"
#include "MenuBar.h"

#include "Resources/Data/textureData.h"
#include "Resources/Data/llamaModelData.h"
#include "Resources/Loader/llamaModelLoader.h"
#include "Resources/Loader/textureLoader.h"
#include "Resources/resourceService.h"

#include "Service/LlamaService/LLAMAService.h"
#include "Service/LlamaService/LLAMAAgent.h"
#include "Service/LlamaService/AgentConfig.h"

#include "DebugTools/DebugSystem.h"

// --------------------------------------------
// 定数
// --------------------------------------------
static constexpr int MAX_CONTEXT_TOKEN = 4096;

// --------------------------------------------
// Initialize
// --------------------------------------------
void BRAIN::Initialize(EditorService* editor){
	m_editor = editor;

	m_isRunning.store(false);
	m_stopRequested.store(false);
	m_exitRequested.store(false);
	m_requestReset.store(false);
	m_scrollToBottom = false;
	m_nPast = 0;

	std::memset(inputBuffer, 0, sizeof(inputBuffer));

	// ロゴ
	logoTexture = m_editor->resourceService
		->Load<TextureData>("Asset/BRAIN/logo/Icon.png");


	// ---------------------------------
	// AgentConfig
	// ---------------------------------
	m_agentConfig = std::make_shared<AgentConfig>();
	m_agentConfig->max_tokens = MAX_CONTEXT_TOKEN;
	m_agentConfig->n_ctx = 8192;
	//m_agentConfig->n_ctx = 256;
	m_agentConfig->n_threads =
		(std::max)(1u, std::thread::hardware_concurrency());

	// ---------------------------------
	// モデルロード
	// ---------------------------------
	m_editor->llamaService->LoadModelAsync(
		"Asset/BRAIN/model/qwen2.5-coder-7b-instruct-q4_k_m.gguf",
		[this](bool success){
			if(!success){
				m_editor->debugLogSystem->LOG_ERROR(
					"B.R.A.I.N.: Failed to load LLM model."
				);
				// ロード失敗
				m_llamaModel.reset();
				return;
			} else {

				m_editor->debugLogSystem->LOG_TRACE(
					"B.R.A.I.N.: Success to load LLM model."
				);

				m_llamaModel =
					m_editor->llamaService
					->GetModel(
						"Asset/BRAIN/model/qwen2.5-coder-7b-instruct-q4_k_m.gguf");

			}
		}
	);

	// ---------------------------------
	// Worker thread
	// ---------------------------------
	m_workerThread = std::thread(&BRAIN::WorkerLoop, this);
}

// --------------------------------------------
// Finalize
// --------------------------------------------
void BRAIN::Finalize(){
	{
		std::lock_guard<std::mutex> lock(m_jobMutex);
		m_exitRequested.store(true);
	}
	m_jobCV.notify_one();

	if(m_workerThread.joinable()){
		m_workerThread.join();
	}

	if(m_mainAgent){
		m_mainAgent->Stop();
		m_mainAgent.reset();
	}
	m_llamaModel.reset();
	m_agentConfig.reset();
}

// -------------------------------------------- 
// WorkerLoop 
// -------------------------------------------- 
void BRAIN::WorkerLoop() {
	while (true) {

		LLMJob job;
		bool hasJob = false;
		bool doReset = false;

		{
			std::unique_lock<std::mutex> lock(m_jobMutex);

			m_jobCV.wait_for(
				lock,
				std::chrono::milliseconds(100),
				[&]() {
				return m_exitRequested.load()
					|| m_requestReset.load()
					|| !m_jobQueue.empty();
			});

			if (m_exitRequested.load()) {
				return;
			}

			if (m_requestReset.load()) {
				m_requestReset.store(false);
				doReset = true;
			}

			if (!m_jobQueue.empty()) {
				job = std::move(m_jobQueue.front());
				m_jobQueue.pop();
				hasJob = true;
				m_isRunning.store(true);
			}
		}

		// ---------------------------
		// Reset 処理
		// ---------------------------
		if (doReset) {
			if (m_mainAgent) {
				while (m_mainAgent->GetState() == LLAMAAgent::State::Running) {
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}
				m_mainAgent->ResetContext();
			}

			m_nPast = 0;

			{
				std::lock_guard<std::mutex> lock(m_outputMutex);
				m_chatLog.clear();
				m_scrollToBottom = false;
			}

			m_isRunning.store(false);
			continue;
		}

		// ---------------------------
		// Agent 初期化
		// ---------------------------
		if (!m_mainAgent) {
			m_llamaModel =
				m_editor->llamaService->GetModel(
					"Asset/BRAIN/model/qwen2.5-coder-7b-instruct-q4_k_m.gguf");

			if (m_llamaModel) {
				m_mainAgent =
					m_editor->llamaService
					->CreateAgent(m_llamaModel, m_agentConfig);
			}
		}

		if (!hasJob || !m_mainAgent) {
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			continue;
		}

		// ---------------------------
		// User log
		// ---------------------------
		{
			std::lock_guard<std::mutex> lock(m_outputMutex);
			m_chatLog.push_back({ ChatEntry::Role::User, job.prompt });
			m_scrollToBottom = true;
		}

		// ---------------------------
		// 実行
		// ---------------------------
		m_mainAgent->RunAsync(job.prompt);

		{
			std::lock_guard<std::mutex> lock(m_outputMutex);
			m_chatLog.push_back({ ChatEntry::Role::Assistant, std::string() });
			m_scrollToBottom = true;
		}
		while (m_mainAgent->GetState() != LLAMAAgent::State::Running) {
		}
		// ---------------------------
		// 出力監視ループ（Running状態監視一本化）
		// ---------------------------
		while (m_mainAgent->GetState() == LLAMAAgent::State::Running) {
			if (m_stopRequested.load()) {
				m_mainAgent->Stop();
				m_stopRequested.store(false);
				break;
			}

			std::string out = m_mainAgent->GetOutput();
			{
				std::lock_guard<std::mutex> lock(m_outputMutex);
				if (!m_chatLog.empty() && m_chatLog.back().role == ChatEntry::Role::Assistant) {
					m_chatLog.back().text = out;
					m_scrollToBottom = true;
				}
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
		OutputDebugStringA(("BRAIN: LLM job completed. Output length: " + std::to_string(m_mainAgent->GetOutput().size()) + "\n" + m_mainAgent->GetOutput()).c_str());

		m_isRunning.store(false);
	}
}


// --------------------------------------------
// Draw
// --------------------------------------------
void BRAIN::Draw(const EditorDrawContext){
	bool* show =
		&m_editor->GetUI<MenuBar>()->showAssetsBrowser;
	if(!show || !*show) return;

	ImGui::Begin("B.R.A.I.N.", show);

	ImVec2 winPos = ImGui::GetWindowPos();
	ImVec2 winSize = ImGui::GetWindowSize();

	// -------------------
	// 背景ロゴ
	// -------------------
	if(logoTexture && logoTexture->pTexture){
		float texW = (float)logoTexture->Width;
		float texH = (float)logoTexture->Height;

		float texRatio = texW / texH;
		float winRatio = winSize.x / winSize.y;

		ImVec2 drawSize;
		if(winRatio > texRatio){
			drawSize.y = winSize.y;
			drawSize.x = winSize.y * texRatio;
		} else{
			drawSize.x = winSize.x;
			drawSize.y = winSize.x / texRatio;
		}

		ImVec2 drawPos{
			winPos.x + (winSize.x - drawSize.x) * 0.5f,
			winPos.y + (winSize.y - drawSize.y) * 0.5f
		};

		ImGui::GetWindowDrawList()->AddImage(
			logoTexture->pTexture.Get(),
			drawPos,
			ImVec2(drawPos.x + drawSize.x,
				   drawPos.y + drawSize.y),
			ImVec2(0, 0),
			ImVec2(1, 1),
			IM_COL32(255, 255, 255, 8)
		);
	}

	// -------------------
	// Chat log
	// -------------------
	float chatLogHeight = winSize.y - 256.0f;

	ImGui::Text("Conversation Log:");
	ImGui::BeginChild(
		"ChatLogChild",
		ImVec2(-1, chatLogHeight),
		true
	);

	// 描画用にコピー（スレッド安全）
	std::vector<ChatEntry> logCopy;
	{
		std::lock_guard<std::mutex> lock(m_outputMutex);
		logCopy = m_chatLog;
	}

	for(const auto& e : logCopy){
		const char* label =
			(e.role == ChatEntry::Role::User)
			? "User"
			: "Assistant";

		ImVec4 color =
			(e.role == ChatEntry::Role::User)
			? ImVec4(0.7f, 0.8f, 1.0f, 1.0f)
			: ImVec4(0.8f, 1.0f, 0.8f, 1.0f);

		ImGui::PushStyleColor(ImGuiCol_Text, color);
		ImGui::Text("%s:", label);
		// 長すぎる文字列も分割表示
		const size_t chunkSize = 4096;
		for(size_t offset = 0; offset < e.text.size(); offset += chunkSize){
			ImGui::TextWrapped("%.*s",
							   (int)(std::min)(chunkSize, e.text.size() - offset),
							   e.text.c_str() + offset
			);
		}

		ImGui::PopStyleColor();
	}

	if(m_scrollToBottom){
		ImGui::SetScrollHereY(1.0f);
		m_scrollToBottom = false;
	}

	ImGui::EndChild();



	ImGui::Separator();

	// -------------------
	// Prompt
	// -------------------
	ImGui::InputTextMultiline(
		"##Prompt",
		inputBuffer,
		sizeof(inputBuffer),
		ImVec2(-1, 100)
	);

	bool running = m_isRunning.load();
	bool canRun =
		!running && m_mainAgent && m_llamaModel;

	ImGui::BeginDisabled(!canRun);
	if(ImGui::Button("Run")){
		LLMJob job;
		job.prompt = std::string(inputBuffer);
		{
			std::lock_guard<std::mutex> lock(m_jobMutex);
			m_jobQueue.push(std::move(job));
		}
		m_jobCV.notify_one();
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::BeginDisabled(!running);
	if(ImGui::Button("Stop")){
		m_stopRequested.store(true);
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::BeginDisabled(running);
	if(ImGui::Button("Reset Conversation")){
		{
			std::lock_guard<std::mutex> lock(m_jobMutex);
			m_requestReset.store(true);
		}
		m_jobCV.notify_one();

		std::lock_guard<std::mutex> lock(m_outputMutex);
		m_chatLog.clear();
		m_scrollToBottom = false;
	}
	ImGui::EndDisabled();

	if(m_mainAgent){
		int used = m_mainAgent->GetTokenCount();
		int max = m_mainAgent->GetMaxTokenCount();

		ImGui::Text("Token: %d / %d", used, max);

		// 使用率バー（おすすめ）
		float rate = m_mainAgent->GetTokenUsageRate();
		ImGui::ProgressBar(
			rate,
			ImVec2(-1, 0),
			nullptr
		);
	}

	ImGui::End();
}
