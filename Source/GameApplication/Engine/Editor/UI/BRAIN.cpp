#define _CRT_SECURE_NO_WARNINGS

#include "BRAIN.h"
#include <Service/DebugTools/DebugSystem.h>
#include <Backends/llama/llama.h>
#include "MenuBar.h"
#include <ImGui/imgui.h>

#include <chrono>
#include <vector>
#include <thread>
#include <filesystem>

#include "Resources/resourceService.h"
#include "Resources/Loader/textureLoader.h"
#include "Resources/Data/textureData.h"

#pragma comment(lib, "llama.lib")

// ============================================================
// Initialize BRAIN system
// ============================================================
void BRAIN::Initialize(EditorService* editor){
	m_editor = editor;

	llama_backend_init();

	// モデル存在チェック
	if(!std::filesystem::exists(modelPath)){
		m_editor->debugLogSystem->LOG_ERROR("BRAIN: model not found: " + modelPath.string());
		return;
	}

	// -------------------
	// モデルロード
	// -------------------
	llama_model_params mParams = llama_model_default_params();
	mParams.n_gpu_layers = 0; // GPUなし

	m_llamaModel = llama_model_load_from_file(modelPath.string().c_str(), mParams);
	if(!m_llamaModel){
		m_editor->debugLogSystem->LOG_ERROR("BRAIN: failed to load model");
		return;
	}

	// -------------------
	// コンテキスト初期化
	// -------------------
	llama_context_params cParams = llama_context_default_params();
	cParams.n_ctx = 2048; // 最大コンテキスト長
	cParams.n_threads = (std::max)(1u, std::thread::hardware_concurrency());

	m_llamaContext = llama_init_from_model(m_llamaModel, cParams);
	if(!m_llamaContext){
		m_editor->debugLogSystem->LOG_ERROR("BRAIN: failed to create context");
		return;
	}

	// -------------------
	// サンプラー初期化
	// -------------------
	CreateSampler();

	// -------------------
	// ワーカースレッド開始
	// -------------------
	m_threadRunning = true;
	m_llmThread = std::thread(&BRAIN::LLMThreadMain, this);

	// -------------------
	// システムプロンプト流し込み
	// -------------------
	const llama_vocab* vocab = llama_model_get_vocab(m_llamaModel);
	std::string sys = std::string(SYSTEM_PROMPT) + "\n\n";

	std::vector<llama_token> tokens(sys.size() * 2 + 8);
	int n = llama_tokenize(vocab, sys.c_str(), (int)sys.size(), tokens.data(), (int)tokens.size(), true, false);
	tokens.resize(n);

	llama_batch batch = llama_batch_init(n, 0, 1);
	for(int i = 0; i < n; ++i){
		batch.token[i] = tokens[i];
		batch.pos[i] = m_nPast + i;
		batch.seq_id[i][0] = 0;
		batch.n_seq_id[i] = 1;
	}
	batch.n_tokens = n;

	llama_decode(m_llamaContext, batch);
	llama_batch_free(batch);

	m_nPast += n;
	m_conversationActive = true;

	// -------------------
	// ロゴテクスチャ読み込み
	// -------------------
	logoTexture = m_editor->resourceService->Load<TextureData>("Asset/BRIAN/logo/Icon.png");
}

// ============================================================
// Finalize BRAIN system
// ============================================================
void BRAIN::Finalize(){
	m_threadRunning = false;
	m_jobCV.notify_all();

	if(m_llmThread.joinable()) m_llmThread.join();

	if(m_sampler)      llama_sampler_free(m_sampler);
	if(m_llamaContext) llama_free(m_llamaContext);
	if(m_llamaModel)   llama_model_free(m_llamaModel);

	m_sampler = nullptr;
	m_llamaContext = nullptr;
	m_llamaModel = nullptr;
}

// ============================================================
// Worker thread main loop
// ============================================================
void BRAIN::LLMThreadMain(){
	while(m_threadRunning){
		LLMJob job;

		{
			std::unique_lock<std::mutex> lock(m_jobMutex);
			m_jobCV.wait(lock, [&]{
				return !m_jobQueue.empty() || m_requestReset || !m_threadRunning;
						 });

			if(!m_threadRunning) return;

			if(m_requestReset){
				ResetContext();
				m_requestReset = false;
				continue;
			}

			job = std::move(m_jobQueue.front());
			m_jobQueue.pop();
		}

		RunLLM_Internal(job.prompt);
	}
}

