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
	// [修正] プロンプト処理（バッチ）用のスレッド数が未設定だと
	// 実装依存のデフォルト値に頼ることになるため、明示的に揃えておく。
	c.n_threads_batch = m_config->n_threads;

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
	int penalty_window = -1;

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

	// [修正] サンプラーチェーンの順序を変更。
	// 以前は penalties -> temp -> top_k -> top_p -> dist の順で、
	// truncation（top_k/top_p による候補の絞り込み）より先に温度で分布をならしてしまっていた。
	// 一般的に推奨される順序は「まず候補を絞り、最後に温度→distで確率化する」なので、
	// top_k / top_p を temp より前に出すよう並び替えた。
	llama_sampler_chain_add(sampler, llama_sampler_init_top_k(m_config->top_k));
	llama_sampler_chain_add(sampler, llama_sampler_init_top_p(m_config->top_p, 1));
	llama_sampler_chain_add(sampler, llama_sampler_init_temp(m_config->temperature));
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
	, m_cancelRequested(false)
	, m_nPast(0)
	, m_isSummarizing(false){

	assert(m_model);
	assert(m_config);
	assert(m_config->IsValid());

	// [修正] assert は NDEBUG（Release）ビルドで無効化されるため、
	// 不正な Config のまま気付かず動き続けるのを避けるためログだけは残しておく。
	if(!m_config || !m_config->IsValid()){
		OutputDebugStringA("[LIFE] WARNING: AgentConfig::IsValid() == false. "
						   "Release ビルドでは assert が効かないため、想定外の挙動になる可能性があります。\n");
	}

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
		m_running.store(false, std::memory_order_release);
		m_cancelRequested.store(true, std::memory_order_release);
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
	m_cancelRequested.store(true, std::memory_order_release);

	// キューに積まれている未処理のジョブもすべて破棄する
	std::queue<std::string> emptyQueue;
	std::swap(m_jobQueue, emptyQueue);

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

		// ジョブ実行前にキャンセルフラグをリセット
		m_cancelRequested.store(false, std::memory_order_release);
		m_state.store(State::Running, std::memory_order_release);
		try{
			RunPromptInternal(prompt);
		} catch(...){
			OutputDebugStringA("[THREAD] CRITICAL ERROR: WorkerMain caught exception!\n");
		}

		// 停止要求で抜けてきたのか、正常終了かで状態を戻す
		if(m_cancelRequested.load(std::memory_order_acquire)){
			m_state.store(State::Idle, std::memory_order_release);
		} else{
			m_state.store(State::Idle, std::memory_order_release);
		}
	}

	m_state.store(State::Dead, std::memory_order_release);
	OutputDebugStringA("[THREAD] LLAMAAgent::WorkerMain end\n");
}

