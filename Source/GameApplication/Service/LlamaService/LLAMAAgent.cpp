#include "LLAMAAgent.h"

#include <cassert>
#include <chrono>
#include <vector>
#include <windows.h>

#include "Backends/llama/llama.h"
#include "Resources/Data/llamaModelData.h"
#include "AgentConfig.h"

// =====================================================
// 内部ヘルパー（生成経路を完全統一）
// =====================================================
llama_context* LLAMAAgent::CreateContext() const {
    assert(m_model);
    assert(m_config);

    OutputDebugStringA("LLAMAAgent::CreateContext start\n");

    llama_context_params c = llama_context_default_params();
    c.n_ctx = m_config->n_ctx;
    c.n_threads = m_config->n_threads;

    llama_context* ctx = llama_init_from_model(m_model->m_model, c);
    assert(ctx && "llama_init_from_model failed");

    OutputDebugStringA("LLAMAAgent::CreateContext end\n");
    return ctx;
}

llama_sampler* LLAMAAgent::CreateSampler() const {
    assert(m_config);

    OutputDebugStringA("LLAMAAgent::CreateSampler start\n");

    llama_sampler_chain_params sp = llama_sampler_chain_default_params();
    llama_sampler* sampler = llama_sampler_chain_init(sp);
    assert(sampler);

    llama_sampler_chain_add(sampler, llama_sampler_init_top_k(m_config->top_k));
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(m_config->top_p, 1));
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(m_config->temperature));
    llama_sampler_chain_add(sampler, llama_sampler_init_penalties(128, m_config->repeat_penalty, 0.0f, 0.0f));
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(static_cast<uint32_t>(
        std::chrono::system_clock::now().time_since_epoch().count()
        )));

    OutputDebugStringA("LLAMAAgent::CreateSampler end\n");
    return sampler;
}

// =====================================================
// ctor
// =====================================================
LLAMAAgent::LLAMAAgent(std::shared_ptr<LLAMAModelData> model,
                       std::shared_ptr<const AgentConfig> config)
    : m_model(std::move(model))
    , m_config(std::move(config))
    , m_running(true)
    , m_nPast(0)
    , m_isSummarizing(false) {

    assert(m_model);
    assert(m_config);
    assert(m_config->IsValid());

    OutputDebugStringA("LLAMAAgent ctor start\n");

    m_ctx = CreateContext();
    m_sampler = CreateSampler();

    m_thread = std::thread(&LLAMAAgent::WorkerMain, this);

    OutputDebugStringA("LLAMAAgent ctor end\n");
}

// =====================================================
// dtor
// =====================================================
LLAMAAgent::~LLAMAAgent() {
    OutputDebugStringA("LLAMAAgent dtor start\n");

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_running.store(false);
    }
    m_cv.notify_all();

    if (m_thread.joinable())
        m_thread.join();

    if (m_sampler) {
        llama_sampler_free(m_sampler);
        m_sampler = nullptr;
    }
    if (m_ctx) {
        llama_free(m_ctx);
        m_ctx = nullptr;
    }

    m_state.store(State::Dead);

    OutputDebugStringA("LLAMAAgent dtor end\n");
}

// =====================================================
// Stop
// =====================================================
void LLAMAAgent::Stop() {
    OutputDebugStringA("LLAMAAgent::Stop called\n");

    std::lock_guard<std::mutex> lock(m_mutex);
    m_state.store(State::Stopping, std::memory_order_release);
    m_running.store(false, std::memory_order_release);
    m_cv.notify_all();
}

// =====================================================
// Public API
// =====================================================
void LLAMAAgent::RunAsync(const std::string& prompt) {
    if (prompt.empty()) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_jobQueue.push(prompt);
    m_cv.notify_one();
}

std::string LLAMAAgent::GetOutput() const {
    std::lock_guard<std::mutex> lock(m_outputMutex);
    return m_output;
}

int LLAMAAgent::GetTokenCount() const noexcept {
    return m_nPast;
}

int LLAMAAgent::GetMaxTokenCount() const noexcept {
    return m_config->n_ctx;
}

float LLAMAAgent::GetTokenUsageRate() const noexcept {
    return m_config->n_ctx > 0 ? static_cast<float>(m_nPast) / m_config->n_ctx : 0.0f;
}

// =====================================================
// Worker
// =====================================================
void LLAMAAgent::WorkerMain() {
    OutputDebugStringA("LLAMAAgent::WorkerMain start\n");

    while (true) {
        std::string prompt;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [&] { return !m_jobQueue.empty() || !m_running.load(); });

            if (!m_running.load() && m_jobQueue.empty())
                break;

            prompt = std::move(m_jobQueue.front());
            m_jobQueue.pop();
        }

        m_state.store(State::Running, std::memory_order_release);
        try {
            RunPromptInternal(prompt);
        }
        catch (...) {
            OutputDebugStringA("LLAMAAgent::WorkerMain caught exception\n");
        }
        m_state.store(State::Idle, std::memory_order_release);
    }

    m_state.store(State::Dead, std::memory_order_release);
    OutputDebugStringA("LLAMAAgent::WorkerMain end\n");
}

