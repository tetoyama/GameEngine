#include "BRAIN.h"
#include "BrainTools.h"

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

	m_agentConfig->system_prompt =
		"You are B.R.A.I.N. (Built-in Reasoning and Intelligence Node), "
		"an AI assistant embedded in a C++ game engine editor.\n"
		"You can browse the engine's source code and asset folder to answer questions.\n\n"
		"Available tools:\n"
		"  list_source_files              : List all source code files under Source/\n"
		"  read_source_file path=\"<path>\" : Read a source file "
		"(e.g., path=\"Source/GameApplication/Engine/Editor/UI/BRAIN.cpp\")\n"
		"  list_assets path=\"<path>\"      : List asset folder contents "
		"(e.g., path=\"Asset/\" or path=\"Asset/Model/\")\n\n"
		"To call a tool, include the following tag in your response:\n"
		"  <tool_call name=\"list_source_files\"/>\n"
		"  <tool_call name=\"read_source_file\" path=\"Source/path/to/file.cpp\"/>\n"
		"  <tool_call name=\"list_assets\" path=\"Asset/\"/>\n\n"
		"You may use multiple tools in one response. "
		"The results will be provided to you automatically. "
		"After receiving the results, give a clear and helpful answer.";

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

		if (!hasJob) {
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
		// 実行（ツール呼び出しサポート、最大3ラウンド）
		// ---------------------------
		const int maxToolRounds = 3;
		std::string currentPrompt = job.prompt;

		for (int round = 0; round < maxToolRounds; ++round) {
			m_mainAgent->RunAsync(currentPrompt);

			{
				std::lock_guard<std::mutex> lock(m_outputMutex);
				m_chatLog.push_back({ ChatEntry::Role::Assistant, std::string() });
				m_scrollToBottom = true;
			}

			// Running 状態になるまで待機
			while (m_mainAgent->GetState() != LLAMAAgent::State::Running) {
				if (m_stopRequested.load()) break;
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}

			// 出力監視ループ（生出力をそのまま表示）
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

			std::string output = m_mainAgent->GetOutput();
			OutputDebugStringA(("BRAIN: Round " + std::to_string(round) +
				" completed. Output length: " + std::to_string(output.size()) + "\n").c_str());

			// ツール呼び出しを解析
			auto toolCalls = BrainTools::ParseToolCalls(output);

			// アシスタントエントリを最終更新：タグを除去、空なら削除
			// （このエントリは RunAsync 呼び出し前に必ず push 済み）
			{
				std::string stripped = BrainTools::StripToolCalls(output);
				// 空白文字（スペース・タブ・改行・CR）のみか確認
				bool hasText = (stripped.find_first_not_of(" \t\n\r") != std::string::npos);
				std::lock_guard<std::mutex> lock(m_outputMutex);
				if (!m_chatLog.empty() && m_chatLog.back().role == ChatEntry::Role::Assistant) {
					if (!hasText && !toolCalls.empty()) {
						// テキストなしでツール呼び出しのみ → エントリを削除
						m_chatLog.pop_back();
					} else {
						m_chatLog.back().text = stripped;
					}
				}
				m_scrollToBottom = true;
			}

			if (toolCalls.empty()) {
				// ツール呼び出しなし → 終了
				break;
			}
			if (round == maxToolRounds - 1) {
				// 最終ラウンド到達：残りのツール呼び出しを無視して終了
				OutputDebugStringA("BRAIN: maxToolRounds reached; remaining tool calls ignored.\n");
				break;
			}

			// ツール呼び出しを実行 → チャットログにコマンド風エントリを追加
			std::string toolResults;
			for (const auto& call : toolCalls) {
				// コマンドラベル: "> tool_name" または "> tool_name \"path\""
				std::string cmdLabel = "> " + call.name;
				if (!call.path.empty()) {
					cmdLabel += " \"" + call.path + "\"";
				}

				OutputDebugStringA(("BRAIN: Executing tool: " + cmdLabel + "\n").c_str());
				std::string result = BrainTools::ExecuteToolCall(call);

				// Tool エントリとして追加（コマンドヘッダー + 結果）
				{
					std::lock_guard<std::mutex> lock(m_outputMutex);
					m_chatLog.push_back({ ChatEntry::Role::Tool, cmdLabel + "\n" + result });
					m_scrollToBottom = true;
				}

				toolResults += "<tool_result name=\"" + call.name + "\">\n";
				toolResults += result;
				toolResults += "\n</tool_result>\n";
			}

			// 次のラウンド：ツール結果をプロンプトとして渡す
			currentPrompt = toolResults;
		}

		OutputDebugStringA(("BRAIN: LLM job completed. Output length: " +
			std::to_string(m_mainAgent->GetOutput().size()) + "\n" +
			m_mainAgent->GetOutput()).c_str());

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
		const char* label;
		ImVec4 color;

		switch(e.role){
		case ChatEntry::Role::User:
			label = "User";
			color = ImVec4(0.7f, 0.8f, 1.0f, 1.0f);
			break;
		case ChatEntry::Role::Tool:
			label = "Tool";
			color = ImVec4(1.0f, 0.85f, 0.4f, 1.0f);
			break;
		default: // Assistant
			label = "Assistant";
			color = ImVec4(0.8f, 1.0f, 0.8f, 1.0f);
			break;
		}

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

	bool running  = m_isRunning.load();
	bool canRun = !running && m_mainAgent && m_llamaModel;

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

		// 使用率バー
		float rate = m_mainAgent->GetTokenUsageRate();
		ImGui::ProgressBar(
			rate,
			ImVec2(-1, 0),
			nullptr
		);
	}

	ImGui::End();
}
