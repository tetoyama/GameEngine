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
	cParams.n_ctx = MAX_CONTEXT_TOKEN; // 最大コンテキスト長
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
	logoTexture = m_editor->resourceService->Load<TextureData>("Asset/BRAIN/logo/Icon.png");
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
	cParams.n_ctx = MAX_CONTEXT_TOKEN;
	cParams.n_threads = (std::max)(1u, std::thread::hardware_concurrency());

	m_llamaContext = llama_init_from_model(m_llamaModel, cParams);

	CreateSampler();

	m_nPast = 0;
	m_conversationActive = false;
}

// ============================================================
// Run LLM (streaming)
// ============================================================
void BRAIN::RunLLM_Internal(const std::string& prompt) {
	if (prompt.empty()) {
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
		m_chatLog.push_back({ ChatEntry::Role::User, prompt });
		m_chatLog.push_back({ ChatEntry::Role::Assistant, "" }); // 仮エントリ
	}

	std::string userPrompt = "<|user|>\n" + prompt + "\n<|assistant|>\n";
	const llama_vocab* vocab = llama_model_get_vocab(m_llamaModel);

	std::vector<llama_token> tokens(userPrompt.size() * 2 + 16);
	int n_prompt = llama_tokenize(vocab, userPrompt.c_str(), (int)userPrompt.size(),
								  tokens.data(), (int)tokens.size(), true, false);
	if (n_prompt <= 0) {
		m_isRunning.store(false, std::memory_order_release);
		m_editor->debugLogSystem->LOG_WARNING("BRAIN: n_prompt <= 0");
		return;
	}
	tokens.resize(n_prompt);

	// -------------------
	// コンテキストに収まるよう調整
	// -------------------
	EnsureContextFits(m_pastTokens);

	// -------------------
	// デコードユーザ入力
	// -------------------
	llama_batch batch = llama_batch_init(n_prompt, 0, 1);
	for (int i = 0; i < n_prompt; ++i) {
		batch.token[i] = tokens[i];
		batch.pos[i] = m_nPast + i;
		batch.seq_id[i][0] = 0;
		batch.n_seq_id[i] = 1;
	}
	batch.n_tokens = n_prompt;
	batch.logits[n_prompt - 1] = 1;

	if (llama_decode(m_llamaContext, batch) != 0) {
		llama_batch_free(batch);
		m_isRunning.store(false, std::memory_order_release);
		m_editor->debugLogSystem->LOG_WARNING("BRAIN: llama_decode failed");
		return;
	}
	llama_batch_free(batch);

	// トークンを保存
	m_pastTokens.insert(m_pastTokens.end(), tokens.begin(), tokens.end());
	m_nPast += n_prompt;

	// -------------------
	// アシスタント応答生成
	// -------------------
	int newlineCount = 0;
	while (m_threadRunning && !m_stopRequested) {
		llama_token tok = llama_sampler_sample(m_sampler, m_llamaContext, -1);
		if (tok == LLAMA_TOKEN_NULL) break;
		if (llama_vocab_is_eog(vocab, tok)) break;
		if (llama_vocab_is_control(vocab, tok)) continue;

		char buf[64];
		int len = llama_token_to_piece(vocab, tok, buf, sizeof(buf), 0, false);
		if (len > 0) {
			std::lock_guard<std::mutex> lock(m_outputMutex);
			m_asyncOutput.append(buf, len);
			if (!m_chatLog.empty() && m_chatLog.back().role == ChatEntry::Role::Assistant)
				m_chatLog.back().text = m_asyncOutput;

			// 改行2回で応答終了
			if (buf[0] == '\n') {
				if (++newlineCount >= 2) break;
			} else {
				newlineCount = 0;
			}
		}

		// トークン保存
		m_pastTokens.push_back(tok);

		llama_batch b = llama_batch_init(1, 0, 1);
		b.token[0] = tok;
		b.pos[0] = m_nPast;
		b.seq_id[0][0] = 0;
		b.n_seq_id[0] = 1;
		b.n_tokens = 1;
		b.logits[0] = 1;

		if (llama_decode(m_llamaContext, b) != 0) {
			llama_batch_free(b);
			break;
		}
		llama_batch_free(b);
		++m_nPast;
	}

	m_isRunning.store(false, std::memory_order_release);
}

