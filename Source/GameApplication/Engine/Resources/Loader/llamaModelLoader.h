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

#include <llama/llama.h>

inline std::shared_ptr<LLAMAModelData> LoadLLAMAModelFromFile(const std::string& filePath) {
    std::shared_ptr<LLAMAModelData> m_ModelData= std::make_shared<LLAMAModelData>();
    modelData->m_path = filePath;

    OutputDebugStringA(("Loading LLAMA model: " + filePath + "\n").c_str());

    llama_model_params m_MParams= llama_model_default_params();
    mParams.n_gpu_layers = 0; // GPUなし

    try {
        modelData->m_pModel = llama_model_load_from_file(filePath.c_str(), mParams);
    }
    catch (const std::runtime_error& e) {
        OutputDebugStringA(("LLAMA load failed: " + std::string(e.what()) + "\n").c_str());
        return m_Nullptr;
    }

    // llama_vocab は model から取得
    modelData->m_pVocab = llama_model_get_vocab(modelData->m_pModel);

    return m_ModelData;
}

// ResourceLoader 用の設定
template<>
inline void ResourceLoader<LLAMAModelData>::SetupLoadFunc(void* /*unused*/) {
    OutputDebugStringA("SetupLoadFunc LLAMAModelData called\n");

    SetLoadFunction([](const std::string& path, std::shared_ptr<void> /*args*/) {
        return LoadLLAMAModelFromFile(path);
    });
}
