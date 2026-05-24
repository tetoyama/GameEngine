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

#include "Backends/llama/llama.h"
#include "Resources/Data/llamaModelData.h"
#include "AgentConfig.h"

// =====================================================
// 内部ヘルパー（生成経路を完全統一）
// =====================================================
llama_context* LLAMAAgent::CreateContext() const {
    assert(m_pModel);
    assert(m_config);

    OutputDebugStringA("LLAMAAgent::CreateContext start\n");

    llama_context_params m_C= llama_context_default_params();
    c.n_ctx = m_config->n_ctx;
    c.n_threads = m_config->n_threads;

    llama_context* ctx = llama_init_from_model(m_pModel->m_pModel, c);
    assert(ctx && "llama_init_from_model failed");

    OutputDebugStringA("LLAMAAgent::CreateContext end\n");
    return m_Ctx;
}

llama_sampler* LLAMAAgent::CreateSampler() const {
    assert(m_config);

    OutputDebugStringA("LLAMAAgent::CreateSampler start\n");

    llama_sampler_chain_params m_Sp= llama_sampler_chain_default_params();
    llama_sampler* pSampler = llama_sampler_chain_init(sp);
    assert(sampler);

    llama_sampler_chain_add(sampler, llama_sampler_init_top_k(m_config->top_k));
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(m_config->top_p, 1));
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(m_config->temperature));
    llama_sampler_chain_add(sampler, llama_sampler_init_penalties(128, m_config->repeat_penalty, 0.0f, 0.0f));
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(static_cast<uint32_t>(
        std::chrono::system_clock::now().time_since_epoch().count()
        )));

    OutputDebugStringA("LLAMAAgent::CreateSampler end\n");
    return m_Sampler;
}

// =====================================================
// ctor / dtor / Stop / Public API
// =====================================================
LLAMAAgent::LLAMAAgent(std::shared_ptr<LLAMAModelData> model,
                       std::shared_ptr<const AgentConfig> config)
    : m_pModel(std::move(model))
    , m_config(std::move(config))
    , m_running(true)
    , m_nPast(0)
    , m_isSummarizing(false) {

    assert(m_pModel);
    assert(m_config);
    assert(m_config->IsValid());

    OutputDebugStringA("LLAMAAgent ctor start\n");

    m_pCtx = CreateContext();
    m_pSampler = CreateSampler();

    m_thread = std::thread(&LLAMAAgent::WorkerMain, this);

    OutputDebugStringA("LLAMAAgent ctor end\n");
}

LLAMAAgent::~LLAMAAgent() {
    OutputDebugStringA("LLAMAAgent dtor start\n");

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_running.store(false);
    }
    m_cv.notify_all();

    if (m_thread.joinable())
        m_thread.join();

    if (m_pSampler) {
        llama_sampler_free(m_pSampler);
        m_pSampler = nullptr;
    }
    if (m_pCtx) {
        llama_free(m_pCtx);
        m_pCtx = nullptr;
    }

    m_state.store(State::Dead);

    OutputDebugStringA("LLAMAAgent dtor end\n");
}

void LLAMAAgent::Stop() {
    OutputDebugStringA("LLAMAAgent::Stop called\n");

    std::lock_guard<std::mutex> lock(m_mutex);
    m_state.store(State::Stopping, std::memory_order_release);
    m_running.store(false, std::memory_order_release);
    m_cv.notify_all();
}

void LLAMAAgent::RunAsync(const std::string& prompt) {
    if (prompt.empty()) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_jobQueue.push(prompt);
    m_cv.notify_one();
}

std::string LLAMAAgent::GetOutput() const {
    std::lock_guard<std::mutex> lock(m_outputMutex);
    return m_Output;
}

int LLAMAAgent::GetTokenCount() const noexcept {
    return m_NPast;
}

int LLAMAAgent::GetMaxTokenCount() const noexcept {
    return m_config->n_ctx;
}

float LLAMAAgent::GetTokenUsageRate() const noexcept {
    return m_config->n_ctx > 0 ? static_cast<float>(m_nPast) / m_config->n_ctx : 0.0f;
}

