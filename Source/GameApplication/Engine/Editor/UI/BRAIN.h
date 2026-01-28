#pragma once

#include "GameApplication.h"

#include "Editor/editorService.h"
#include "Editor/InterFace/IEditorUI.h"

#include <unordered_set>
#include <unordered_map>
#include <string>
#include <filesystem>

struct llama_model;
struct llama_context;
struct llama_sampler;

// B.R.A.I.N. = Buddy for Runtime Artificial Intelligence Navigator

class BRAIN: public IEditorUI{

public:
	void Initialize(EditorService* editor) override;
	void Finalize() override;
	void Draw(const EditorDrawContext ctx) override;

private:

	void RunLLM();

	EditorService* m_editor = nullptr;
	llama_model* m_llamaModel = nullptr;
	llama_context* m_llamaContext = nullptr;
	llama_sampler* m_sampler = nullptr;

	static inline const std::filesystem::path modelPath = "Asset/BRAIN/model/qwen2.5-coder-7b-instruct-q4_k_m.gguf";

	char inputBuffer[512]{};
	std::string outputText;
};
