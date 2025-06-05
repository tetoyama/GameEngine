#pragma once
class ISystem
{
public:

	virtual void Initialize(){}
	virtual void Finalize(){}
	
	virtual void Update(float deltaTime){}
	virtual void Draw(){}

};
