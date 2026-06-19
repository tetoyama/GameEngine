// =======================================================================
// 
// LLAMAAgent.cpp
// 
// =======================================================================
#include "LLAMAAgent.h"

#include <cassert>
#include <chrono>
#include <vector>
#include <windows.h>
#include <sstream>
#include <algorithm>

#include "llama.h"
#include "convertWString.h"
#include "Resources/Data/llamaModelData.h"
#include "AgentConfig.h"

#define DBG_UTF8(x) OutputDebugStringW(Utf8ToWString(x).c_str())

// =====================================================
// 内部ヘルパー（生成経路を完全統一）
// =====================================================
llama_context* LLAMAAgent::CreateContext() const{
	assert(m_model);
	assert(m_config);

	OutputDebugStringA("[INFRA] LLAMAAgent::CreateContext start\n");

	llama_context_params c = llama_context_default_params();
	c.n_ctx = m_config->n_ctx;
	c.n_threads = m_config->n_threads;

	llama_context* ctx = llama_init_from_model(m_model->m_model, c);
	assert(ctx && "llama_init_from_model failed");

	OutputDebugStringA("[INFRA] LLAMAAgent::CreateContext end\n");
	return ctx;
}

llama_sampler* LLAMAAgent::CreateSampler() const{
	assert(m_config);

	OutputDebugStringA("[INFRA] LLAMAAgent::CreateSampler start\n");

	llama_sampler_chain_params sp = llama_sampler_chain_default_params();
	llama_sampler* sampler = llama_sampler_chain_init(sp);
	assert(sampler);

	// 【超強力なループクラッシャー設定】
		// コンテキスト全体ではなく「直近の256トークン」だけを監視対象にする
	int penalty_window = 256;

	// 同じフレーズを繰り返そうとした瞬間、強烈なマイナス補正(1.15)をかけて別の言葉を強制的に選ばせる
	float penalty_repeat = 1.15f;
	float penalty_freq = 0.05f;
	float penalty_present = 0.05f;

	llama_sampler_chain_add(sampler, llama_sampler_init_penalties(
		penalty_window,
		penalty_repeat,
		penalty_freq,
		penalty_present
	));

	llama_sampler_chain_add(sampler, llama_sampler_init_temp(m_config->temperature));
	llama_sampler_chain_add(sampler, llama_sampler_init_top_k(m_config->top_k));
	llama_sampler_chain_add(sampler, llama_sampler_init_top_p(m_config->top_p, 1));
	llama_sampler_chain_add(sampler, llama_sampler_init_dist(static_cast<uint32_t>(
		std::chrono::system_clock::now().time_since_epoch().count()
		)));

	OutputDebugStringA("[INFRA] LLAMAAgent::CreateSampler end\n");
	return sampler;
}

// =====================================================
// ctor / dtor / Stop / Public API
// =====================================================
LLAMAAgent::LLAMAAgent(std::shared_ptr<LLAMAModelData> model,
					   std::shared_ptr<const AgentConfig> config)
	: m_model(std::move(model))
	, m_config(std::move(config))
	, m_running(true)
	, m_nPast(0)
	, m_isSummarizing(false){

	assert(m_model);
	assert(m_config);
	assert(m_config->IsValid());

	OutputDebugStringA("[LIFE] LLAMAAgent ctor start\n");

	m_ctx = CreateContext();
	m_sampler = CreateSampler();

	m_thread = std::thread(&LLAMAAgent::WorkerMain, this);

	OutputDebugStringA("[LIFE] LLAMAAgent ctor end\n");
}

LLAMAAgent::~LLAMAAgent(){
	OutputDebugStringA("[LIFE] LLAMAAgent dtor start\n");

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_running.store(false);
	}
	m_cv.notify_all();

	if(m_thread.joinable())
		m_thread.join();

	if(m_sampler){
		llama_sampler_free(m_sampler);
		m_sampler = nullptr;
	}
	if(m_ctx){
		llama_free(m_ctx);
		m_ctx = nullptr;
	}

	m_state.store(State::Dead);

	OutputDebugStringA("[LIFE] LLAMAAgent dtor end\n");
}

