#pragma once
class IAnalyzer {
public:
    virtual ~IAnalyzer() = default;
    virtual void RunAsync() = 0;
};