// =====================================================
// Prompt Execution
// =====================================================
void LLAMAAgent::RunPromptInternal(const std::string& prompt) {
    OutputDebugStringA("LLAMAAgent::RunPromptInternal start\n");

    std::unique_lock<std::mutex> lock(m_mutex);
    const llama_vocab* vocab = llama_model_get_vocab(m_model->m_model);
    if (!vocab) return;

    // ---- fullPrompt 構築 ----
    std::string fullPrompt;
    if (!m_config->system_prompt.empty())
        fullPrompt += m_config->system_prompt + "\n";

    if (!m_summaryText.empty())
        fullPrompt += "[Conversation Summary]\n" + m_summaryText + "\n";

    fullPrompt += "<|user|>\n" + prompt + "\n<|assistant|>\n";

    // ---- トークン化 ----
    std::vector<llama_token> tokens(fullPrompt.size() * 2 + 8);
    int n = llama_tokenize(vocab, fullPrompt.c_str(), (int)fullPrompt.size(),
                           tokens.data(), (int)tokens.size(), true, false);
    if (n <= 0) return;
    tokens.resize(n);

    // ---- n_ctx 超過対策 ----
    const int safety_margin = 16;
    int max_tokens_allowed = m_config->n_ctx - safety_margin;

    if (m_nPast + n > max_tokens_allowed) {
        OutputDebugStringA("LLAMAAgent::RunPromptInternal n_ctx 超過、SummarizeAndReset 呼び出し\n");

        // 要約可能ならまず要約
        if (!m_isSummarizing)
            SummarizeAndReset();

        // それでも超過する場合は古い履歴を削除
        if (m_nPast + n > max_tokens_allowed) {
            int over = (m_nPast + n) - max_tokens_allowed;
            if (over < (int)m_pastTokens.size()) {
                m_pastTokens.erase(m_pastTokens.begin(), m_pastTokens.begin() + over);
                m_nPast -= over;
                OutputDebugStringA("LLAMAAgent::RunPromptInternal 古いトークンを削除\n");
            } else {
                m_pastTokens.clear();
                m_nPast = 0;
                OutputDebugStringA("LLAMAAgent::RunPromptInternal 全トークンをクリア\n");
            }
        }
    }

    // ---- full sequence ----
    std::vector<llama_token> fullTokens;
    fullTokens.insert(fullTokens.end(), m_pastTokens.begin(), m_pastTokens.end());
    fullTokens.insert(fullTokens.end(), tokens.begin(), tokens.end());

    // ---- 過去トークンをバッチで decode ----
    const int batch_size = 64;
    int pos = 0;
    for (size_t offset = 0; offset < fullTokens.size(); offset += batch_size) {
        int sz = (int)(std::min)(batch_size, (int)fullTokens.size() - (int)offset);
        llama_batch b = llama_batch_init(sz, 0, 1);
        for (int i = 0; i < sz; ++i) {
            b.token[i] = fullTokens[offset + i];
            b.pos[i] = pos + i;
            b.seq_id[i][0] = 0;
            b.n_seq_id[i] = 1;
            b.logits[i] = (i == sz - 1);
        }
        b.n_tokens = sz;
        llama_decode(m_ctx, b);
        llama_batch_free(b);
    }
    pos += (int)fullTokens.size();

    // ---- sampler 更新 ----
    if (m_sampler) {
        llama_sampler_reset(m_sampler);
        for (auto t : fullTokens)
            llama_sampler_accept(m_sampler, t);
    }

    m_pastTokens = fullTokens;
    m_nPast = (int)fullTokens.size();

    // ---- 出力リセット ----
    {
        std::lock_guard<std::mutex> o(m_outputMutex);
        m_output.clear();
    }

    // ---- 生成 ----
    llama_batch gen = llama_batch_init(1, 0, 1);
    while (m_running.load() && m_nPast < max_tokens_allowed) {
        llama_token tok = llama_sampler_sample(m_sampler, m_ctx, -1);
        if (tok == LLAMA_TOKEN_NULL || llama_vocab_is_eog(vocab, tok))
            break;

        llama_sampler_accept(m_sampler, tok);

        if (!llama_vocab_is_control(vocab, tok)) {
            char buf[256];
            int len = llama_token_to_piece(vocab, tok, buf, sizeof(buf), 0, false);
            if (len > 0) {
                std::lock_guard<std::mutex> o(m_outputMutex);
                m_output.append(buf, len);
            }
        }

        gen.token[0] = tok;
        gen.pos[0] = m_nPast++;
        gen.seq_id[0][0] = 0;
        gen.n_seq_id[0] = 1;
        gen.logits[0] = 1;
        gen.n_tokens = 1;
        llama_decode(m_ctx, gen);

        m_pastTokens.push_back(tok);
    }
    llama_batch_free(gen);

    OutputDebugStringA("LLAMAAgent::RunPromptInternal end\n");
}

