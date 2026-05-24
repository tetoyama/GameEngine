// =======================================================================
// 
// AnalyzerManager.h
// 
// =======================================================================
#pragma once
#include <vector>
#include <memory>
#include <typeindex>
#include <type_traits>
#include <string>
#include <unordered_map>
#include <utility>

#include "../InterFace/IAnalyzer.h"

class DebugLogService;

struct AnalyzerEntry {
    std::string name;                      // 外部識別子
    std::unique_ptr<IAnalyzer> instance;   // 実体
};

struct AnalyzerManagerContext {
    DebugLogService* pDebug = nullptr;
};

class AnalyzerManager {
public:
    void Initialize(const AnalyzerManagerContext& context);
    void Finalize();

    template<class T>
    void Run() {
        auto key = std::type_index(typeid(T));
        auto it = m_Analyzers.find(key);
        if (it == m_Analyzers.end())
            return;

        it->second.instance->RunAsync();
    }

    void RunAll();

private:
    template<class T, class... Args>
    void Register(const std::string& name, Args&&... args) {
        static_assert(std::is_base_of_v<IAnalyzer, T>);

        auto key = std::type_index(typeid(T));
        AnalyzerEntry entry;
        entry.name = name;
        entry.instance = std::make_unique<T>(std::forward<Args>(args)...);

        m_Analyzers.emplace(key, std::move(entry));
    }

private:
    std::unordered_map<std::type_index, AnalyzerEntry> m_Analyzers;

    DebugLogService* m_pDebug = nullptr;
};
