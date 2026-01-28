#define _CRT_SECURE_NO_WARNINGS // for sscanf

#include "BRAIN.h"
#include <Service/DebugTools/DebugSystem.h>
#include <Backends/llama/llama.h>
#include <Backends/llama/llama-model.h>
#include <Backends/llama/llama-cpp.h>
#include "MenuBar.h"
#include <ImGui/imgui.h>
#include <chrono>
#include <filesystem>
#include <vector>
#include <string>
#include <thread>

#pragma comment(lib, "llama.lib")
static void DebugOutUTF8(const std::string utf8){
	if(utf8.empty()) return;

	int len = MultiByteToWideChar(
		CP_UTF8,
		0,
		utf8.c_str(),
		-1,
		nullptr,
		0
	);

	if(len <= 0) return;

	std::wstring wide(len, L'\0');
	MultiByteToWideChar(
		CP_UTF8,
		0,
		utf8.c_str(),
		-1,
		wide.data(),
		len
	);

	OutputDebugStringW(wide.c_str());
}
void BRAIN::Initialize(EditorService* editor){
	m_editor = editor;

	// LLaMA バックエンド初期化
	llama_backend_init();

	if(!std::filesystem::exists(modelPath)){
		m_editor->debugLogSystem->LOG_ERROR("BRAIN: model not found: " + modelPath.string());
		return;
	}

	// --- モデルロード (CPU専用・GGUF対応) ---
	llama_model_params mParams = llama_model_default_params();
	mParams.n_gpu_layers = 0; // GPUは使わない
	mParams.main_gpu = -1;    // GPUなし

	m_llamaModel = llama_model_load_from_file(modelPath.string().c_str(), mParams);
	if(!m_llamaModel){
		m_editor->debugLogSystem->LOG_ERROR("BRAIN: failed to load model");
		return;
	}

	// --- コンテキスト作成 (CPU専用) ---
	llama_context_params ctxParams = llama_context_default_params();
	ctxParams.n_threads = (std::max)(1u, std::thread::hardware_concurrency());
	ctxParams.n_ctx = 2048;
	ctxParams.offload_kqv = false; // CPU専用
	ctxParams.kv_unified = false;

	m_llamaContext = llama_init_from_model(m_llamaModel, ctxParams);
	if(!m_llamaContext){
		m_editor->debugLogSystem->LOG_ERROR("BRAIN: failed to create context");
		return;
	}

	// --- サンプラー作成 ---
	llama_sampler_chain_params sparams = llama_sampler_chain_default_params();
	m_sampler = llama_sampler_chain_init(sparams);
	if(m_sampler){
		llama_sampler_chain_add(m_sampler, llama_sampler_init_top_k(40));
		llama_sampler_chain_add(m_sampler, llama_sampler_init_top_p(0.95f, 1));
		llama_sampler_chain_add(m_sampler, llama_sampler_init_temp(1.0f));
		llama_sampler_chain_add(m_sampler, llama_sampler_init_dist(
			(uint32_t)std::chrono::system_clock::now().time_since_epoch().count()
		));

		if(!llama_set_sampler(m_llamaContext, 0, m_sampler)){
			m_editor->debugLogSystem->LOG_WARNING("BRAIN: failed to set sampler on context");
		}
	}
}
static std::string DetokenizePiece(const char* piece){
	if(!piece) return {};

	std::string s = piece;

	// GPT系BPEの可視トークン対策
	for(size_t pos = 0; (pos = s.find("Ġ", pos)) != std::string::npos; ){
		s.replace(pos, 2, " ");
	}

	for(size_t pos = 0; (pos = s.find("Ċ", pos)) != std::string::npos; ){
		s.replace(pos, 2, "\n");
	}

	return s;
}

