// =======================================================================
// 
// RenderableSprite.h
// 
// =======================================================================
#pragma once
#include "../IRenderable.h"
class ComponentRegistry;

struct RenderPassContext;
struct SceneManagerContext;
class MeshRendererComponent;

// 2Dスプライトの描画を行うレンダラブル
class RenderableSprite: public IRenderable {
public:
	void Initialize(SceneManagerContext* context) override;
	void Finalize() override;

	void Execute(
		const RenderPassContext& ctx,
		SceneContext* sceneContext,
		const Entity& entity
	)override;

private:
	MeshRendererComponent* m_spriteMesh = nullptr;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> m_instanceVS;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> m_instanceLayout;
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_instanceBuffer;
};
