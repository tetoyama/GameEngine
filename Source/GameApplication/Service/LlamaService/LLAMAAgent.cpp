#include "LLAMAAgent.h"
#include <Windows.h>

#include "Resources/Data/llamaModelData.h"
#include "AgentConfig.h"
#include "Backends/llama/llama.h"

LLAMAAgent::LLAMAAgent(const std::shared_ptr<LLAMAModelData>& model, const std::shared_ptr<const AgentConfig>& config) : m_model(model), m_config(config) {
    OutputDebugStringA(("LLAMAAgent created\n"));
}
LLAMAAgent::~LLAMAAgent() {
    OutputDebugStringA("LLAMAAgent destroyed\n");
}

// 同期版
std::string LLAMAAgent::RunPrompt(const std::string& prompt) {
    std::string output;
    InternalRunPrompt(prompt, output);
    return output;
}

// 非同期版
void LLAMAAgent::RunPromptAsync(const std::string& prompt, std::function<void(const std::string&)> callback) {
    std::thread([this, prompt, callback]() {
        std::string output;
        InternalRunPrompt(prompt, output);
        if (callback) callback(output);
    }).detach(); // デタッチして非同期実行
}

// 実際のモデル呼び出し
void LLAMAAgent::InternalRunPrompt(const std::string& prompt, std::string& output) {
    std::lock_guard<std::mutex> lock(m_mutex);

    output.clear();

    if (!m_model || !m_model->m_model) {
        output = "[LLAMAAgent] No model loaded.";
        return;
    }

    const llama_vocab* vocab = m_model->m_vocab;

    // ============================
    // context 初期化（初回のみ）
    // ============================
    if (!m_context) {
        llama_context_params cparams = llama_context_default_params();
        cparams.n_ctx = m_config->maxTokens;
        cparams.n_threads = (std::max)(1u, std::thread::hardware_concurrency());

        m_context = llama_init_from_model(m_model->m_model, cparams);
        if (!m_context) {
            output = "[LLAMAAgent] Failed to create context.";
            return;
        }

        // sampler
        llama_sampler_chain_params sp = llama_sampler_chain_default_params();
        m_sampler = llama_sampler_chain_init(sp);
        llama_sampler_chain_add(m_sampler, llama_sampler_init_top_k(m_config->topK));
        llama_sampler_chain_add(m_sampler, llama_sampler_init_top_p(m_config->topP, 1));
        llama_sampler_chain_add(m_sampler, llama_sampler_init_temp(m_config->temperature));
    }

    // ============================
    // prompt 組み立て
    // ============================
    std::string fullPrompt =
        "<|user|>\n" + prompt + "\n<|assistant|>\n";

    std::vector<llama_token> promptTokens(fullPrompt.size() * 2 + 16);
    int n_prompt = llama_tokenize(
        vocab,
        fullPrompt.c_str(),
        (int)fullPrompt.size(),
        promptTokens.data(),
        (int)promptTokens.size(),
        true,
        false
    );

    if (n_prompt <= 0) {
        output = "[LLAMAAgent] Tokenize failed.";
        return;
    }
    promptTokens.resize(n_prompt);

    // ============================
    // ユーザー入力を decode
    // ============================
    {
        llama_batch batch = llama_batch_init(n_prompt, 0, 1);
        for (int i = 0; i < n_prompt; ++i) {
            batch.token[i] = promptTokens[i];
            batch.pos[i] = m_nPast + i;
            batch.seq_id[i][0] = 0;
            batch.n_seq_id[i] = 1;
        }
        batch.n_tokens = n_prompt;
        batch.logits[n_prompt - 1] = 1;

        if (llama_decode(m_context, batch) != 0) {
            llama_batch_free(batch);
            output = "[LLAMAAgent] llama_decode failed (prompt).";
            return;
        }
        llama_batch_free(batch);
    }

    m_pastTokens.insert(m_pastTokens.end(), promptTokens.begin(), promptTokens.end());
    m_nPast += n_prompt;

    // ============================
    // アシスタント生成
    // ============================
    int newlineCount = 0;

    for (int i = 0; i < m_config->maxTokens; ++i) {
        llama_token tok = llama_sampler_sample(m_sampler, m_context, -1);
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

        // token を context に反映
        llama_batch b = llama_batch_init(1, 0, 1);
        b.token[0] = tok;
        b.pos[0] = m_nPast;
        b.seq_id[0][0] = 0;
        b.n_seq_id[0] = 1;
        b.n_tokens = 1;
        b.logits[0] = 1;

        if (llama_decode(m_context, b) != 0) {
            llama_batch_free(b);
            break;
        }
        llama_batch_free(b);

        m_pastTokens.push_back(tok);
        ++m_nPast;
    }
}