std::string BRAIN::DecodeTokensToString(const std::vector<llama_token>& tokens, const llama_vocab* vocab) {
	std::string result;
	char buf[64];
	for (auto tok : tokens) {
		int len = llama_token_to_piece(vocab, tok, buf, sizeof(buf), 0, false);
		if (len > 0) result.append(buf, len);
	}
	return result;
}

std::string BRAIN::SummarizeText(const std::string& text) {
	if (!m_llamaModel) return "";

	m_editor->debugLogSystem->LOG_DEBUG("BRAIN: SummarizeText start");

	// --- 新規 LLM コンテキスト作成 ---
	llama_context_params cParams = llama_context_default_params();
	cParams.n_ctx = MAX_CONTEXT_TOKEN;
	if (cParams.n_ctx < text.size() * 3) {
		cParams.n_ctx = text.size() * 3;
	}
	cParams.n_threads = (std::max)(1u, std::thread::hardware_concurrency());

	llama_context* tempContext = llama_init_from_model(m_llamaModel, cParams);
	if (!tempContext) {
		m_editor->debugLogSystem->LOG_WARNING("BRAIN: failed to create temporary context for summarization");
		return "";
	}

	// --- サマリ専用サンプラー ---
	llama_sampler_chain_params sp = llama_sampler_chain_default_params();
	llama_sampler* tempSampler = llama_sampler_chain_init(sp);
	llama_sampler_chain_add(tempSampler, llama_sampler_init_top_k(20));
	llama_sampler_chain_add(tempSampler, llama_sampler_init_top_p(0.85f, 1));
	llama_sampler_chain_add(tempSampler, llama_sampler_init_temp(0.5f));
	llama_sampler_chain_add(tempSampler, llama_sampler_init_dist(
		(uint32_t)std::chrono::system_clock::now().time_since_epoch().count()));
	llama_sampler_chain_add(tempSampler, llama_sampler_init_penalties(128, 1.5f, 0.0f, 0.0f));

	const llama_vocab* vocab = llama_model_get_vocab(m_llamaModel);

	// --- プロンプト作成 ---
	std::string prompt = "<|user|>\n次のテキストを要約してください（英語で簡潔に、最大 "
		+ std::to_string(MAX_CONTEXT_TOKEN / 4) + "文字）:\n" + text + "\n<|assistant|>\n";

	OutputDebugStringA(prompt.c_str());

	// --- トークン化 ---
	std::vector<llama_token> tokens(prompt.size() * 3 + 128); // 安全に余裕をもたせる
	int n_tokens = llama_tokenize(vocab, prompt.c_str(), (int)prompt.size(),
								  tokens.data(), (int)tokens.size(), true, false);
	if (n_tokens <= 0) {
		llama_free(tempContext);
		llama_sampler_free(tempSampler);
		return "";
	}
	tokens.resize(n_tokens);

	// --- デコード入力 ---
	llama_batch batch = llama_batch_init(n_tokens, 0, 1);
	for (int i = 0; i < n_tokens; ++i) {
		batch.token[i] = tokens[i];
		batch.pos[i] = i;
		batch.seq_id[i][0] = 0;
		batch.n_seq_id[i] = 1;
	}
	batch.n_tokens = n_tokens;
	if (llama_decode(tempContext, batch) != 0) {
		llama_batch_free(batch);
		llama_free(tempContext);
		llama_sampler_free(tempSampler);
		return "";
	}
	llama_batch_free(batch);

	// --- 応答生成 ---
	std::string output;
	int newlineCount = 0;
	const int MAX_OUTPUT_TOKENS = 256; // 応答トークン上限
	for (int i = 0; i < MAX_OUTPUT_TOKENS; ++i) {
		llama_token tok = llama_sampler_sample(tempSampler, tempContext, -1);
		if (tok == LLAMA_TOKEN_NULL) break;
		if (llama_vocab_is_eog(vocab, tok)) break;
		if (llama_vocab_is_control(vocab, tok)) continue;

		char buf[64];
		int len = llama_token_to_piece(vocab, tok, buf, sizeof(buf), 0, false);
		if (len > 0) {
			output.append(buf, len);

			if (buf[0] == '\n') {
				if (++newlineCount >= 2) break;
			} else {
				newlineCount = 0;
			}
		}

		// トークンをデコード
		llama_batch b = llama_batch_init(1, 0, 1);
		b.token[0] = tok;
		b.pos[0] = i;
		b.seq_id[0][0] = 0;
		b.n_seq_id[0] = 1;
		b.n_tokens = 1;
		b.logits[0] = 1;
		if (llama_decode(tempContext, b) != 0) {
			llama_batch_free(b);
			break;
		}
		llama_batch_free(b);
	}

	// --- 終了処理 ---
	llama_free(tempContext);
	llama_sampler_free(tempSampler);

	// --- デバッグ ---
	std::string debugMsg = "BRAIN: SummarizeText done\n" + output;
	m_editor->debugLogSystem->LOG_DEBUG(debugMsg.c_str());

	return output;
}

