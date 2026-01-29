// LoadLLAMAModelFromFile.h
#pragma once

#include "ResourceLoader.h"

#include <memory>
#include <string>

#include "../Data/llamaModelData.h"

#include <llama/llama.h>
#include <llama/llama-model.h>
#include <llama/llama-model-loader.h>

inline std::shared_ptr<LLAMAModelData> LoadLLAMAModelFromFile(const std::string& filePath) {
    std::shared_ptr<LLAMAModelData> modelData = std::make_shared<LLAMAModelData>();
    modelData->m_path = filePath;

    OutputDebugStringA(("Loading LLAMA model: " + filePath + "\n").c_str());

    llama_model_params mParams = llama_model_default_params();
    mParams.n_gpu_layers = 0; // GPUなし

    modelData->m_model = llama_model_load_from_file(filePath.c_str(), mParams);
    if (!modelData->m_model) {
        OutputDebugStringA(("Failed to load LLAMA model: " + filePath + "\n").c_str());
        return nullptr;
    }

    // llama_vocab は model から取得
    modelData->m_vocab = llama_model_get_vocab(modelData->m_model);

    return modelData;
}

// ResourceLoader 用の設定
template<>
inline void ResourceLoader<LLAMAModelData>::SetupLoadFunc(void* /*unused*/) {
    OutputDebugStringA("SetupLoadFunc LLAMAModelData called\n");

    SetLoadFunction([](const std::string& path, void*) {
        return LoadLLAMAModelFromFile(path);
    });
}
