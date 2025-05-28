#pragma once
class ISystem
{
public:

	virtual void Initialize(){}
	virtual void Finalize(){}
	
	virtual void Update(){}
	virtual void Draw(){}

};