// ============================================================
// Reset LLM context
// ============================================================
void BRAIN::ResetContext(){
	if(m_llamaContext){
		llama_free(m_llamaContext);
		m_llamaContext = nullptr;
	}

	llama_context_params cParams = llama_context_default_params();
	cParams.n_ctx = 2048;
	cParams.n_threads = (std::max)(1u, std::thread::hardware_concurrency());

	m_llamaContext = llama_init_from_model(m_llamaModel, cParams);

	CreateSampler();

	m_nPast = 0;
	m_conversationActive = false;
}

// ============================================================
// Run LLM (streaming)
// ============================================================
void BRAIN::RunLLM_Internal(const std::string& prompt){
	if(prompt.empty()){
		m_editor->debugLogSystem->LOG_WARNING("BRAIN: prompt.empty()");
		return;
	}

	m_isRunning.store(true, std::memory_order_release);
	m_stopRequested = false;

	// -------------------
	// ログ準備
	// -------------------
	{
		std::lock_guard<std::mutex> lock(m_outputMutex);
		m_asyncOutput.clear();
		m_chatLog.push_back({ChatEntry::Role::User, prompt});
		m_chatLog.push_back({ChatEntry::Role::Assistant, ""}); // 仮エントリ
	}

	std::string userPrompt = "<|user|>\n" + prompt + "\n<|assistant|>\n";
	const llama_vocab* vocab = llama_model_get_vocab(m_llamaModel);

	std::vector<llama_token> tokens(prompt.size() * 2 + 16);
	int n_prompt = llama_tokenize(vocab, userPrompt.c_str(), (int)userPrompt.size(), tokens.data(), (int)tokens.size(), true, false);
	if(n_prompt <= 0){
		m_isRunning.store(false, std::memory_order_release);
		m_editor->debugLogSystem->LOG_WARNING("BRAIN: n_prompt <= 0");
		return;
	}
	tokens.resize(n_prompt);

	// -------------------
	// デコードユーザ入力
	// -------------------
	llama_batch batch = llama_batch_init(n_prompt, 0, 1);
	for(int i = 0; i < n_prompt; ++i){
		batch.token[i] = tokens[i];
		batch.pos[i] = m_nPast + i;
		batch.seq_id[i][0] = 0;
		batch.n_seq_id[i] = 1;
	}
	batch.n_tokens = n_prompt;
	batch.logits[n_prompt - 1] = 1;

	if(llama_decode(m_llamaContext, batch) != 0){
		llama_batch_free(batch);
		m_isRunning.store(false, std::memory_order_release);
		m_editor->debugLogSystem->LOG_WARNING("BRAIN: llama_decode failed");
		return;
	}
	llama_batch_free(batch);
	m_nPast += n_prompt;

	// -------------------
	// アシスタント応答生成
	// -------------------
	int newlineCount = 0;
	while(m_threadRunning && !m_stopRequested){
		llama_token tok = llama_sampler_sample(m_sampler, m_llamaContext, -1);
		if(tok == LLAMA_TOKEN_NULL) break;
		if(llama_vocab_is_eog(vocab, tok)) break;
		if(llama_vocab_is_control(vocab, tok)) continue;

		char buf[64];
		int len = llama_token_to_piece(vocab, tok, buf, sizeof(buf), 0, false);
		if(len > 0){
			std::lock_guard<std::mutex> lock(m_outputMutex);
			m_asyncOutput.append(buf, len);
			if(!m_chatLog.empty() && m_chatLog.back().role == ChatEntry::Role::Assistant)
				m_chatLog.back().text = m_asyncOutput;

			// 改行2回で応答終了
			if(buf[0] == '\n'){
				if(++newlineCount >= 2) break;
			} else{
				newlineCount = 0;
			}
		}

		llama_batch b = llama_batch_init(1, 0, 1);
		b.token[0] = tok;
		b.pos[0] = m_nPast;
		b.seq_id[0][0] = 0;
		b.n_seq_id[0] = 1;
		b.n_tokens = 1;
		b.logits[0] = 1;

		if(llama_decode(m_llamaContext, b) != 0){
			llama_batch_free(b);
			break;
		}
		llama_batch_free(b);
		++m_nPast;
	}

	m_isRunning.store(false, std::memory_order_release);
}