// =====================================================
// Summary
// =====================================================
void LLAMAAgent::SummarizeAndReset() {
    OutputDebugStringA("LLAMAAgent::SummarizeAndReset start\n");
    assert(!m_isSummarizing);
    m_isSummarizing = true;

    const llama_vocab* vocab = llama_model_get_vocab(m_model->m_model);
    if (!vocab) {
        m_isSummarizing = false;
        OutputDebugStringA("LLAMAAgent::SummarizeAndReset end early\n");
        return;
    }

    std::string history;
    for (llama_token t : m_pastTokens) {
        char buf[256];
        int len = llama_token_to_piece(vocab, t, buf, sizeof(buf), 0, false);
        if (len > 0) history.append(buf, len);
    }

    if (history.empty()) {
        ResetContextUnlocked();
        m_isSummarizing = false;
        OutputDebugStringA("LLAMAAgent::SummarizeAndReset end early (no history)\n");
        return;
    }

    std::string prompt = "Summarize the following conversation briefly:\n\n" + history + "\n\nSummary:";

    llama_context* ctx = CreateContext();
    llama_sampler* sampler = CreateSampler();

    std::vector<llama_token> tokens(prompt.size() * 2 + 8);
    int n = llama_tokenize(vocab, prompt.c_str(), (int)prompt.size(), tokens.data(), (int)tokens.size(), true, false);
    if (n <= 0) {
        llama_sampler_free(sampler);
        llama_free(ctx);
        ResetContextUnlocked();
        m_isSummarizing = false;
        OutputDebugStringA("LLAMAAgent::SummarizeAndReset end early (tokenize failed)\n");
        return;
    }
    tokens.resize(n);

    const int max_ctx = m_config->n_ctx - 8;
    int start = 0;
    if (n > max_ctx) start = n - max_ctx;

    const int batch_size = 64;
    for (int offset = start; offset < n; offset += batch_size) {
        int sz = (std::min)(batch_size, n - offset);
        llama_batch b = llama_batch_init(sz, 0, 1);
        for (int i = 0; i < sz; ++i) {
            b.token[i] = tokens[offset + i];
            b.pos[i] = i - start;
            b.seq_id[i][0] = 0;
            b.n_seq_id[i] = 1;
            b.logits[i] = (i == sz - 1);
        }
        b.n_tokens = sz;
        llama_decode(ctx, b);
        llama_batch_free(b);
    }

    for (int i = start; i < n; ++i)
        llama_sampler_accept(sampler, tokens[i]);

    m_summaryText.clear();
    int pos = n - start;
    for (int i = 0; i < (int)m_config->max_tokens; ++i) {
        llama_token tok = llama_sampler_sample(sampler, ctx, -1);
        if (tok == LLAMA_TOKEN_NULL || llama_vocab_is_eog(vocab, tok))
            break;

        llama_sampler_accept(sampler, tok);

        if (!llama_vocab_is_control(vocab, tok)) {
            char buf[256];
            int len = llama_token_to_piece(vocab, tok, buf, sizeof(buf), 0, false);
            if (len > 0) m_summaryText.append(buf, len);
        }

        llama_batch g = llama_batch_init(1, 0, 1);
        g.token[0] = tok;
        g.pos[0] = pos++;
        g.seq_id[0][0] = 0;
        g.n_seq_id[0] = 1;
        g.logits[0] = 1;
        g.n_tokens = 1;
        llama_decode(ctx, g);
        llama_batch_free(g);
    }

    llama_sampler_free(sampler);
    llama_free(ctx);

    ResetContextUnlocked();
    m_isSummarizing = false;

    OutputDebugStringA("LLAMAAgent::SummarizeAndReset end\n");
}

// =====================================================
// Context reset
// =====================================================
void LLAMAAgent::ResetContext() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state.store(State::Idle, std::memory_order_release);
    ResetContextUnlocked();
}

// m_mutex を握っている前提
void LLAMAAgent::ResetContextUnlocked() {
    OutputDebugStringA("LLAMAAgent::ResetContextUnlocked start\n");

    if (m_ctx) {
        llama_free(m_ctx);
        m_ctx = nullptr;
    }

    llama_context_params c = llama_context_default_params();
    c.n_ctx = m_config->n_ctx;
    c.n_threads = m_config->n_threads;

    m_ctx = llama_init_from_model(m_model->m_model, c);
    assert(m_ctx);

    if (m_sampler)
        llama_sampler_reset(m_sampler);

    m_pastTokens.clear();
    m_nPast = 0;

    {
        std::lock_guard<std::mutex> o(m_outputMutex);
        m_output.clear();
    }

    OutputDebugStringA("LLAMAAgent::ResetContextUnlocked end\n");
}
