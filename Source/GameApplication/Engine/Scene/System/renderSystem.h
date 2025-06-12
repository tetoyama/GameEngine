// Engine/Scene/System/renderSystem.h
#pragma once
#include "Interface/ISystem.h"
#include "d3d11.h"

struct SceneContext;

class TransformComponent;
class TextureComponent;
class MeshRendererComponent;
class ModelRendererComponent;
class BillBoardRendererComponent;

class RenderSystem : public ISystem{
public:
	RenderSystem(SceneContext* context): m_context(context){}
	~RenderSystem(){}

	void Initialize() override;
	void Finalize()override {};

	void Start() override {};
	void Update(float deltaTime) override{};
	void FixedUpdate(float fidedDeltaTime) override {}
	void Draw() override;

private:

	void DrawMesh(TransformComponent* pTransform, MeshRendererComponent* pMesh, TextureComponent* pTexture);
	void DrawModel(TransformComponent* pTransform, ModelRendererComponent* pMesh, TextureComponent* pTexture);
	void DrawBillBoard(TransformComponent* pTransform, MeshRendererComponent* pMesh, BillBoardRendererComponent* pBillBoard, TextureComponent* pTexture);

	SceneContext* m_context;
};
