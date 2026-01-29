#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <Windows.h>
#include "llamaModelData.h"
#include <debugapi.h>
#include <llama/llama.h>

#pragma comment(lib, "llama.lib")

LLAMAModelData::LLAMAModelData() {
	OutputDebugStringA(("Created LLAMAModelData " + m_path + "\n").c_str());
}

LLAMAModelData::~LLAMAModelData() {
	OutputDebugStringA(("Destroyed LLAMAModelData: " + m_path + "\n").c_str());
	if (m_model) {
		llama_model_free(m_model);
		m_model = nullptr;
	}
}
