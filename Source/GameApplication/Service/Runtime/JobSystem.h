// =======================================================================
// 
// JobSystem.h
// 
// =======================================================================
#pragma once
#include <functional>
// 非同期ジョブを管理・実行するシステム
class JobSystem {
public:
    JobSystem();
    ~JobSystem();
    void EnqueueJob(std::function<void()> job);
};
