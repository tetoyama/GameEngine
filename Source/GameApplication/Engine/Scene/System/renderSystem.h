// Engine/Scene/System/renderSystem.h
#pragma once
#include "Interface/ISystem.h"
#include "d3d11.h"

class EntityRegistry;
class MainRenderer;

class TransformComponent;
class MeshRendererComponent;
class ModelRendererComponent;

class RenderSystem : public ISystem{
public:
	RenderSystem(EntityRegistry* registry, MainRenderer* renderer)
		: m_registry(registry), m_renderer(renderer){
		Initialize();
	}
	~RenderSystem(){}

	void Initialize() override{};
	void Finalize()override {};

	void Start() override {};
	void Update(float deltaTime) override{};
	void FixedUpdate(float fidedDeltaTime) override {}
	void Draw() override;

private:

	void DrawMesh(TransformComponent* pTransform, MeshRendererComponent* pMesh);
	void DrawModel(TransformComponent* pTransform, ModelRendererComponent* pMesh);

	EntityRegistry* m_registry = nullptr;
	MainRenderer* m_renderer = nullptr;
};