void BRAIN::RunLLM(){
	if(!m_llamaContext || !m_llamaModel || !m_sampler){
		outputText = "Error: model not initialized";
		return;
	}

	const std::string prompt = inputBuffer;
	if(prompt.empty()){
		outputText.clear();
		return;
	}

	const llama_vocab* vocab = llama_model_get_vocab(m_llamaModel);
	if(!vocab){
		outputText = "Error: failed to get vocab";
		return;
	}

	// ===== tokenize =====
	std::vector<llama_token> promptTokens(prompt.size() * 2 + 16);
	int32_t n_prompt = llama_tokenize(
		vocab,
		prompt.c_str(),
		(int32_t)prompt.size(),
		promptTokens.data(),
		(int32_t)promptTokens.size(),
		true,
		false
	);
	if(n_prompt <= 0){
		outputText = "Tokenization failed";
		return;
	}
	promptTokens.resize(n_prompt);

	// ===== decode prompt =====
	{
		llama_batch batch = llama_batch_get_one(promptTokens.data(), n_prompt);
		if(llama_decode(m_llamaContext, batch) != 0){
			outputText = "Decode (prompt) failed";
			return;
		}
	}

	// ===== generation =====
	std::vector<llama_token> genTokens;
	const int max_gen = 256;

	for(int i = 0; i < max_gen; ++i){
		llama_token tok = llama_sampler_sample(m_sampler, m_llamaContext, -1);

		if(tok == LLAMA_TOKEN_NULL) break;
		if(llama_vocab_is_eog(vocab, tok)) break;

		// 特殊トークンは無視
		if(llama_vocab_is_control(vocab, tok)){
			continue;
		}

		genTokens.push_back(tok);

		llama_batch tbatch = llama_batch_get_one(&tok, 1);
		if(llama_decode(m_llamaContext, tbatch) != 0){
			break;
		}
	}


	// ===== detokenize safely =====
	std::string utf8;
	utf8.reserve(genTokens.size() * 4);

	for(llama_token t : genTokens){
		char buf[8 * 4]; // 十分大きめ
		int len = llama_token_to_piece(
			vocab,
			t,
			buf,
			sizeof(buf),
			0,
			false
		);

		if(len > 0){
			utf8.append(buf, len);
		}
	}


	outputText = utf8;
	auto RemoveSpecialTokens = [](std::string& s){
		const char* patterns[] = {
			"<|im_start|>",
			"<|im_end|>",
			"<|assistant|>",
			"<|system|>",
			"<|user|>"
		};

		for(const char* p : patterns){
			size_t pos;
			while((pos = s.find(p)) != std::string::npos){
				s.erase(pos, strlen(p));
			}
		}
		};
	RemoveSpecialTokens(outputText);

	// ===== UTF-8 → UTF-16 → Debug出力 =====
	int wlen = MultiByteToWideChar(CP_UTF8, 0, outputText.c_str(), -1, nullptr, 0);
	if(wlen > 0){
		std::wstring wstr(wlen, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, outputText.c_str(), -1, wstr.data(), wlen);
		OutputDebugStringW(L"\n[BRAIN Output]\n");
		OutputDebugStringW(wstr.c_str());
		OutputDebugStringW(L"\n");
	}
}







void BRAIN::Finalize(){
	if(m_llamaContext){
		llama_free(m_llamaContext);
		m_llamaContext = nullptr;
	}
	if(m_llamaModel){
		llama_model_free(m_llamaModel);
		m_llamaModel = nullptr;
	}
	if(m_sampler){
		llama_sampler_free(m_sampler);
		m_sampler = nullptr;
	}
}

void BRAIN::Draw(const EditorDrawContext ctx){
	bool* showUI = &m_editor->GetUI<MenuBar>()->showAssetsBrowser;
	if(!showUI || !*showUI) return;

	ImGui::Begin("B.R.A.I.N.", showUI);

	// --- Prompt ---
	ImGui::Text("Prompt");
	ImGui::InputTextMultiline(
		"##Prompt",
		inputBuffer,
		sizeof(inputBuffer),
		ImVec2(-1, 100)
	);

	if(ImGui::Button("Run")){
		RunLLM();
	}

	ImGui::Separator();

	// --- Output (Copyable) ---
	ImGui::Text("Output");

	ImGuiInputTextFlags flags =
		ImGuiInputTextFlags_ReadOnly |
		ImGuiInputTextFlags_NoUndoRedo;

	ImGui::InputTextMultiline(
		"##Output",
		(char*)outputText.c_str(),   // ImGuiは non-const を要求する
		outputText.size() + 1,       // null終端込み
		ImVec2(-1, 200),
		flags
	);

	ImGui::End();
}


