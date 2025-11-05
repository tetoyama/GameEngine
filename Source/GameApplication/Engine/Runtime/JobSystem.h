// JobSystem.h
#pragma once
#include <functional>
class JobSystem {
public:
    JobSystem();
    ~JobSystem();
    void EnqueueJob(std::function<void()> job);
};