// ============================================================
// ImGui UI
// ============================================================
void BRAIN::Draw(const EditorDrawContext){
	bool* show = &m_editor->GetUI<MenuBar>()->showAssetsBrowser;
	if(!show || !*show) return;

	ImGui::Begin("B.R.A.I.N.", show);

	ImGui::InputTextMultiline("##Prompt", inputBuffer, sizeof(inputBuffer), ImVec2(-1, 100));

	bool running = m_isRunning.load(std::memory_order_acquire);

	// -------------------
	// Run ボタン
	// -------------------
	ImGui::BeginDisabled(running);
	if(ImGui::Button("Run")){
		LLMJob job{inputBuffer};
		{
			std::lock_guard<std::mutex> lock(m_jobMutex);
			m_jobQueue.push(std::move(job));
		}
		m_jobCV.notify_one();
	}
	ImGui::EndDisabled();

	// -------------------
	// Stop ボタン
	// -------------------
	ImGui::SameLine();
	ImGui::BeginDisabled(!running);
	if(ImGui::Button("Stop")){
		m_stopRequested = true;
	}
	ImGui::EndDisabled();

	// -------------------
	// Reset Conversation ボタン
	// -------------------
	ImGui::SameLine();
	ImGui::BeginDisabled(running);
	if(ImGui::Button("Reset Conversation")){
		{
			std::lock_guard<std::mutex> lock(m_jobMutex);
			m_requestReset = true;
		}
		m_jobCV.notify_one();

		std::lock_guard<std::mutex> lock(m_outputMutex);
		m_asyncOutput.clear();
		m_chatLog.clear();
	}
	ImGui::EndDisabled();

	ImGui::Separator();
	ImGui::Text("Conversation Log:");

	// -------------------
	// Chat log
	// -------------------
	ImGui::BeginChild("ChatLogChild");
	std::lock_guard<std::mutex> lock(m_outputMutex);

	int count = 0;
	for(auto& entry : m_chatLog){
		std::string label = (entry.role == ChatEntry::Role::User) ? "User" : "Assistant";
		ImVec4 color = (entry.role == ChatEntry::Role::User) ? ImVec4(0.7f, 0.8f, 1.0f, 1.0f) : ImVec4(0.8f, 1.0f, 0.8f, 1.0f);

		std::string buf = entry.text + '\0';

		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_Text, color);
		ImGui::Text("%s:", label.c_str());
		ImGui::InputTextMultiline(("##" + label + std::to_string(count)).c_str(), buf.data(), buf.size(), ImVec2(-1, 0), ImGuiInputTextFlags_ReadOnly);
		ImGui::PopStyleColor(2);

		count++;
	}

	if(ImGui::GetScrollY() >= ImGui::GetScrollMaxY()){
		ImGui::SetScrollHereY(1.0f);
	}

	ImGui::EndChild();
	ImGui::End();
}

// ============================================================
// Create LLM sampler chain
// ============================================================
void BRAIN::CreateSampler(){
	if(m_sampler){
		llama_sampler_free(m_sampler);
		m_sampler = nullptr;
	}

	llama_sampler_chain_params sp = llama_sampler_chain_default_params();
	m_sampler = llama_sampler_chain_init(sp);

	// Top-K
	llama_sampler_chain_add(m_sampler, llama_sampler_init_top_k(20));

	// Top-P
	llama_sampler_chain_add(m_sampler, llama_sampler_init_top_p(0.85f, 1));

	// 温度
	llama_sampler_chain_add(m_sampler, llama_sampler_init_temp(0.6f));

	// 時刻ベースシード
	llama_sampler_chain_add(m_sampler, llama_sampler_init_dist(
		(uint32_t)std::chrono::system_clock::now().time_since_epoch().count()
	));

	// ペナルティ設定
	llama_sampler_chain_add(m_sampler, llama_sampler_init_penalties(
		512,  // last n
		2.0f, // repeat penalty
		0.0f, // freq
		0.0f  // presence
	));
}