void BRAIN::EnsureContextFits(const std::vector<llama_token>& newTokens) {
	const int MAX_CTX = MAX_CONTEXT_TOKEN * 0.5f;

	int totalTokens = (int)(m_pastTokens.size() + newTokens.size());
	if (totalTokens >= MAX_CTX) {
		m_editor->debugLogSystem->LOG_DEBUG("BRAIN: EnsureContextFits");

		m_editor->debugLogSystem->LOG_DEBUG("BRAIN: Context exceeds MAX_CTX, performing summarization");

		// --- 過去トークンを文字列化 ---
		const llama_vocab* vocab = llama_model_get_vocab(m_llamaModel);
		std::string pastText = DecodeTokensToString(m_pastTokens, vocab);

		// --- 要約 ---
		std::string summary = SummarizeText(pastText);

		// --- 要約をトークン化 ---
		std::vector<llama_token> summaryTokens(summary.size() * 2 + 16);
		int n_summary = llama_tokenize(
			vocab,
			summary.c_str(),
			(int)summary.size(),
			summaryTokens.data(),
			(int)summaryTokens.size(),
			true,
			false
		);
		summaryTokens.resize(n_summary);

		// --- 最新の 10% トークンを保持 ---
		int keep = m_pastTokens.size() / 10;
		if (keep < 0) keep = 0;
		std::vector<llama_token> recentTokens(
			m_pastTokens.end() - keep,
			m_pastTokens.end()
		);

		// --- 新しい m_pastTokens = summaryTokens + recentTokens ---
		m_pastTokens = summaryTokens;
		m_pastTokens.insert(m_pastTokens.end(), recentTokens.begin(), recentTokens.end());

		// --- MAX_CTX に収まるよう調整 ---
		if ((int)m_pastTokens.size() > MAX_CTX - 1) {
			// 最新のトークンを削って調整
			m_pastTokens.erase(
				m_pastTokens.begin(),
				m_pastTokens.begin() + ((int)m_pastTokens.size() - (MAX_CTX - 1))
			);
		}

		// --- コンテキストをリセット ---
		ResetContext();

		// --- m_pastTokens を再注入 ---
		llama_batch batch = llama_batch_init((int)m_pastTokens.size(), 0, 1);
		for (int i = 0; i < (int)m_pastTokens.size(); ++i) {
			batch.token[i] = m_pastTokens[i];
			batch.pos[i] = i;
			batch.seq_id[i][0] = 0;
			batch.n_seq_id[i] = 1;
		}
		batch.n_tokens = (int)m_pastTokens.size();

		if (llama_decode(m_llamaContext, batch) != 0) {
			m_editor->debugLogSystem->LOG_WARNING("BRAIN: llama_decode failed during EnsureContextFits");
		}
		llama_batch_free(batch);

		m_nPast = (int)m_pastTokens.size();

		m_editor->debugLogSystem->LOG_DEBUG(
			"BRAIN: Context summarized and re-injected. New token count: " + std::to_string(m_nPast)
		);
	}
}




