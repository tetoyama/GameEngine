#include "GBufferPass.h"
#include "System/RenderSystem/renderSystem.h"
#include "sceneManager.h"

void GBufferPass::Initialize(RenderSystem* renderSystem, SceneManagerContext* context) {
	m_renderSystem = renderSystem;
	m_context = context;
}

void GBufferPass::Finalize() {

}

void GBufferPass::Execute(const RenderPassContext& ctx) {

}