void LLAMAAgent::Stop(){
	OutputDebugStringA("[API] LLAMAAgent::Stop called\n");

	std::lock_guard<std::mutex> lock(m_mutex);
	m_state.store(State::Stopping, std::memory_order_release);
	m_running.store(false, std::memory_order_release);
	m_cv.notify_all();
}

void LLAMAAgent::RunAsync(const std::string& prompt){
	if(prompt.empty()) return;

	std::lock_guard<std::mutex> lock(m_mutex);
	m_jobQueue.push(prompt);
	m_cv.notify_one();
}

std::string LLAMAAgent::GetOutput() const{
	std::lock_guard<std::mutex> lock(m_outputMutex);
	return m_visibleOutput;
}

int LLAMAAgent::GetTokenCount() const noexcept{
	return m_nPast;
}

int LLAMAAgent::GetMaxTokenCount() const noexcept{
	return m_config->n_ctx;
}

float LLAMAAgent::GetTokenUsageRate() const noexcept{
	return m_config->n_ctx > 0 ? static_cast<float>(m_nPast) / m_config->n_ctx : 0.0f;
}

// =====================================================
// WorkerMain
// =====================================================
void LLAMAAgent::WorkerMain(){
	OutputDebugStringA("[THREAD] LLAMAAgent::WorkerMain start\n");

	while(true){
		std::string prompt;
		{
			std::unique_lock<std::mutex> lock(m_mutex);
			m_cv.wait(lock, [&]{ return !m_jobQueue.empty() || !m_running.load(); });

			if(!m_running.load() && m_jobQueue.empty())
				break;

			prompt = std::move(m_jobQueue.front());
			m_jobQueue.pop();
		}

		m_state.store(State::Running, std::memory_order_release);
		try{
			RunPromptInternal(prompt);
		} catch(...){
			OutputDebugStringA("[THREAD] CRITICAL ERROR: WorkerMain caught exception!\n");
		}
		m_state.store(State::Idle, std::memory_order_release);
	}

	m_state.store(State::Dead, std::memory_order_release);
	OutputDebugStringA("[THREAD] LLAMAAgent::WorkerMain end\n");
}

