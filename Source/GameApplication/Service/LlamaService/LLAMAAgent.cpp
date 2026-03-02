// LLAMAAgent.cpp
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
// ctor / dtor / Stop / Public API
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
// WorkerMain (unchanged)
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
// RunPromptInternal — 既存実装（必要ならこちらも改善可）
// =====================================================
void LLAMAAgent::RunPromptInternal(const std::string& prompt) {
    OutputDebugStringA("LLAMAAgent::RunPromptInternal start\n");
    if (prompt.empty()) return;

    // NOTE: m_mutex is intentionally NOT held here.
    // All state accessed below (m_ctx, m_sampler, m_pastTokens, m_nPast,
    // m_isSummarizing) is exclusively owned by this worker thread.
    // m_running / m_state are atomic.  m_output is guarded by m_outputMutex.
    // Holding m_mutex throughout the generation loop would permanently block
    // Stop() from setting m_running=false, causing a deadlock/hang.
    const llama_vocab* vocab = llama_model_get_vocab(m_model->m_model);
    if (!vocab) return;

    // Leave 256 tokens of headroom: prevents generation from filling the context
    // to the brim and giving no room for the next prompt.
    const int safety_margin = 256;
    const int max_tokens_allowed = static_cast<int>(m_config->n_ctx) - safety_margin;

    // ---- Helper: build full prompt (reads m_summaryText, which may be updated by
    //      SummarizeAndReset below, so it is called lazily via a lambda) ----
    auto buildFullPrompt = [&]() -> std::string {
        std::string fp;
        if (!m_config->system_prompt.empty())
            fp += m_config->system_prompt + "\n";
        if (!m_summaryText.empty())
            fp += "[Conversation Summary]\n" + m_summaryText + "\n";
        fp += "<|user|>\n" + prompt + "\n<|assistant|>\n";
        return fp;
    };

    // ---- Helper: tokenize a string into tokens[], returns token count ----
    // Buffer sized at (chars * 2 + 8): worst-case is ~1 token per byte for
    // ASCII, the *2 guard handles multi-byte chars, +8 adds BOS/EOS slack.
    auto tokenizeStr = [&](const std::string& s, std::vector<llama_token>& toks) -> int {
        toks.assign(s.size() * 2 + 8, 0);
        int cnt = llama_tokenize(vocab, s.c_str(), static_cast<int>(s.size()),
                                 toks.data(), static_cast<int>(toks.size()), true, false);
        if (cnt > 0) toks.resize(cnt);
        return cnt;
    };

    // ---- Initial tokenization ----
    std::string fullPrompt = buildFullPrompt();
    std::vector<llama_token> tokens;
    int n = tokenizeStr(fullPrompt, tokens);
    if (n <= 0) return;

    // ---- n_ctx 超過対策: 要約してリセット ----
    if (m_nPast + n > max_tokens_allowed && !m_isSummarizing) {
        OutputDebugStringA("LLAMAAgent::RunPromptInternal: context near limit, calling SummarizeAndReset\n");
        SummarizeAndReset();

        // Re-build and re-tokenize NOW so the summary text is included in this
        // call's prompt (not just the next call).  After SummarizeAndReset,
        // m_nPast == 0 and m_summaryText is set.
        fullPrompt = buildFullPrompt();
        n = tokenizeStr(fullPrompt, tokens);
        if (n <= 0) return;
    }

    // ---- プロンプト単体が max を超える場合、末尾優先で切り詰め ----
    if (n > max_tokens_allowed) {
        int skip = n - max_tokens_allowed;
        tokens.erase(tokens.begin(), tokens.begin() + skip);
        n = static_cast<int>(tokens.size());
        OutputDebugStringA("LLAMAAgent::RunPromptInternal: prompt truncated to fit n_ctx\n");
    }

    // ---- 要約後もまだ溢れる場合は強制リセット ----
    if (m_nPast + n > max_tokens_allowed) {
        OutputDebugStringA("LLAMAAgent::RunPromptInternal: still overflowing after summarize, forcing hard reset\n");
        ResetContextUnlocked();
        // Clamp n in case it is somehow still too large
        if (n > max_tokens_allowed) {
            int skip = n - max_tokens_allowed;
            tokens.erase(tokens.begin(), tokens.begin() + skip);
            n = static_cast<int>(tokens.size());
        }
    }

    // ---- コンテキスト初期化 ----
    if (!m_ctx) m_ctx = CreateContext();
    if (!m_sampler) m_sampler = CreateSampler();
    if (!m_ctx) {
        OutputDebugStringA("LLAMAAgent::RunPromptInternal: m_ctx is null, cannot proceed\n");
        return;
    }

    // ---- Hard guard: must hold before every llama_decode call ----
    if (m_nPast + n > static_cast<int>(m_config->n_ctx)) {
        OutputDebugStringA("LLAMAAgent::RunPromptInternal: position would exceed n_ctx, aborting\n");
        ResetContextUnlocked();
        return;
    }

    // ---- decode NEW prompt tokens into existing context at position m_nPast ----
    // llama_decode enforces  n_tokens <= n_batch  (GGML_ASSERT, abort on violation).
    // n_batch defaults to 2048; a large tool-result prompt can exceed this easily.
    // We chunk the decode exactly as SummarizeAndReset does, using the context's
    // own n_batch as the chunk limit so it is always safe regardless of config.
    {
        const int batch_size = static_cast<int>(llama_n_batch(m_ctx));
        bool decodeOk = true;
        for (int offset = 0; offset < n && decodeOk; offset += batch_size) {
            int sz = (std::min)(batch_size, n - offset);
            llama_batch b = llama_batch_init(sz, 0, 1);
            for (int i = 0; i < sz; ++i) {
                b.token[i] = tokens[offset + i];
                b.pos[i] = m_nPast + offset + i;
                b.seq_id[i][0] = 0;
                b.n_seq_id[i] = 1;
                // Only the very last token of the whole prompt needs logits
                b.logits[i] = (offset + i == n - 1) ? 1 : 0;
            }
            b.n_tokens = sz;
            int rc = llama_decode(m_ctx, b);
            llama_batch_free(b);
            if (rc != 0) {
                decodeOk = false;
            }
        }
        if (!decodeOk) {
            OutputDebugStringA("LLAMAAgent::RunPromptInternal: decode(prompt) failed — resetting context\n");
            // Context is in an unknown state; reset so the next call starts clean.
            ResetContextUnlocked();
            return;
        }
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
    llama_batch gen = llama_batch_init(1, 0, 1);
    int loopCount = 0;
    while (m_running.load() && m_nPast < max_tokens_allowed) {
        ++loopCount;

        llama_token tok = llama_sampler_sample(m_sampler, m_ctx, -1);

        // debug: sampled id
        {
            char tb[128];
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

        // Hard guard before decode
        if (m_nPast >= static_cast<int>(m_config->n_ctx)) {
            OutputDebugStringA("RunPromptInternal: m_nPast reached n_ctx, stopping generation\n");
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
            if (llama_decode(m_ctx, gen) != 0) {
                OutputDebugStringA("RunPromptInternal: decode(control) failed\n");
                break;
            }
            m_pastTokens.push_back(tok);
            ++m_nPast;
            continue;
        }

        // convert token to piece and append to output
        {
            char buf[256];
            int len = llama_token_to_piece(vocab, tok, buf, sizeof(buf), 0, false);
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
        if (llama_decode(m_ctx, gen) != 0) {
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
        std::string dbg = m_output;
        if (dbg.size() > 512) dbg.resize(512);
        dbg += "\n";
        OutputDebugStringA(dbg.c_str());
    }
}

// =====================================================
// SummarizeAndReset
//  - 履歴を末尾優先で切り詰めて安定化
//  - 要約指示を明確化（2-3 文、短く）
//  - 要約トークン数を上限で制限（既定 200）
//  - 要約サンプラーは低温度で生成する（繰り返し軽減）
//  - 要約テキストを m_summaryText に格納し、次の RunPromptInternal で
//    プロンプトに組み込む（KV キャッシュへの raw トークン復元は行わない）
// =====================================================
void LLAMAAgent::SummarizeAndReset() {
    OutputDebugStringA("LLAMAAgent::SummarizeAndReset start\n");
    if (m_isSummarizing) {
        OutputDebugStringA("SummarizeAndReset: already summarizing\n");
        return;
    }
    m_isSummarizing = true;

    const llama_vocab* vocab = llama_model_get_vocab(m_model->m_model);
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
    const int history_token_limit = (std::max)(64, (int)m_config->n_ctx / 2); // heuristic
    int start_idx = 0;
    if ((int)m_pastTokens.size() > history_token_limit)
        start_idx = (int)m_pastTokens.size() - history_token_limit;

    std::string history;
    history.reserve(4096);
    for (size_t i = (size_t)start_idx; i < m_pastTokens.size(); ++i) {
        char tmp[512];
        int len = llama_token_to_piece(vocab, m_pastTokens[i], tmp, sizeof(tmp), 0, false);
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
    std::string prompt =
        "Summarize the following conversation in 2-3 concise sentences. "
        "Be factual, avoid repetition, and keep the summary short.\n\n"
        + history + "\n\nSummary:";

    // ---- create temp ctx and conservative sampler ----
    llama_context* ctx = CreateContext();
    llama_sampler_chain_params sp = llama_sampler_chain_default_params();
    llama_sampler* sampler = llama_sampler_chain_init(sp);
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
    int n = llama_tokenize(vocab, prompt.c_str(), (int)prompt.size(), tokens.data(), (int)tokens.size(), true, false);
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
        std::ostringstream oss;
        oss << "SummarizeAndReset: prompt tokens n=" << n << " first:";
        for (int i = 0; i < n && i < 8; ++i) {
            char tb[128]; llama_token_to_piece(vocab, tokens[i], tb, sizeof(tb), 0, false);
            oss << " " << tokens[i] << "(" << tb << ")";
        }
        OutputDebugStringA(oss.str().c_str());
    }

    // ---- decode prompt into temp ctx ----
    {
        const int batch_size = 64;
        int pos = 0;
        for (int offset = 0; offset < n; offset += batch_size) {
            int sz = (std::min)(batch_size, n - offset);
            llama_batch b = llama_batch_init(sz, 0, 1);
            for (int i = 0; i < sz; ++i) {
                b.token[i] = tokens[offset + i];
                b.pos[i] = pos + i;
                b.seq_id[i][0] = 0;
                b.n_seq_id[i] = 1;
                b.logits[i] = (i == sz - 1);
            }
            b.n_tokens = sz;
            int rc = llama_decode(ctx, b);
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
    m_summaryText.clear();
    // Cap summary at 200 tokens; that is enough for 2-3 concise sentences and
    // keeps the context impact of the injected summary predictably small.
    constexpr int kMaxSummaryTokens = 200;
    const int max_summary_tokens = (std::min)(kMaxSummaryTokens, (int)m_config->max_tokens);
    int gen_pos = n; // next decode position in temp ctx (prompt occupies 0..n-1)

    for (int i = 0; i < max_summary_tokens; ++i) {
        llama_token tok = llama_sampler_sample(sampler, ctx, -1);
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

        // append to summary text (ignore control tokens)
        if (!llama_vocab_is_control(vocab, tok)) {
            char piece[512];
            int len = llama_token_to_piece(vocab, tok, piece, sizeof(piece), 0, false);
            if (len > 0) m_summaryText.append(piece, len);
        }

        // decode sampled token into temp ctx at continuous position
        {
            llama_batch g = llama_batch_init(1, 0, 1);
            g.token[0] = tok;
            g.pos[0] = gen_pos;
            g.seq_id[0][0] = 0;
            g.n_seq_id[0] = 1;
            g.logits[0] = 1;
            g.n_tokens = 1;
            int rc = llama_decode(ctx, g);
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

    // ---- reset main context (clean start) ----
    // The summary text is already captured in m_summaryText as human-readable text.
    // RunPromptInternal will re-tokenize the full prompt (which now includes
    // summaryText) so the model sees a coherent context:
    //   [system_prompt][Conversation Summary: ...][<|user|>][new question][<|assistant|>]
    // Restoring raw compressed tokens into the KV cache would create an incoherent
    // sequence ([raw summary fragments][system_prompt][user]) and is avoided here.
    ResetContextUnlocked(); // m_nPast = 0, fresh m_ctx

    // ---- debug print summary safely in chunks ----
    {
        OutputDebugStringA("LLAMAAgent::SummarizeAndReset completed\n");
        OutputDebugStringA("SummaryText:\n");
        const size_t chunk = 1024;
        for (size_t i = 0; i < m_summaryText.size(); i += chunk) {
            std::string piece = m_summaryText.substr(i, chunk);
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

    if (m_ctx) {
        llama_free(m_ctx);
        m_ctx = nullptr;
    }

    llama_context_params c = llama_context_default_params();
    c.n_ctx = m_config->n_ctx;
    c.n_threads = m_config->n_threads;

    m_ctx = llama_init_from_model(m_model->m_model, c);
    if (!m_ctx) {
        // Context creation failed (OOM or other error).
        // m_ctx stays null; RunPromptInternal will detect this and bail out safely.
        OutputDebugStringA("LLAMAAgent::ResetContextUnlocked: llama_init_from_model returned null!\n");
        m_pastTokens.clear();
        m_nPast = 0;
        return;
    }

    if (m_sampler)
        llama_sampler_reset(m_sampler);

    // clear history by default — SummarizeAndReset will re-populate if needed
    m_pastTokens.clear();
    m_nPast = 0;

    {
        std::lock_guard<std::mutex> o(m_outputMutex);
        m_output.clear();
    }

    OutputDebugStringA("LLAMAAgent::ResetContextUnlocked end\n");
}