// ============================================================
// ImGui UI
// ============================================================
void BRAIN::Draw(const EditorDrawContext){
	bool* show = &m_editor->GetUI<MenuBar>()->showAssetsBrowser;
	if(!show || !*show) return;

	ImGui::Begin("B.R.A.I.N.", show);

	ImVec2 winPos = ImGui::GetWindowPos();
	ImVec2 winSize = ImGui::GetWindowSize();

	// -------------------
	// 背景にロゴ描画（アスペクト比維持）
	// -------------------
	if(logoTexture && logoTexture->pTexture){
		float texWidth = (float)logoTexture->Width;
		float texHeight = (float)logoTexture->Height;

		float texRatio = texWidth / texHeight;
		float winRatio = winSize.x / winSize.y;

		ImVec2 drawSize;
		if(winRatio > texRatio){
			drawSize.y = winSize.y;
			drawSize.x = winSize.y * texRatio;
		} else{
			drawSize.x = winSize.x;
			drawSize.y = winSize.x / texRatio;
		}

		// 中心配置
		ImVec2 drawPos;
		drawPos.x = winPos.x + (winSize.x - drawSize.x) * 0.5f;
		drawPos.y = winPos.y + (winSize.y - drawSize.y) * 0.5f;

		ImGui::GetWindowDrawList()->AddImage(
			logoTexture->pTexture.Get(),
			drawPos,
			ImVec2(drawPos.x + drawSize.x, drawPos.y + drawSize.y),
			ImVec2(0, 0),
			ImVec2(1, 1),
			IM_COL32(255, 255, 255, 8) // 半透明
		);
	}

	// -------------------
	// Chat log
	// -------------------
	float chatLogHeight = winSize.y - 240.0f;

	ImGui::Text("Conversation Log:");
	ImGui::BeginChild("ChatLogChild", ImVec2(-1, chatLogHeight), true, ImGuiWindowFlags_HorizontalScrollbar);
	{
		std::lock_guard<std::mutex> lock(m_outputMutex);

		int count = 0;
		for(auto& entry : m_chatLog){
			std::string label = (entry.role == ChatEntry::Role::User) ? "User" : "Assistant";
			ImVec4 color = (entry.role == ChatEntry::Role::User) ? ImVec4(0.7f, 0.8f, 1.0f, 1.0f) : ImVec4(0.8f, 1.0f, 0.8f, 1.0f);

			std::string buf = entry.text + '\0';

			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
			ImGui::PushStyleColor(ImGuiCol_Text, color);

			float maxWidth = ImGui::GetContentRegionAvail().x; // 最大横幅
			ImGui::BeginChild(("PromptChild" + label + std::to_string(count)).c_str(), ImVec2(maxWidth, winSize.y * 0.2f), true);
			ImGui::Text("%s:", label.c_str());
			maxWidth = ImGui::GetContentRegionAvail().x;
			ImGui::InputTextMultiline(("##" + label + std::to_string(count)).c_str(), buf.data(), buf.size(), ImVec2(maxWidth, -1), ImGuiInputTextFlags_ReadOnly);
			ImGui::EndChild();

			ImGui::PopStyleColor(2);

			count++;
		}

		if(ImGui::GetScrollY() >= ImGui::GetScrollMaxY()){
			ImGui::SetScrollHereY(1.0f);
		}
	}
	ImGui::EndChild();

	ImGui::Separator();

	// -------------------
	// Prompt 入力欄
	// -------------------
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

	ImGui::Text(("Token:" + std::to_string(m_nPast) + "/" + std::to_string(MAX_CONTEXT_TOKEN)).c_str());

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
