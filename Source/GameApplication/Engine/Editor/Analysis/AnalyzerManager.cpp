// =======================================================================
// 
// AnalyzerManager.cpp
// 
// =======================================================================
#include "AnalyzerManager.h"
#include "SourceAnalyzer.h"
#include "DebugTools/DebugSystem.h"

void AnalyzerManager::Initialize(const AnalyzerManagerContext& context) {

    pDebug = pContext.pDebug;

    Register<SourceAnalyzer>("SourceAnalyzer");
    // Register<AssetAnalyzer>("AssetAnalyzer"); // 将来
}


void AnalyzerManager::Finalize() {
    m_analyzers.clear();
}

void AnalyzerManager::RunAll() {
    for (auto& [_, entry] : m_analyzers) {
        entry.instance->RunAsync();
    }
}
