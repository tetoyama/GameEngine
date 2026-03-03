// =======================================================================
// 
// IAnalyzer.h
// 
// =======================================================================
#pragma once
// 解析処理の基底インターフェース
class IAnalyzer {
public:
    virtual ~IAnalyzer() = default;
    virtual void RunAsync() = 0;
};
