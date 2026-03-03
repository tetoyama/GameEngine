// =======================================================================
// 
// llamaModelData.h
// 
// =======================================================================
#pragma once

#include <string>
#include <memory>

// llama forward declarations
struct llama_model;
struct llama_vocab;

struct LLAMAModelData {
public:
    LLAMAModelData();
    ~LLAMAModelData();

public:
    std::string m_path;
    llama_model* m_model = nullptr;
    const llama_vocab* m_vocab = nullptr;
};
