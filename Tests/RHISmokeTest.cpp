#include <cassert>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

#include "Service/Graphics/RHI/RHIResourcePool.h"
#include "Service/Graphics/RHI/RHIRenderGraph.h"
#include "Service/Graphics/RHI/D3D11/D3D11RHIDevice.h"

namespace {

class MockCommandList final : public RHI::IRHICommandList {
public:
	void Begin() override { ++beginCount; }
	void End() override { ++endCount; }
	bool BeginRenderPass(const RHI::RenderPassDesc&) override { return true; }
	void EndRenderPass() override {}
	bool SetPipelineState(RHI::PipelineStateHandle) override { return true; }
	void SetViewport(const RHI::Viewport&) override {}
	bool SetVertexBuffer(uint32_t, RHI::BufferHandle, uint32_t, uint32_t) override { return true; }
	bool SetIndexBuffer(RHI::BufferHandle, RHI::IndexFormat, uint32_t) override { return true; }
	bool SetConstantBuffer(RHI::ShaderStage, uint32_t, RHI::BufferHandle) override { return true; }
	bool SetTexture(RHI::ShaderStage, uint32_t, RHI::TextureHandle) override { return true; }
	bool UpdateBuffer(RHI::BufferHandle, std::span<const std::byte>, uint32_t) override { return true; }
	void Draw(uint32_t, uint32_t) override { ++drawCount; }
	void DrawIndexed(uint32_t, uint32_t, int32_t) override {}
	void Dispatch(uint32_t, uint32_t, uint32_t) override {}

	int beginCount = 0;
	int endCount = 0;
	int drawCount = 0;
};

void TestGenerationalHandles(){
	RHI::ResourcePool<RHI::BufferHandle, int> pool;
	const RHI::BufferHandle first = pool.Create(10);
	assert(first);
	assert(pool.IsAlive(first));
	assert(*pool.TryGet(first) == 10);
	assert(pool.Destroy(first));
	assert(!pool.IsAlive(first));

	const RHI::BufferHandle second = pool.Create(20);
	assert(second);
	assert(second.index == first.index);
	assert(second.generation != first.generation);
	assert(pool.TryGet(first) == nullptr);
	assert(*pool.TryGet(second) == 20);
}

void TestRenderGraphExecution(){
	RHI::RenderGraph graph;
	const auto gbuffer = graph.AddResource("GBuffer");
	const auto lighting = graph.AddResource("Lighting");
	std::vector<int> execution;

	graph.AddPass(
		"GBuffer",
		[&](RHI::RenderGraphPassBuilder& builder){ builder.Write(gbuffer); },
		[&](RHI::IRHICommandList& commands){ execution.push_back(1); commands.Draw(3, 0); }
	);
	graph.AddPass(
		"Lighting",
		[&](RHI::RenderGraphPassBuilder& builder){
			builder.Read(gbuffer);
			builder.Write(lighting);
		},
		[&](RHI::IRHICommandList&){ execution.push_back(2); }
	);
	graph.AddPass(
		"Present",
		[&](RHI::RenderGraphPassBuilder& builder){ builder.Read(lighting); },
		[&](RHI::IRHICommandList&){ execution.push_back(3); }
	);

	assert(graph.Compile());
	assert(graph.ExecutionOrder().size() == 3);

	MockCommandList commands;
	assert(graph.Execute(commands));
	assert(commands.beginCount == 1);
	assert(commands.endCount == 1);
	assert(commands.drawCount == 1);
	assert((execution == std::vector<int>{1, 2, 3}));
}

void TestRenderPassDescriptor(){
	RHI::RenderPassDesc pass;
	pass.debugName = "Main";
	pass.colorAttachments.push_back({});
	pass.colorAttachments[0].loadOperation = RHI::LoadOperation::Clear;
	pass.colorAttachments[0].clearColor = {0.1f, 0.2f, 0.3f, 1.0f};
	pass.hasDepthAttachment = true;
	pass.depthAttachment.depthLoadOperation = RHI::LoadOperation::Clear;
	assert(pass.colorAttachments.size() == 1);
	assert(pass.hasDepthAttachment);
}

} // namespace

static_assert(std::is_base_of_v<RHI::IRHIDevice, RHI::D3D11RHIDevice>);
static_assert(std::is_base_of_v<RHI::IRHICommandList, RHI::D3D11RHICommandList>);
static_assert(std::is_base_of_v<RHI::IRHISwapChain, RHI::D3D11RHISwapChain>);

int main(){
	TestGenerationalHandles();
	TestRenderGraphExecution();
	TestRenderPassDescriptor();
	return 0;
}
