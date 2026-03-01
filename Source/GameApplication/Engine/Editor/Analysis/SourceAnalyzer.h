#pragma once
#include "../InterFace/IAnalyzer.h"

class SourceAnalyzer : public IAnalyzer{
public:
	SourceAnalyzer();
	~SourceAnalyzer(){}

	virtual void RunAsync() override{}
private:

};
