#pragma once
class ISystem
{
public:
	virtual ~ISystem(){}

	virtual void Initialize() = 0;
	virtual void Finalize() = 0;
	
	virtual void Start() = 0;
	virtual void Update(float deltaTime) = 0;
	virtual void FixedUpdate(float fixedDeltaTime) = 0;
	virtual void Draw() = 0;

	virtual void EditorUpdate(float deltaTime) = 0;
};