// =====================================================
// RunPromptInternal — 履歴一元管理版
// =====================================================
void LLAMAAgent::RunPromptInternal(const std::string& prompt){
	OutputDebugStringA("==================================================\n");
	OutputDebugStringA("LLAMAAgent: RunPromptInternal START\n");
	OutputDebugStringA(("User Prompt: " + prompt + "\n").c_str());
	OutputDebugStringA("==================================================\n");

	if(!m_model || !m_ctx || !m_sampler){
		OutputDebugStringA("[PIPELINE] CRITICAL: Model, Context, or Sampler is missing!\n");
		return;
	}

	const llama_vocab* vocab = llama_model_get_vocab(m_model->m_model);
	if(!vocab){
		OutputDebugStringA("[PIPELINE] CRITICAL: Failed to get vocab!\n");
		return;
	}

	// 1. 今回のユーザー入力を履歴（歴史）に追加
	m_history.push_back({"user", prompt});

	// システムプロンプトの二重注入を完全に防ぐ
	if(!m_config->system_prompt.empty()){
		bool has_system = false;
		if(!m_history.empty() && m_history.front().role == "system"){
			has_system = true;
		}
		if(!has_system){
			m_history.insert(m_history.begin(), {"system", m_config->system_prompt});
			OutputDebugStringA("[PIPELINE] Injecting clean System Prompt into history head.\n");
		}
	}

	// =====================================================
	// 2. 差分メッセージのテンプレート構築
	// =====================================================
	std::vector<llama_chat_message> msgs;
	msgs.reserve(m_history.size());

	OutputDebugStringA(("[PIPELINE] Current History Chain Size: " + std::to_string(m_history.size()) + "\n").c_str());
	for(size_t i = 0; i < m_history.size(); ++i){
		msgs.push_back({m_history[i].role.c_str(), m_history[i].text.c_str()});
		OutputDebugStringA(("[History Node " + std::to_string(i) + "] [" + m_history[i].role + "]: " + m_history[i].text.substr(0, 60) + "...\n").c_str());
	}

	int32_t buf_size = 4096;
	std::string diffFormatted;
	diffFormatted.resize(buf_size);

	// テンプレート適用時にアシスタントタグ付与フラグ (add_ass) を常に true にする
	bool add_ass = true;
	int32_t n = llama_chat_apply_template(
		nullptr,
		msgs.data(),
		msgs.size(),
		add_ass,
		diffFormatted.data(),
		buf_size
	);

	if(n < 0){
		OutputDebugStringA("[PIPELINE] ERROR: llama_chat_apply_template failed!\n");
		return;
	}
	if(n > buf_size){
		diffFormatted.resize(n);
		n = llama_chat_apply_template(nullptr, msgs.data(), msgs.size(), add_ass, diffFormatted.data(), n);
		if(n < 0) return;
	}
	diffFormatted.resize(n);

	// 文字列の「末尾」にアシスタント開始タグがあるか厳密にチェックする
	std::string astTag = "<|im_start|>assistant";
	std::string astTagNL = "<|im_start|>assistant\n";

	if(diffFormatted.length() >= astTagNL.length() &&
	   diffFormatted.compare(diffFormatted.length() - astTagNL.length(), astTagNL.length(), astTagNL) == 0){
		// 既に改行付きで存在する場合は何もしない
	} else if(diffFormatted.length() >= astTag.length() &&
			  diffFormatted.compare(diffFormatted.length() - astTag.length(), astTag.length(), astTag) == 0){
		// タグはあるが改行がない場合
		diffFormatted += "\n";
	} else{
		// どちらでもなければ確実に末尾へ付与
		diffFormatted += "<|im_start|>assistant\n";
	}

	DBG_UTF8(("\n[DEBUG] Input Diff Text:\n" + diffFormatted + "\n----------------------------\n").c_str());

	// =====================================================
	// 3. トークナイズ
	// =====================================================
	std::vector<llama_token> tokens(diffFormatted.size() * 2);

	// 全文のテンプレートをトークン化するため常に is_first_token = true とする
	bool is_first_token = true;
	int ntok = llama_tokenize(
		vocab,
		diffFormatted.c_str(),
		(int)diffFormatted.size(),
		tokens.data(),
		(int)tokens.size(),
		is_first_token,
		true // 特殊コントロールトークンを開通
	);

	if(ntok <= 0){
		OutputDebugStringA("[PIPELINE] ERROR: Tokenize produced 0 tokens!\n");
		return;
	}
	tokens.resize(ntok);

	// =====================================================
	// 4. コンテキスト溢れチェック (75% の容量で要約をトリガー)
	// =====================================================
	const int overflow_threshold = static_cast<int>(m_config->n_ctx * 0.75f);

	// ntok は全文のトークン数なので、これだけで閾値を超えているか判定する
	if(ntok > overflow_threshold){
		OutputDebugStringA("[OVERFLOW] Context overflow detected! Triggering SummarizeAndReset...\n");

		if(!m_history.empty()){
			m_history.pop_back(); // 今回追加した分を一旦退避
		}

		SummarizeAndReset();

		OutputDebugStringA("[OVERFLOW] Re-trying RunPromptInternal with clean context summary baton.\n");
		RunPromptInternal(prompt);
		return;
	}

	// =====================================================
	// 5. 差分デコード (Prefix Match)
	// =====================================================
	// KVキャッシュ内の過去の記憶と、新しく作ったプロンプトの一致部分を特定する
	int reuse_count = 0;
	int check_limit = (std::min)((int)m_pastTokens.size(), ntok);
	for(int i = 0; i < check_limit; ++i){
		if(m_pastTokens[i] == tokens[i]){
			reuse_count++;
		} else{
			break;
		}
	}

	// もしキャッシュと一致しない部分（要約で歴史が変わった時など）があれば、一度綺麗にリセットする
	if(reuse_count < m_nPast){
		ResetContextUnlocked();
		reuse_count = 0; // 全て再計算させる
	}

	int tokens_to_decode = ntok - reuse_count;

	OutputDebugStringA(("Reusing " + std::to_string(reuse_count) + " tokens from KV cache.\n").c_str());
	OutputDebugStringA(("Decoding " + std::to_string(tokens_to_decode) + " new tokens.\n").c_str());

	if(tokens_to_decode > 0){
		llama_batch batch = llama_batch_init(tokens_to_decode, 0, 1);
		for(int i = 0; i < tokens_to_decode; i++){
			batch.token[i] = tokens[reuse_count + i];
			// 過去のキャッシュに末尾追加するのではなく、絶対座標(reuse_count + i)に配置する。これで重複バグが消滅する。
			batch.pos[i] = reuse_count + i;
			batch.seq_id[i][0] = 0;
			batch.n_seq_id[i] = 1;
			batch.logits[i] = (i == tokens_to_decode - 1);
		}
		batch.n_tokens = tokens_to_decode;

		if(llama_decode(m_ctx, batch) != 0){
			llama_batch_free(batch);
			OutputDebugStringA("[PIPELINE] CRITICAL: llama_decode for prompt failed!\n");
			return;
		}
		llama_batch_free(batch);
	}

	m_pastTokens = tokens;
	m_nPast = (int)m_pastTokens.size();

	// サンプラー同期
	OutputDebugStringA("[SAMPLER] Synchronizing sampler history with full context...\n");
	llama_sampler_reset(m_sampler);
	for(size_t i = 0; i < m_pastTokens.size(); ++i){
		llama_sampler_accept(m_sampler, m_pastTokens[i]);
	}
	OutputDebugStringA("[SAMPLER] Sampler synchronization completed.\n");

	// =====================================================
	// 6. 出力バッファのクリア
	// =====================================================
	{
		std::lock_guard<std::mutex> lock(m_outputMutex);
		m_output.clear();
		m_visibleOutput.clear();
	}

	// =====================================================
	// 7. 生成ループ
	// =====================================================
	OutputDebugStringA("[LOOP] Starting LLM Generation Loop...\n");

	llama_token tok = llama_sampler_sample(m_sampler, m_ctx, -1);
	llama_sampler_accept(m_sampler, tok);

	llama_batch gen = llama_batch_init(1, 0, 1);
	int loop = 0;
	bool nativeEogDetected = false;

	// 【修正点】生成ループは overflow_threshold ではなく、物理的な限界値 (m_config->n_ctx) まで回れるように解放する
	while(m_running.load() && m_nPast < m_config->n_ctx){

		if(tok == LLAMA_TOKEN_NULL){
			OutputDebugStringA("[LOOP] Stop Reason: LLAMA_TOKEN_NULL\n");
			break;
		}
		if(llama_vocab_is_eog(vocab, tok) || llama_vocab_get_attr(vocab, tok) == LLAMA_TOKEN_ATTR_CONTROL){
			OutputDebugStringA("[LOOP] Stop Reason: Native EOG/Control token detected.\n");
			nativeEogDetected = true;
			break;
		}

		char buf[256]{};
		int len = llama_token_to_piece(vocab, tok, buf, sizeof(buf), 0, false);

		if(len > 0){
			std::string piece(buf, len);

			// 仮想結合チェック
			std::string testString = m_output;
			if(testString.size() > 12){
				testString = testString.substr(testString.size() - 12);
			}
			testString += piece;

			if(testString.find("<|im_start") != std::string::npos ||
			   testString.find("<|im_end") != std::string::npos ||
			   testString.find("<|im_") != std::string::npos ||
			   testString.find("<|") != std::string::npos){
				OutputDebugStringA("[LOOP] Stop Reason: Pre-emptive virtual-join safety stop (ChatML tag structure detected!).\n");
				break;
			}

			m_output.append(buf, len);

			{
				std::lock_guard<std::mutex> lock(m_outputMutex);
				m_visibleOutput = m_output;
			}
		}

		gen.token[0] = tok;
		gen.pos[0] = m_nPast;
		gen.seq_id[0][0] = 0;
		gen.n_seq_id[0] = 1;
		gen.logits[0] = 1;
		gen.n_tokens = 1;

		if(llama_decode(m_ctx, gen) != 0){
			OutputDebugStringA("[LOOP] CRITICAL: llama_decode failed within generation loop!\n");
			break;
		}

		m_pastTokens.push_back(tok);
		++m_nPast;

		tok = llama_sampler_sample(m_sampler, m_ctx, -1);
		llama_sampler_accept(m_sampler, tok);

		char dbg_buf[256]{};
		int dbg_len = llama_token_to_piece(vocab, tok, dbg_buf, sizeof(dbg_buf), 0, false);
		if(dbg_len > 0){
			DBG_UTF8(("[Generated Piece]: " + std::string(dbg_buf, dbg_len) + "\n").c_str());
		}

		++loop;
		if(loop > 4096){
			OutputDebugStringA("[LOOP] Stop Reason: Safety break (loop count > 4096)\n");
			break;
		}
	}

	llama_batch_free(gen);

	{
		std::lock_guard<std::mutex> lock(m_outputMutex);
		m_visibleOutput = m_output;
		m_history.push_back({"assistant", m_visibleOutput});
	}

	// =====================================================
	// 8. 終了タグの手動補完完全同期
	// =====================================================
	if(!nativeEogDetected){
		OutputDebugStringA("[POST] Manually appending and synchronizing EOG token to KV cache...\n");
		std::string endTag = "<|im_end|>\n";
		std::vector<llama_token> endTokens(endTag.size() * 2);

		int nEnd = llama_tokenize(vocab, endTag.c_str(), (int)endTag.size(), endTokens.data(), (int)endTokens.size(), false, true);
		if(nEnd > 0){
			endTokens.resize(nEnd);
			llama_batch endBatch = llama_batch_init(nEnd, 0, 1);
			for(int i = 0; i < nEnd; ++i){
				endBatch.token[i] = endTokens[i];
				endBatch.pos[i] = m_nPast + i;
				endBatch.seq_id[i][0] = 0;
				endBatch.n_seq_id[i] = 1;
				endBatch.logits[i] = (i == nEnd - 1);
			}
			endBatch.n_tokens = nEnd;

			if(llama_decode(m_ctx, endBatch) == 0){
				for(int i = 0; i < nEnd; ++i){
					llama_sampler_accept(m_sampler, endTokens[i]);
				}
				m_pastTokens.insert(m_pastTokens.end(), endTokens.begin(), endTokens.end());
				m_nPast = (int)m_pastTokens.size();
				OutputDebugStringA("[POST] EOG synchronization success.\n");
			} else{
				OutputDebugStringA("[POST] ERROR: Failed to decode manual EOG token!\n");
			}
			llama_batch_free(endBatch);
		}
	}

	OutputDebugStringA("==================================================\n");
	OutputDebugStringA("LLAMAAgent: RunPromptInternal END\n");
	OutputDebugStringA(("Final Token Count (m_nPast): " + std::to_string(m_nPast) + "\n").c_str());
	OutputDebugStringA("==================================================\n");
}

