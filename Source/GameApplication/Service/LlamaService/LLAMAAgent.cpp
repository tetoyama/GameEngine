#include "LLAMAAgent.h"

#include <cassert>
#include <chrono>

#include "Backends/llama/llama.h"
#include "Resources/Data/llamaModelData.h"
#include "AgentConfig.h"

// ============================
// ctor
// ============================
LLAMAAgent::LLAMAAgent(
	std::shared_ptr<LLAMAModelData> model,
	std::shared_ptr<const AgentConfig> config
)
	: m_model(std::move(model))
	, m_config(std::move(config)){
	assert(m_model);
	assert(m_config);
	assert(m_config->IsValid());

	llama_context_params c = llama_context_default_params();
	c.n_ctx = m_config->n_ctx;
	c.n_threads = m_config->n_threads;

	m_ctx = llama_init_from_model(m_model->m_model, c);
	assert(m_ctx);

	llama_sampler_chain_params sp = llama_sampler_chain_default_params();
	m_sampler = llama_sampler_chain_init(sp);

	llama_sampler_chain_add(m_sampler, llama_sampler_init_top_k(m_config->top_k));
	llama_sampler_chain_add(m_sampler, llama_sampler_init_top_p(m_config->top_p, 1));
	llama_sampler_chain_add(m_sampler, llama_sampler_init_temp(m_config->temperature));
	llama_sampler_chain_add(
		m_sampler,
		llama_sampler_init_penalties(
			128,
			m_config->repeat_penalty,
			0.0f,
			0.0f
		)
	);
	llama_sampler_chain_add(
		m_sampler,
		llama_sampler_init_dist(
			static_cast<uint32_t>(
				std::chrono::system_clock::now().time_since_epoch().count()
				)
		)
	);

	m_thread = std::thread(&LLAMAAgent::WorkerMain, this);
}

// ============================
// dtor
// ============================
LLAMAAgent::~LLAMAAgent(){
	m_running.store(false);
	m_cv.notify_all();

	if(m_thread.joinable())
		m_thread.join();

	if(m_sampler)
		llama_sampler_free(m_sampler);

	if(m_ctx)
		llama_free(m_ctx);

	m_state.store(State::Dead);
}

// ============================
// Public API
// ============================
void LLAMAAgent::RunAsync(const std::string& prompt){
	if(prompt.empty())
		return;

	std::lock_guard<std::mutex> lock(m_mutex);
	m_jobQueue.push(prompt);
	m_cv.notify_one();
}

void LLAMAAgent::Stop(){
	m_state.store(State::Stopping);
	m_running.store(false);
	m_cv.notify_all();
}

std::string LLAMAAgent::GetOutput() const{
	std::lock_guard<std::mutex> lock(m_outputMutex);
	return m_output;
}

// ============================
// Worker
// ============================
void LLAMAAgent::WorkerMain(){
	while(m_running.load()){
		std::string prompt;

		{
			std::unique_lock<std::mutex> lock(m_mutex);
			m_cv.wait(lock, [&]{
				return !m_jobQueue.empty() || !m_running.load();
					  });

			if(!m_running.load())
				break;

			prompt = std::move(m_jobQueue.front());
			m_jobQueue.pop();
		}

		m_state.store(State::Running);
		RunPromptInternal(prompt);
		m_state.store(State::Idle);
	}

	m_state.store(State::Dead);
}

// ============================
// Prompt Execution (async only)
// ============================
void LLAMAAgent::RunPromptInternal(const std::string& prompt){
	const llama_vocab* vocab = llama_model_get_vocab(m_model->m_model);

	// フルプロンプト生成
	std::string fullPrompt;
	if(!m_config->system_prompt.empty())
		fullPrompt += m_config->system_prompt + "\n";
	if(!m_summaryText.empty())
		fullPrompt += "[Conversation Summary]\n" + m_summaryText + "\n";
	fullPrompt += "<|user|>\n" + prompt + "\n<|assistant|>\n";

	// トークン化
	std::vector<llama_token> tokens(fullPrompt.size() * 2 + 16);
	int n = llama_tokenize(
		vocab,
		fullPrompt.c_str(),
		static_cast<int>(fullPrompt.size()),
		tokens.data(),
		static_cast<int>(tokens.size()),
		true,
		false
	);
	if(n <= 0)
		return;
	tokens.resize(n);

	EnsureContextFits(tokens.size());

	// llama_batch 1回で処理
	llama_batch batch = llama_batch_init(n, 0, 1);
	for(int i = 0; i < n; ++i){
		batch.token[i] = tokens[i];
		batch.pos[i] = m_nPast + i;
		batch.seq_id[i][0] = 0;
		batch.n_seq_id[i] = 1;
		batch.logits[i] = 0;
	}
	batch.n_tokens = n;
	batch.logits[n - 1] = 1;

	llama_decode(m_ctx, batch);
	llama_batch_free(batch);

	m_nPast += n;
	m_pastTokens.insert(m_pastTokens.end(), tokens.begin(), tokens.end());

	// 出力初期化
	{
		std::lock_guard<std::mutex> lock(m_outputMutex);
		m_output.clear();
	}

	// トークン生成ループ
	llama_batch tokenBatch = llama_batch_init(1, 0, 1); // 再利用用
	while(m_running.load() && m_state.load() != State::Stopping){
		llama_token tok = llama_sampler_sample(m_sampler, m_ctx, -1);
		if(tok == LLAMA_TOKEN_NULL || llama_vocab_is_eog(vocab, tok))
			break;
		if(llama_vocab_is_control(vocab, tok))
			continue;

		char buf[64];
		int len = llama_token_to_piece(vocab, tok, buf, sizeof(buf), 0, false);
		if(len > 0){
			std::lock_guard<std::mutex> lock(m_outputMutex);
			m_output.append(buf, len);
		}

		// バッチ再利用
		tokenBatch.token[0] = tok;
		tokenBatch.pos[0] = m_nPast++;
		tokenBatch.seq_id[0][0] = 0;
		tokenBatch.n_seq_id[0] = 1;
		tokenBatch.n_tokens = 1;
		tokenBatch.logits[0] = 1;

		llama_decode(m_ctx, tokenBatch);
		m_pastTokens.push_back(tok);
	}
	llama_batch_free(tokenBatch);
}

