// =======================================================================
// 
// SourceAnalyzer.h
// 
// =======================================================================
#pragma once
#include "../InterFace/IAnalyzer.h"

// ソースコード解析クラス
class SourceAnalyzer : public IAnalyzer{
public:
	SourceAnalyzer();
	~SourceAnalyzer(){}

	virtual void RunAsync() override{}
private:

};