// =====================================================
// SummarizeAndReset — 既存インフラ安全再利用版
// =====================================================
void LLAMAAgent::SummarizeAndReset(){
	OutputDebugStringA("[SUM] LLAMAAgent::SummarizeAndReset start\n");
	if(m_isSummarizing){
		OutputDebugStringA("[SUM] SummarizeAndReset: already summarizing\n");
		return;
	}
	m_isSummarizing = true;

	const llama_vocab* vocab = llama_model_get_vocab(m_model->m_model);
	if(!vocab){
		OutputDebugStringA("[SUM] SummarizeAndReset: no vocab\n");
		m_isSummarizing = false;
		return;
	}

	// 1. 手元のクリーンな歴史構造（m_history）から要約の原資テキストを組み立てる
	if(m_history.empty()){
		OutputDebugStringA("[SUM] SummarizeAndReset: m_history empty -> ResetContextUnlocked\n");
		ResetContextUnlocked();
		m_isSummarizing = false;
		return;
	}

	std::string conversationText;
	conversationText.reserve(4096);
	for(const auto& node : m_history){
		if(node.role == "system") continue;

		if(node.role == "user"){
			conversationText += "ユーザー: " + node.text + "\n";
		} else if(node.role == "assistant"){
			conversationText += "アシスタント: " + node.text + "\n";
		}
	}

	if(conversationText.empty()){
		OutputDebugStringA("[SUM] SummarizeAndReset: conversationText empty\n");
		ResetContextUnlocked();
		m_isSummarizing = false;
		return;
	}

	// QwenのChatML形式に準拠した、日本語要約プロンプトの構築
	std::string sumPrompt =
		"<|im_start|>system\n"
		"あなたは優秀な記憶管理アシスタントです。提供された会話の記録から、ユーザーの名前、重要な設定、現在話しているテーマ、ユーザーからの質問内容などの最重要事実を漏れなく抽出し、2-3文の簡潔な日本語で事実に基いて要約してください。<|im_end|>\n"
		"<|im_start|>user\n"
		"[会話の記録]\n" + conversationText + "\n"
		"この会話の要約を作成してください。<|im_end|>\n"
		"<|im_start|>assistant\n"
		"これまでの会話の要約：";

	// 要約プロンプトのトークナイズ
	std::vector<llama_token> sumTokens(sumPrompt.size() * 2 + 8);
	int n = llama_tokenize(vocab, sumPrompt.c_str(), (int)sumPrompt.size(), sumTokens.data(), (int)sumTokens.size(), true, true);
	if(n <= 0){
		OutputDebugStringA("[SUM] SummarizeAndReset: tokenize failed\n");
		m_isSummarizing = false;
		return;
	}
	sumTokens.resize(n);

	// 【安全な完全リセット】
	// 既存の関数を使ってコンテキストと内部状態を完全に初期化する
	ResetContextUnlocked();

	// 2. 専用の要約サンプラーを「リセット後のクリーンな状態」に対して再構築
	llama_sampler* sumSampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
	llama_sampler_chain_add(sumSampler, llama_sampler_init_penalties(128, m_config->repeat_penalty, 0.0f, 0.0f));
	llama_sampler_chain_add(sumSampler, llama_sampler_init_temp(0.1f)); // 決定論的な要約にするため低温度化
	llama_sampler_chain_add(sumSampler, llama_sampler_init_top_k((std::max)(5, (int)m_config->top_k / 4)));
	llama_sampler_chain_add(sumSampler, llama_sampler_init_top_p(0.9f, 1));
	llama_sampler_chain_add(sumSampler, llama_sampler_init_dist(static_cast<uint32_t>(
		std::chrono::system_clock::now().time_since_epoch().count()
		)));

	// メインコンテキストへ要約用プロンプトをデコード（バッチ処理）
	{
		const int batch_size = 32;
		int current_pos = 0;
		for(int offset = 0; offset < n; offset += batch_size){
			int sz = (std::min)(batch_size, n - offset);
			llama_batch b = llama_batch_init(sz, 0, 1);
			for(int i = 0; i < sz; ++i){
				b.token[i] = sumTokens[offset + i];
				b.pos[i] = current_pos + i;
				b.seq_id[i][0] = 0;
				b.n_seq_id[i] = 1;
				b.logits[i] = (i == sz - 1);
			}
			b.n_tokens = sz;
			int rc = llama_decode(m_ctx, b);
			if(rc != 0){
				OutputDebugStringA("[SUM] CRITICAL: SummarizeAndReset decode failed within batch loop!\n");
				llama_batch_free(b);
				llama_sampler_free(sumSampler);
				m_isSummarizing = false;
				return;
			}
			llama_batch_free(b);
			current_pos += sz;
		}
	}

	// サンプラーにプロンプトを食わせる
	for(int i = 0; i < n; ++i){
		llama_sampler_accept(sumSampler, sumTokens[i]);
	}

	// 3. 要約テキストの自動生成ループ
	m_summaryText.clear();
	const int max_summary_tokens = 256;
	int gen_pos = n;

	OutputDebugStringA("[SUM] Starting Summary Generation Loop...\n");

	for(int i = 0; i < max_summary_tokens; ++i){
		llama_token tok = sumSampler ? llama_sampler_sample(sumSampler, m_ctx, -1) : LLAMA_TOKEN_NULL;
		if(tok == LLAMA_TOKEN_NULL){
			OutputDebugStringA("[SUM] Loop Stop: LLAMA_TOKEN_NULL\n");
			break;
		}
		if(llama_vocab_is_eog(vocab, tok) || llama_vocab_get_attr(vocab, tok) == LLAMA_TOKEN_ATTR_CONTROL){
			OutputDebugStringA("[SUM] Loop Stop: EOG/Control token detected.\n");
			break;
		}

		llama_sampler_accept(sumSampler, tok);

		char piece[512]{};
		int len = llama_token_to_piece(vocab, tok, piece, sizeof(piece), 0, false);
		if(len > 0){
			m_summaryText.append(piece, len);
			DBG_UTF8(("[SUM Piece]: " + std::string(piece, len) + "\n").c_str());
		}

		// 次のトークン予測のためのデコード
		llama_batch g = llama_batch_init(1, 0, 1);
		g.token[0] = tok;
		g.pos[0] = gen_pos;
		g.seq_id[0][0] = 0;
		g.n_seq_id[0] = 1;
		g.logits[0] = 1;
		g.n_tokens = 1;

		int rc = llama_decode(m_ctx, g);
		llama_batch_free(g);
		if(rc != 0){
			OutputDebugStringA("[SUM] ERROR: decode failed during token generation\n");
			break;
		}
		++gen_pos;
	}

	llama_sampler_free(sumSampler);

	OutputDebugStringA(("[SUM] Final Generated Summary Text: " + m_summaryText + "\n").c_str());

	// =====================================================
	// 4. 要約生成完了後の本番用最終クリア
	// =====================================================
	std::string savedSummary = m_summaryText;

	// 要約生成で汚れたKVキャッシュを一度完全にクリーンにする
	ResetContextUnlocked();

	// 退避しておいた要約テキストを復元
	m_summaryText = savedSummary;

	m_history.clear();
	if(!m_summaryText.empty()){
		std::string combinedSystemPrompt = "";

		if(!m_config->system_prompt.empty()){
			combinedSystemPrompt += m_config->system_prompt + "\n\n";
		}

		combinedSystemPrompt += "[これまでの会話のコンテキスト要約]\n";
		combinedSystemPrompt += m_summaryText;

		m_history.push_back({"system", combinedSystemPrompt});
		OutputDebugStringA("[SUM] Successfully injected summary into new system history node.\n");
	} else{
		OutputDebugStringA("[SUM] WARNING: Summary text was empty. Resetting to default system prompt.\n");
		if(!m_config->system_prompt.empty()){
			m_history.push_back({"system", m_config->system_prompt});
		}
	}

	m_isSummarizing = false;
	OutputDebugStringA("[SUM] LLAMAAgent::SummarizeAndReset end\n");
}

// =====================================================
// Context reset
// =====================================================
void LLAMAAgent::ResetContext(){
	std::lock_guard<std::mutex> lock(m_mutex);
	m_state.store(State::Idle, std::memory_order_release);
	ResetContextUnlocked();
}

void LLAMAAgent::ResetContextUnlocked(){
	OutputDebugStringA("[INFRA] LLAMAAgent::ResetContextUnlocked start\n");

	if(m_ctx){
		llama_free(m_ctx);
		m_ctx = nullptr;
	}

	llama_context_params c = llama_context_default_params();
	c.n_ctx = m_config->n_ctx;
	c.n_threads = m_config->n_threads;

	m_ctx = llama_init_from_model(m_model->m_model, c);
	assert(m_ctx);

	if(m_sampler)
		llama_sampler_reset(m_sampler);

	m_pastTokens.clear();
	m_nPast = 0;
	m_history.clear();
	m_summaryText.clear();

	{
		std::lock_guard<std::mutex> o(m_outputMutex);
		m_output.clear();
		m_visibleOutput.clear();
	}

	OutputDebugStringA("[INFRA] LLAMAAgent::ResetContextUnlocked end\n");
}