// =====================================================
// WorkerMain (unchanged)
// =====================================================
void LLAMAAgent::WorkerMain() {
    OutputDebugStringA("LLAMAAgent::WorkerMain start\n");

    while (true) {
        std::string m_Prompt;
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
// RunPromptInternal — 既存実装（必要ならこちらも改善可）
// =====================================================
void LLAMAAgent::RunPromptInternal(const std::string& prompt) {
    OutputDebugStringA("LLAMAAgent::RunPromptInternal start\n");
    if (prompt.empty()) return;

    std::unique_lock<std::mutex> lock(m_mutex);
    const llama_vocab* vocab = llama_model_get_vocab(m_pModel->m_pModel);
    if (!vocab) return;

    // ---- fullPrompt 構築 ----
    std::string m_FullPrompt;
    if (!m_config->system_prompt.empty())
        fullPrompt += m_config->system_prompt + "\n";

    if (!m_summaryText.empty())
        fullPrompt += "[Conversation Summary]\n" + m_summaryText + "\n";

    fullPrompt += "<|user|>\n" + prompt + "\n<|assistant|>\n";

    // ---- トークン化 ----
    std::vector<llama_token> tokens(fullPrompt.size() * 2 + 8);
    int m_N= llama_tokenize(vocab, fullPrompt.c_str(), (int)fullPrompt.size(),
                           tokens.data(), (int)tokens.size(), true, false);
    if (n <= 0) return;
    tokens.resize(n);

    // ---- n_ctx 超過対策 ----
    const int m_SafetyMargin= 16;
    const int m_MaxTokensAllowed= m_config->n_ctx - safety_margin;

    if (m_nPast + n > max_tokens_allowed && !m_isSummarizing) {
        OutputDebugStringA("LLAMAAgent::RunPromptInternal n_ctx 超過、SummarizeAndReset 呼び出し\n");
        SummarizeAndReset(); // 過去トークン圧縮
    }

    // ---- コンテキスト初期化 ----
    if (!m_pCtx) m_pCtx = CreateContext();
    if (!m_pSampler) m_pSampler = CreateSampler();

    // ---- decode NEW prompt tokens into existing context at position m_nPast ----
    {
        llama_batch m_B= llama_batch_init(n, 0, 1);
        for (int i = 0; i < n; ++i) {
            b.token[i] = tokens[i];
            b.pos[i] = m_nPast + i;
            b.seq_id[i][0] = 0;
            b.n_seq_id[i] = 1;
            b.logits[i] = (i == n - 1) ? 1 : 0;
        }
        b.n_tokens = n;
        if (llama_decode(m_pCtx, b) != 0) {
            llama_batch_free(b);
            OutputDebugStringA("LLAMAAgent::RunPromptInternal: decode(prompt) failed\n");
            return;
        }
        llama_batch_free(b);
    }

    // append prompt tokens to history and update m_nPast
    m_pastTokens.insert(m_pastTokens.end(), tokens.begin(), tokens.end());
    m_nPast = static_cast<int>(m_pastTokens.size());

    // ---- clear output buffer before generation ----
    {
        std::lock_guard<std::mutex> o(m_outputMutex);
        m_output.clear();
    }

    // ---- generation loop (sample -> output -> decode -> append) ----
    llama_batch m_Gen= llama_batch_init(1, 0, 1);
    int m_LoopCount= 0;
    while (m_running.load() && m_nPast < max_tokens_allowed) {
        ++loopCount;

        llama_token m_Tok= llama_sampler_sample(m_pSampler, m_pCtx, -1);

        // debug: sampled id
        {
            char m_Tb[128];
            sprintf_s(tb, "sampled tok=%d loop=%d\n", (int)tok, loopCount);
            OutputDebugStringA(tb);
        }

        if (tok == LLAMA_TOKEN_NULL) {
            OutputDebugStringA("RunPromptInternal: sampled NULL\n");
            break;
        }
        if (llama_vocab_is_eog(vocab, tok)) {
            OutputDebugStringA("RunPromptInternal: sampled EOG\n");
            break;
        }

        // ignore control tokens for textual output, but still decode and store them
        if (llama_vocab_is_control(vocab, tok)) {
            // decode into context to keep positions aligned
            gen.token[0] = tok;
            gen.pos[0] = m_nPast;
            gen.seq_id[0][0] = 0;
            gen.n_seq_id[0] = 1;
            gen.logits[0] = 1;
            gen.n_tokens = 1;
            if (llama_decode(m_pCtx, gen) != 0) {
                OutputDebugStringA("RunPromptInternal: decode(control) failed\n");
                break;
            }
            m_pastTokens.push_back(tok);
            ++m_nPast;
            continue;
        }

        // convert token to piece and append to output
        {
            char m_Buf[256];
            int m_Len= llama_token_to_piece(vocab, tok, buf, sizeof(buf), 0, false);
            if (len > 0) {
                std::lock_guard<std::mutex> o(m_outputMutex);
                m_output.append(buf, len);
            }
        }

        // decode token into context (advance position)
        gen.token[0] = tok;
        gen.pos[0] = m_nPast;
        gen.seq_id[0][0] = 0;
        gen.n_seq_id[0] = 1;
        gen.logits[0] = 1;
        gen.n_tokens = 1;
        if (llama_decode(m_pCtx, gen) != 0) {
            OutputDebugStringA("RunPromptInternal: decode(sample) failed\n");
            break;
        }

        // record token AFTER decode
        m_pastTokens.push_back(tok);
        ++m_nPast;

        // safety break
        if (loopCount > 20000) {
            OutputDebugStringA("RunPromptInternal: loopCount safety break\n");
            break;
        }
    }

    llama_batch_free(gen);

    OutputDebugStringA("LLAMAAgent::RunPromptInternal end\n");

    // final output debug line (shortened)
    {
        std::lock_guard<std::mutex> o(m_outputMutex);
        std::string m_Dbg= m_output;
        if (dbg.size() > 512) dbg.resize(512);
        dbg += "\n";
        OutputDebugStringA(dbg.c_str());
    }
}

// =====================================================
// SummarizeAndReset — 改善版
//  - 履歴を末尾優先で切り詰めて安定化
//  - 要約指示を明確化（2-3 文、短く）
//  - 要約トークン数を上限で制限（既定 200）
//  - 要約サンプラーは低温度で生成する（繰り返し軽減）
//  - 圧縮トークンを新しいコンテキストに再デコードして復元
// =====================================================
void LLAMAAgent::SummarizeAndReset() {
    OutputDebugStringA("LLAMAAgent::SummarizeAndReset start\n");
    if (m_isSummarizing) {
        OutputDebugStringA("SummarizeAndReset: already summarizing\n");
        return;
    }
    m_isSummarizing = true;

    const llama_vocab* vocab = llama_model_get_vocab(m_pModel->m_pModel);
    if (!vocab) {
        OutputDebugStringA("SummarizeAndReset: no vocab\n");
        m_isSummarizing = false;
        return;
    }

    if (m_pastTokens.empty()) {
        OutputDebugStringA("SummarizeAndReset: no past tokens -> ResetContextUnlocked\n");
        ResetContextUnlocked();
        m_isSummarizing = false;
        return;
    }

    // ---- truncate history (prefer most recent tokens) ----
    const int m_HistoryTokenLimit= (std::max)(64, (int)m_config->n_ctx / 2); // heuristic
    int m_StartIdx= 0;
    if ((int)m_pastTokens.size() > history_token_limit)
        start_idx = (int)m_pastTokens.size() - history_token_limit;

    std::string m_History;
    history.reserve(4096);
    for (size_t i = (size_t)start_idx; i < m_pastTokens.size(); ++i) {
        char m_Tmp[512];
        int m_Len= llama_token_to_piece(vocab, m_pastTokens[i], tmp, sizeof(tmp), 0, false);
        if (len > 0) history.append(tmp, len);
        if ((int)history.size() > 4000) break; // hard char limit
    }

    if (history.empty()) {
        OutputDebugStringA("SummarizeAndReset: history empty after truncation\n");
        ResetContextUnlocked();
        m_isSummarizing = false;
        return;
    }

    // ---- summarization prompt (explicit instruction) ----
    std::string m_Prompt=
        "Summarize the following conversation in 2-3 concise sentences. "
        "Be factual, avoid repetition, and keep the summary short.\n\n"
        + history + "\n\nSummary:";

    // ---- create temp ctx and conservative sampler ----
    llama_context* ctx = CreateContext();
    llama_sampler_chain_params m_Sp= llama_sampler_chain_default_params();
    llama_sampler* pSampler = llama_sampler_chain_init(sp);
    // conservative settings to reduce repetition
    llama_sampler_chain_add(sampler, llama_sampler_init_top_k((std::max)(5, (int)m_config->top_k / 4)));
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(0.9f, 1));
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(0.2f));
    llama_sampler_chain_add(sampler, llama_sampler_init_penalties(128, m_config->repeat_penalty, 0.0f, 0.0f));
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(static_cast<uint32_t>(
        std::chrono::system_clock::now().time_since_epoch().count()
        )));

        // ---- tokenize prompt ----
    std::vector<llama_token> tokens(prompt.size() * 2 + 8);
    int m_N= llama_tokenize(vocab, prompt.c_str(), (int)prompt.size(), tokens.data(), (int)tokens.size(), true, false);
    if (n <= 0) {
        OutputDebugStringA("SummarizeAndReset: tokenize failed\n");
        llama_sampler_free(sampler);
        llama_free(ctx);
        m_isSummarizing = false;
        return;
    }
    tokens.resize(n);

    // debug: token sample
    {
        std::ostringstream m_Oss;
        oss << "SummarizeAndReset: prompt tokens n=" << n << " first:";
        for (int i = 0; i < n && i < 8; ++i) {
            char m_Tb[128]; llama_token_to_piece(vocab, tokens[i], tb, sizeof(tb), 0, false);
            oss << " " << tokens[i] << "(" << tb << ")";
        }
        OutputDebugStringA(oss.str().c_str());
    }

    // ---- decode prompt into temp ctx ----
    {
        const int m_BatchSize= 64;
        int m_Pos= 0;
        for (int offset = 0; offset < n; offset += batch_size) {
            int m_Sz= (std::min)(batch_size, n - offset);
            llama_batch m_B= llama_batch_init(sz, 0, 1);
            for (int i = 0; i < sz; ++i) {
                b.token[i] = tokens[offset + i];
                b.pos[i] = pos + i;
                b.seq_id[i][0] = 0;
                b.n_seq_id[i] = 1;
                b.logits[i] = (i == sz - 1);
            }
            b.n_tokens = sz;
            int m_Rc= llama_decode(ctx, b);
            if (rc != 0) {
                OutputDebugStringA("SummarizeAndReset: decode(prompt) failed rc != 0\n");
                llama_batch_free(b);
                llama_sampler_free(sampler);
                llama_free(ctx);
                m_isSummarizing = false;
                return;
            }
            llama_batch_free(b);
            pos += sz;
        }
    }

    // ---- prime sampler with prompt tokens (important for repetition penalties) ----
    for (int i = 0; i < n; ++i) {
        llama_sampler_accept(sampler, tokens[i]);
    }

    // ---- sampling loop ----
    std::vector<llama_token> m_CompressedTokens;
    m_summaryText.clear();
    const int m_MaxSummaryTokens= (std::min)(200, (int)m_config->max_tokens);
    int m_GenPos= n; // next decode position in temp ctx (prompt occupies 0..n-1)

    for (int i = 0; i < max_summary_tokens; ++i) {
        llama_token m_Tok= llama_sampler_sample(pSampler, ctx, -1);
        if (tok == LLAMA_TOKEN_NULL) {
            OutputDebugStringA("SummarizeAndReset: sampled NULL\n");
            break;
        }
        if (llama_vocab_is_eog(vocab, tok)) {
            OutputDebugStringA("SummarizeAndReset: sampled EOG\n");
            break;
        }

        // ensure sampler state updated
        llama_sampler_accept(sampler, tok);
        compressedTokens.push_back(tok);

        // append to summary text (ignore control tokens)
        if (!llama_vocab_is_control(vocab, tok)) {
            char m_Piece[512];
            int m_Len= llama_token_to_piece(vocab, tok, piece, sizeof(piece), 0, false);
            if (len > 0) m_summaryText.append(piece, len);
        }

        // decode sampled token into temp ctx at continuous position
        {
            llama_batch m_G= llama_batch_init(1, 0, 1);
            g.token[0] = tok;
            g.pos[0] = gen_pos;
            g.seq_id[0][0] = 0;
            g.n_seq_id[0] = 1;
            g.logits[0] = 1;
            g.n_tokens = 1;
            int m_Rc= llama_decode(ctx, g);
            llama_batch_free(g);
            if (rc != 0) {
                OutputDebugStringA("SummarizeAndReset: decode(sample) failed (rc != 0)\n");
                break;
            }
            ++gen_pos;
        }
    }

    // cleanup temp sampler/ctx
    llama_sampler_free(sampler);
    llama_free(ctx);

    // ---- reset main context and restore compressed tokens ----
    ResetContextUnlocked(); // creates new m_pCtx and resets main sampler

    if (!compressedTokens.empty()) {
        // decode compressedTokens into the newly created m_pCtx in batches with contiguous positions
        int m_Pos= 0;
        const int m_BatchSize= 64;
        for (size_t offset = 0; offset < compressedTokens.size(); offset += batch_size) {
            int m_Sz= (int)(std::min)(batch_size, (int)compressedTokens.size() - (int)offset);
            llama_batch m_B= llama_batch_init(sz, 0, 1);
            for (int j = 0; j < sz; ++j) {
                b.token[j] = compressedTokens[offset + j];
                b.pos[j] = pos + j;
                b.seq_id[j][0] = 0;
                b.n_seq_id[j] = 1;
                b.logits[j] = (j == sz - 1) ? 1 : 0;
            }
            b.n_tokens = sz;
            int m_Rc= llama_decode(m_pCtx, b);
            llama_batch_free(b);
            if (rc != 0) {
                OutputDebugStringA("SummarizeAndReset: decode(compressed) failed rc != 0\n");
                // if decoding compressedTokens fails, clear and bail to consistent state
                m_pastTokens.clear();
                m_nPast = 0;
                break;
            }
            pos += sz;
        }
        // restore history if decode succeeded
        if (!compressedTokens.empty()) {
            m_pastTokens = compressedTokens;
            m_nPast = (int)m_pastTokens.size();
        }
    } else {
        m_pastTokens.clear();
        m_nPast = 0;
    }

    // ---- debug print summary safely in chunks ----
    {
        OutputDebugStringA("LLAMAAgent::SummarizeAndReset completed\n");
        OutputDebugStringA("SummaryText:\n");
        const size_t m_Chunk= 1024;
        for (size_t i = 0; i < m_summaryText.size(); i += chunk) {
            std::string m_Piece= m_summaryText.substr(i, chunk);
            OutputDebugStringA(piece.c_str());
        }
        OutputDebugStringA("\n");
    }

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

void LLAMAAgent::ResetContextUnlocked() {
    OutputDebugStringA("LLAMAAgent::ResetContextUnlocked start\n");

    if (m_pCtx) {
        llama_free(m_pCtx);
        m_pCtx = nullptr;
    }

    llama_context_params m_C= llama_context_default_params();
    c.n_ctx = m_config->n_ctx;
    c.n_threads = m_config->n_threads;

    m_pCtx = llama_init_from_model(m_pModel->m_pModel, c);
    assert(m_pCtx);

    if (m_pSampler)
        llama_sampler_reset(m_pSampler);

    // clear history by default — SummarizeAndReset will re-populate if needed
    m_pastTokens.clear();
    m_nPast = 0;

    {
        std::lock_guard<std::mutex> o(m_outputMutex);
        m_output.clear();
    }

    OutputDebugStringA("LLAMAAgent::ResetContextUnlocked end\n");
}