// =====================================================
// RunPromptInternal — 履歴一元管理版
// =====================================================
void LLAMAAgent::RunPromptInternal(const std::string& prompt, int retryDepth){
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

	// =====================================================
	// Cancel rollback snapshot (top-level prompt unit)
	// =====================================================
	struct CancelSnapshot{
		decltype(m_history) history;
		decltype(m_pastTokens) pastTokens;
		int nPast = 0;
		bool valid = false;
	};

	static thread_local CancelSnapshot s_cancelSnapshot;

	// outermost call only: keep the pre-call state
	if(retryDepth == 0 || !s_cancelSnapshot.valid){
		s_cancelSnapshot.history = m_history;
		s_cancelSnapshot.pastTokens = m_pastTokens;
		s_cancelSnapshot.nPast = m_nPast;
		s_cancelSnapshot.valid = true;
	}

	struct SnapshotScope{
		int retryDepth;
		~SnapshotScope(){
			if(retryDepth == 0){
				s_cancelSnapshot.valid = false;
			}
		}
	} snapshotScope{retryDepth};

	auto rollbackToSnapshot = [&](){
		if(!s_cancelSnapshot.valid){
			return;
		}

		OutputDebugStringA("[POST] Cancel/abort detected. Rolling back to snapshot...\n");

		const auto historySnapshot = s_cancelSnapshot.history;
		const auto pastTokensSnapshot = s_cancelSnapshot.pastTokens;
		const int nPastSnapshot = s_cancelSnapshot.nPast;

		// Reset context and sampler to a clean state first
		ResetContextUnlocked();

		// Restore conversation history
		m_history = historySnapshot;

		// Rebuild KV cache + sampler state from token snapshot
		if(!pastTokensSnapshot.empty() && m_ctx && m_sampler){
			uint32_t n_batch = llama_n_batch(m_ctx);
			if(n_batch == 0) n_batch = 512;

			int current_pos = 0;
			const int total = static_cast<int>(pastTokensSnapshot.size());

			for(int offset = 0; offset < total; offset += static_cast<int>(n_batch)){
				int chunk_size = (std::min)(static_cast<int>(n_batch), total - offset);

				llama_batch batch = llama_batch_init(chunk_size, 0, 1);
				for(int i = 0; i < chunk_size; ++i){
					int idx = offset + i;
					batch.token[i] = pastTokensSnapshot[idx];
					batch.pos[i] = current_pos + i;
					batch.seq_id[i][0] = 0;
					batch.n_seq_id[i] = 1;
					batch.logits[i] = (idx == total - 1);
				}
				batch.n_tokens = chunk_size;

				if(llama_decode(m_ctx, batch) != 0){
					OutputDebugStringA("[POST] WARNING: rollback replay decode failed. Context is left reset.\n");
					llama_batch_free(batch);
					break;
				}

				llama_batch_free(batch);
				current_pos += chunk_size;
			}

			for(const auto& tok : pastTokensSnapshot){
				llama_sampler_accept(m_sampler, tok);
			}
		}

		m_pastTokens = pastTokensSnapshot;
		m_nPast = nPastSnapshot;

		{
			std::lock_guard<std::mutex> lock(m_outputMutex);
			m_output.clear();
			m_visibleOutput.clear();
		}

		s_cancelSnapshot.valid = false;
		};

	if(m_cancelRequested.load(std::memory_order_acquire)){
		rollbackToSnapshot();
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

	// [修正] テンプレートを nullptr 固定で呼んでいたのをやめ、モデルに埋め込まれた
	// 本来の chat_template（GGUFメタデータ）を明示的に取得して使うようにした。
	// これで Qwen 以外（ChatML 以外のテンプレート）のモデルに差し替えても
	// 正しいフォーマットで整形されるようになる。
	// [再修正] このプロジェクトの llama.h の llama_chat_apply_template は
	// model 引数を取らない (tmpl, chat, n_msg, add_ass, buf, length) の6引数版だったため、
	// tmpl だけを渡す形に修正。tmpl を渡さないとモデル由来のテンプレートを参照する手段が
	// 無くなるので、事前に llama_model_chat_template() で必ず取得しておく必要がある。
	const char* tmpl = llama_model_chat_template(m_model->m_model, nullptr);

	int32_t buf_size = 4096;
	std::string diffFormatted;
	diffFormatted.resize(buf_size);

	// テンプレート適用時にアシスタントタグ付与フラグ (add_ass) を常に true にする
	bool add_ass = true;
	int32_t n = llama_chat_apply_template(
		tmpl,
		msgs.data(),
		msgs.size(),
		add_ass,
		diffFormatted.data(),
		buf_size
	);

	if(n < 0){
		OutputDebugStringA("[PIPELINE] ERROR: llama_chat_apply_template failed!\n");
		rollbackToSnapshot();
		return;
	}
	if(n > buf_size){
		diffFormatted.resize(n);
		n = llama_chat_apply_template(tmpl, msgs.data(), msgs.size(), add_ass, diffFormatted.data(), n);
		if(n < 0){
			rollbackToSnapshot();
			return;
		}
	}
	diffFormatted.resize(n);

	// [修正] 以前はここで「末尾に <|im_start|>assistant が付いているか」を手動で
	// 文字列チェックして無ければ付与する、というChatML専用の後処理をしていた。
	// add_ass=true を正しいテンプレートに対して渡している以上、この後処理は不要
	// （かつ ChatML 以外のモデルでは逆に害になる）ので削除した。

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
		rollbackToSnapshot();
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

		// [修正] 要約してもなおオーバーフローが解消しない異常系で
		// RunPromptInternal が無限に自分自身を再帰呼び出しするのを防ぐガード。
		if(retryDepth >= 1){
			OutputDebugStringA("[OVERFLOW] CRITICAL: Overflow persists even after summarization. Aborting to avoid infinite recursion.\n");
			rollbackToSnapshot();
			return;
		}

		SummarizeAndReset();

		OutputDebugStringA("[OVERFLOW] Re-trying RunPromptInternal with clean context summary baton.\n");
		RunPromptInternal(prompt, retryDepth + 1);
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

	// [修正] 以前はキャッシュ不一致（要約で履歴が変わった時など）が起きるたびに
	// ResetContextUnlocked() でコンテキストを丸ごと作り直していた。
	// 現行 API の llama_memory_seq_rm を使えば、一致した部分（reuse_count まで）の
	// KVキャッシュは残したまま、ズレた末尾分だけをピンポイントで破棄できる。
	if(reuse_count < m_nPast){
		llama_memory_t mem = llama_get_memory(m_ctx);
		if(mem){
			// seq_id=0 の reuse_count 番目以降（末尾まで）を破棄
			llama_memory_seq_rm(mem, 0, reuse_count, -1);
		} else{
			OutputDebugStringA("[PIPELINE] WARNING: llama_get_memory returned null, falling back to full reset.\n");
			ResetContextUnlocked();
			reuse_count = 0;
		}
		m_pastTokens.resize(reuse_count);
		m_nPast = reuse_count;
	}

	int tokens_to_decode = ntok - reuse_count;

	OutputDebugStringA(("Reusing " + std::to_string(reuse_count) + " tokens from KV cache.\n").c_str());
	OutputDebugStringA(("Decoding " + std::to_string(tokens_to_decode) + " new tokens.\n").c_str());

	if(tokens_to_decode > 0){
		// llama_context から現在の最大バッチサイズを取得 (llama.cpp の仕様による)
		// 取得できない場合はデフォルトの 512 を安全な上限とする
		uint32_t n_batch = llama_n_batch(m_ctx);
		if(n_batch == 0) n_batch = 512;

		int current_pos = reuse_count;
		int tokens_processed = 0;

		// 巨大なプロンプトを n_batch ごとに分割してデコードする安全なループ
		while(tokens_processed < tokens_to_decode && m_running.load() && !m_cancelRequested.load(std::memory_order_acquire)){
			int chunk_size = (std::min)((int)n_batch, tokens_to_decode - tokens_processed);

			llama_batch batch = llama_batch_init(chunk_size, 0, 1);
			for(int i = 0; i < chunk_size; i++){
				int token_index = reuse_count + tokens_processed + i;
				batch.token[i] = tokens[token_index];
				batch.pos[i] = current_pos + i;
				batch.seq_id[i][0] = 0;
				batch.n_seq_id[i] = 1;
				// 最後のチャンクの、最後のトークンでのみ logits を計算する
				batch.logits[i] = (tokens_processed + i == tokens_to_decode - 1);
			}
			batch.n_tokens = chunk_size;

			if(llama_decode(m_ctx, batch) != 0){
				llama_batch_free(batch);
				OutputDebugStringA("[PIPELINE] CRITICAL: llama_decode for prompt failed!\n");
				rollbackToSnapshot();
				return;
			}
			llama_batch_free(batch);

			tokens_processed += chunk_size;
			current_pos += chunk_size;
		}

		// [修正] 以前は毎ターン llama_sampler_reset() してから m_pastTokens 全体を
		// accept() し直していたため、会話が伸びるほど O(n) のコストが線形に積み上がっていた。
		// KVキャッシュ側を差分更新するようにした今は、サンプラー側も
		// 「今回新しく文脈に追加された分（reuse_count 以降）」だけを accept() すれば十分。
		// 既に生成済みだった分は、生成ループの中で逐次 accept() 済みなので再送不要。
		for(int i = reuse_count; i < ntok; ++i){
			llama_sampler_accept(m_sampler, tokens[i]);
		}
	}

	m_pastTokens = tokens;
	m_nPast = (int)m_pastTokens.size();

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

	// [修正] 以前はハードコードされた 4096 ループのみが生成数の上限で、
	// AgentConfig::max_tokens は完全に無視されていた。ここで実際に使うようにする。
	const int genTokenLimit = (m_config->max_tokens > 0)
		? (int)m_config->max_tokens
		: 2048;

	// [修正] m_nPast (int) と m_config->n_ctx (uint32_t) の符号なし比較で
	// C4018 警告が出ていたため、明示的にキャストして揃える。
	// m_cancelRequested も条件に追加し、即座に抜けられるようにする。
	while(m_running.load() && !m_cancelRequested.load(std::memory_order_acquire) && m_nPast < static_cast<int>(m_config->n_ctx) && loop < genTokenLimit){

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
			// [注記] このチェックは ChatML 専用（<|im_start| 等）。
			// 別系統のチャットテンプレートのモデルに差し替えた場合は、
			// このタグ漏れ検知も合わせて見直すこと。
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
			llama_batch_free(gen);
			rollbackToSnapshot();
			return;
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
		// [修正] 以前あったハードコード 4096 の安全ブレークは、
		// while条件の genTokenLimit に統合したので撤去した。
	}

	llama_batch_free(gen);

	{
		std::lock_guard<std::mutex> lock(m_outputMutex);
		m_visibleOutput = m_output;

		// キャンセルされていなければ、履歴に残す
		if(!m_cancelRequested.load(std::memory_order_acquire)){
			m_history.push_back({"assistant", m_visibleOutput});
		}
	}

	// キャンセルされた場合はタグ補完をスキップする
	if(m_cancelRequested.load(std::memory_order_acquire)){
		OutputDebugStringA("[POST] Skip EOG append due to cancelation.\n");
		rollbackToSnapshot();
		return;
	}

	// =====================================================
	// 8. 終了タグの手動補完完全同期
	// =====================================================
	// [注記] ここも ChatML（<|im_end|>）専用の後処理。モデルを差し替える場合は要見直し。
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

	// [修正] 以前は ChatML のタグ（<|im_start|> 等）を直書きしていたが、
	// メインの会話パイプラインと同じく、モデル本来の chat_template を使うように統一した。
	// これで本筋とここの整形ロジックが二重管理にならず、モデルを差し替えても破綻しない。
	std::string sysPrompt =
		"あなたは優秀な記憶管理アシスタントです。提供された会話の記録から、"
		"ユーザーの名前、重要な設定、現在話しているテーマ、ユーザーからの質問内容などの"
		"最重要事実を漏れなく抽出し、前置きなしで2-3文の簡潔な日本語で事実に基いて要約してください。";

	std::string userPrompt =
		"[会話の記録]\n" + conversationText + "\nこの会話の要約を作成してください。";

	std::vector<llama_chat_message> sumMsgs;
	sumMsgs.push_back({"system", sysPrompt.c_str()});
	sumMsgs.push_back({"user", userPrompt.c_str()});

	const char* tmpl = llama_model_chat_template(m_model->m_model, nullptr);

	int32_t sumBufSize = (int32_t)((sysPrompt.size() + userPrompt.size()) * 2) + 256;
	std::string sumPrompt;
	sumPrompt.resize(sumBufSize);

	// [修正] こちらも model 引数を取らない6引数版のシグネチャに合わせて tmpl のみ渡す。
	int32_t sn = llama_chat_apply_template(
		tmpl, sumMsgs.data(), sumMsgs.size(), true,
		sumPrompt.data(), sumBufSize
	);
	if(sn < 0){
		OutputDebugStringA("[SUM] SummarizeAndReset: llama_chat_apply_template failed\n");
		m_isSummarizing = false;
		return;
	}
	if(sn > sumBufSize){
		sumPrompt.resize(sn);
		sn = llama_chat_apply_template(tmpl, sumMsgs.data(), sumMsgs.size(), true, sumPrompt.data(), sn);
		if(sn < 0){
			m_isSummarizing = false;
			return;
		}
	}
	sumPrompt.resize(sn);

	// 要約プロンプトのトークナイズ
	std::vector<llama_token> sumTokens(sumPrompt.size() * 2 + 8);
	int n = llama_tokenize(vocab, sumPrompt.c_str(), (int)sumPrompt.size(), sumTokens.data(), (int)sumTokens.size(), true, true);
	if(n <= 0){
		OutputDebugStringA("[SUM] SummarizeAndReset: tokenize failed\n");
		m_isSummarizing = false;
		return;
	}
	sumTokens.resize(n);

	// 【コンテキストの一時クリア】
	// [修正] ResetContextUnlocked が llama_memory_clear ベースの軽量処理になったので、
	// 以前ほど「丸ごと作り直す」重さは無くなっている。
	ResetContextUnlocked();

	// 2. 専用の要約サンプラーを「リセット後のクリーンな状態」に対して再構築
	llama_sampler* sumSampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
	llama_sampler_chain_add(sumSampler, llama_sampler_init_penalties(128, m_config->repeat_penalty, 0.0f, 0.0f));
	// [修正] ここもメインのサンプラーと同様に top_k/top_p を temp より前に並べる。
	llama_sampler_chain_add(sumSampler, llama_sampler_init_top_k((std::max)(5, (int)m_config->top_k / 4)));
	llama_sampler_chain_add(sumSampler, llama_sampler_init_top_p(0.9f, 1));
	llama_sampler_chain_add(sumSampler, llama_sampler_init_temp(0.1f)); // 決定論的な要約にするため低温度化
	llama_sampler_chain_add(sumSampler, llama_sampler_init_dist(static_cast<uint32_t>(
		std::chrono::system_clock::now().time_since_epoch().count()
		)));

	// メインコンテキストへ要約用プロンプトをデコード（バッチ処理）
	{
		const int batch_size = 32;
		int current_pos = 0;
		for(int offset = 0; offset < n && m_running.load() && !m_cancelRequested.load(std::memory_order_acquire); offset += batch_size){
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
	for(int i = 0; i < n && m_running.load() && !m_cancelRequested.load(std::memory_order_acquire); ++i){
		llama_sampler_accept(sumSampler, sumTokens[i]);
	}

	// 3. 要約テキストの自動生成ループ
	m_summaryText.clear();
	const int max_summary_tokens = 256;
	int gen_pos = n;

	OutputDebugStringA("[SUM] Starting Summary Generation Loop...\n");

	for(int i = 0; i < max_summary_tokens && m_running.load() && !m_cancelRequested.load(std::memory_order_acquire); ++i){
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

	// キャンセルされた場合は要約作業を破棄して戻る
	if(m_cancelRequested.load(std::memory_order_acquire)){
		OutputDebugStringA("[SUM] Summarize canceled. Discarding work.\n");
		m_isSummarizing = false;
		return;
	}

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

	// [修正] 以前はここで llama_free() + llama_init_from_model() により
	// llama_context をまるごと作り直していた。これは計算グラフの再構築や
	// 各種バッファの再アロケーションを伴う重い処理で、「会話履歴をクリアしたいだけ」
	// の用途には過剰だった。
	//
	// 現行の llama.cpp は KV キャッシュ管理が llama_memory_* API（unified memory
	// abstraction）に統合されているので、メモリの中身だけをクリアすれば
	// 同じ目的をはるかに低コストで達成できる。
	//
	// 注意: お使いの llama.cpp のバージョンが古く llama_memory_* が存在しない場合は、
	// 代わりに旧APIの llama_kv_cache_clear(m_ctx) を使うこと（ctx を直接渡す版）。
	if(m_ctx){
		llama_memory_t mem = llama_get_memory(m_ctx);
		if(mem){
			llama_memory_clear(mem, true);
		} else{
			OutputDebugStringA("[INFRA] WARNING: llama_get_memory returned null during reset.\n");
		}
	}

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