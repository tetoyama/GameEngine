// =======================================================================
// 
// llamaModelData.cpp
// 
// =======================================================================
#include "llamaModelData.h"

#include <Windows.h>
#include <llama.h>

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
