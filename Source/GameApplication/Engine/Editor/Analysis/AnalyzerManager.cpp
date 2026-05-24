// =======================================================================
// 
// AnalyzerManager.cpp
// 
// =======================================================================
#include "AnalyzerManager.h"
#include "SourceAnalyzer.h"
#include "DebugTools/DebugSystem.h"

void AnalyzerManager::Initialize(const AnalyzerManagerContext& context) {
    m_pDebug = context.pDebug;

    Register<SourceAnalyzer>("SourceAnalyzer");
    // Register<AssetAnalyzer>("AssetAnalyzer"); // 将来
}


void AnalyzerManager::Finalize() {
    m_Analyzers.clear();
}

void AnalyzerManager::RunAll() {
    for (auto& [_, entry] : m_Analyzers) {
        entry.instance->RunAsync();
    }
}
