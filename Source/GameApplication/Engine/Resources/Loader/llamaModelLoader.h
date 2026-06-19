// =======================================================================
// 
// llamaModelLoader.h
// 
// =======================================================================
#pragma once

#include "ResourceLoader.h"

#include <memory>
#include <string>

#include "../Data/llamaModelData.h"

#include <llama.h>

inline std::shared_ptr<LLAMAModelData> LoadLLAMAModelFromFile(const std::string& filePath){
	std::shared_ptr<LLAMAModelData> modelData = std::make_shared<LLAMAModelData>();
	modelData->m_path = filePath;

	OutputDebugStringA(("Loading LLAMA model: " + filePath + "\n").c_str());

	llama_model_params mParams = llama_model_default_params();
	mParams.n_gpu_layers = 0; // GPUなし

	// llama_model_load_from_file は C API なので例外は投げない。
	// 失敗時は単に nullptr を返すだけなので、try/catch ではなく戻り値チェックで判定する。
	modelData->m_model = llama_model_load_from_file(filePath.c_str(), mParams);
	if(!modelData->m_model){
		OutputDebugStringA(("LLAMA load failed (model is null): " + filePath + "\n").c_str());
		return nullptr;
	}

	// llama_vocab は model から取得
	modelData->m_vocab = llama_model_get_vocab(modelData->m_model);
	if(!modelData->m_vocab){
		// vocab が取れないのは異常系。モデルだけ作って vocab が null のまま
		// 後段（トークナイズ等）に渡ると未定義動作の温床になるので、ここで弾く。
		OutputDebugStringA(("LLAMA load failed (vocab is null): " + filePath + "\n").c_str());
		llama_model_free(modelData->m_model);
		modelData->m_model = nullptr;
		return nullptr;
	}

	return modelData;
}

// ResourceLoader 用の設定
template<>
inline void ResourceLoader<LLAMAModelData>::SetupLoadFunc(void* /*unused*/){
	OutputDebugStringA("SetupLoadFunc LLAMAModelData called\n");

	SetLoadFunction([](const std::string& path, std::shared_ptr<void> /*args*/){
		return LoadLLAMAModelFromFile(path);
					});
}