// ============================
// Context helpers
// ============================
void LLAMAAgent::EnsureContextFits(size_t incomingTokens){
	size_t reserve = m_config->max_tokens + 32;
	if(m_nPast + incomingTokens + reserve < m_config->n_ctx)
		return;

	SummarizeAndReset();

}

void LLAMAAgent::ResetContext(){
	if(m_ctx)
		llama_free(m_ctx);

	llama_context_params c = llama_context_default_params();
	c.n_ctx = m_config->n_ctx;
	c.n_threads = m_config->n_threads;

	m_ctx = llama_init_from_model(m_model->m_model, c);
	assert(m_ctx);

	if(m_sampler)
		llama_sampler_free(m_sampler);
	llama_sampler_chain_params sp = llama_sampler_chain_default_params();
	m_sampler = llama_sampler_chain_init(sp);

	llama_sampler_chain_add(m_sampler, llama_sampler_init_top_k(m_config->top_k));
	llama_sampler_chain_add(m_sampler, llama_sampler_init_top_p(m_config->top_p, 1));
	llama_sampler_chain_add(m_sampler, llama_sampler_init_temp(m_config->temperature));

	m_pastTokens.clear();
	m_nPast = 0;
}

std::string LLAMAAgent::DecodeTokensToString() const{
	const llama_vocab* vocab = llama_model_get_vocab(m_model->m_model);

	std::string result;
	char buf[64];
	for(auto tok : m_pastTokens){
		int len = llama_token_to_piece(vocab, tok, buf, sizeof(buf), 0, false);
		if(len > 0)
			result.append(buf, len);
	}
	return result;
}

// ============================
// Summary Agent
// ============================
void LLAMAAgent::SetSummaryAgent(std::shared_ptr<LLAMAAgent> agent){
	std::lock_guard<std::mutex> lock(m_summaryMutex);
	m_summaryAgent = std::move(agent);
	m_summaryCv.notify_all();
}

// ============================
// GenerateSync
// ============================
std::string LLAMAAgent::GenerateSync(const std::string& prompt){
	const llama_vocab* vocab = llama_model_get_vocab(m_model->m_model);

	std::vector<llama_token> tokens(prompt.size() * 2 + 16);
	int n = llama_tokenize(
		vocab,
		prompt.c_str(),
		static_cast<int>(prompt.size()),
		tokens.data(),
		static_cast<int>(tokens.size()),
		true,
		false
	);
	if(n <= 0)
		return {};

	tokens.resize(n);

	llama_batch batch = llama_batch_init(n, 0, 1);
	for(int i = 0; i < n; ++i){
		batch.token[i] = tokens[i];
		batch.pos[i] = i;
		batch.seq_id[i][0] = 0;
		batch.n_seq_id[i] = 1;
		batch.logits[i] = 0;
	}
	batch.n_tokens = n;
	batch.logits[n - 1] = 1;

	llama_decode(m_ctx, batch);
	llama_batch_free(batch);

	std::string output;

	for(unsigned int i = 0; i < m_config->max_tokens; ++i){
		llama_token tok = llama_sampler_sample(m_sampler, m_ctx, -1);
		if(llama_vocab_is_eog(vocab, tok))
			break;

		char buf[64];
		int len = llama_token_to_piece(vocab, tok, buf, sizeof(buf), 0, false);
		if(len > 0){
			output.append(buf, len);
		}

		llama_batch b = llama_batch_init(1, 0, 1);
		b.token[0] = tok;
		b.pos[0] = n + i;
		b.seq_id[0][0] = 0;
		b.n_seq_id[0] = 1;
		b.n_tokens = 1;
		b.logits[0] = 1;

		llama_decode(m_ctx, b);
		llama_batch_free(b);
	}

	return output;
}

// ============================
// SummarizeAndReset
// ============================
void LLAMAAgent::SummarizeAndReset(){
	bool expected = false;
	if(!m_isSummarizing.compare_exchange_strong(expected, true))
		return;

	std::string history = DecodeTokensToString();
	if(!history.empty()){

		if(m_summaryAgent){
			std::string summary = m_summaryAgent->GenerateSync(
				"Summarize the following conversation briefly:\n" + history
			);

			if(!summary.empty()){
				std::lock_guard<std::mutex> lock(m_outputMutex);
				m_summaryText = summary;
				m_pastTokens.clear();
				m_nPast = 0;
			}
		}
	}

	m_isSummarizing.store(false);
}
