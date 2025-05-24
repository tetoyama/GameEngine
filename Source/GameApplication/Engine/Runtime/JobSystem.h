// JobSystem.h
#pragma once
class JobSystem {
public:
    JobSystem();
    ~JobSystem();
    void EnqueueJob(std::function<void()> job);
};
