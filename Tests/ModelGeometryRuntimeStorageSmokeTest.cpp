#include <array>
#include <cassert>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include <d3d11.h>
#include <wrl/client.h>

#include "Engine/Scene/Component/modelRendererComponent.h"
#include "Engine/Scene/System/Render/Model/ModelGeometryRuntimeStorage.h"
#include "Engine/Scene/System/Render/Model/ModelGeometryRuntimeTaskRegistrar.h"
#include "Service/Graphics/RHI/D3D11/D3D11RHIDevice.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace {

std::string ReadTextFile(const char* path){
	std::ifstream stream(path, std::ios::binary);
	assert(stream && "contract source file must exist");
	return std::string(
		std::istreambuf_iterator<char>(stream),
		std::istreambuf_iterator<char>()
	);
}

struct NativeDeviceContext {
	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
};

NativeDeviceContext CreateWarpDevice(){
	NativeDeviceContext result;
	D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
	constexpr D3D_FEATURE_LEVEL requestedLevels[]{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0
	};
	HRESULT createResult = D3D11CreateDevice(
		nullptr,
		D3D_DRIVER_TYPE_WARP,
		nullptr,
		0,
		requestedLevels,
		static_cast<UINT>(std::size(requestedLevels)),
		D3D11_SDK_VERSION,
		result.device.GetAddressOf(),
		&featureLevel,
		result.context.GetAddressOf()
	);
	if(createResult == E_INVALIDARG){
		result.device.Reset();
		result.context.Reset();
		constexpr D3D_FEATURE_LEVEL fallbackLevel = D3D_FEATURE_LEVEL_11_0;
		createResult = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_WARP,
			nullptr,
			0,
			&fallbackLevel,
			1,
			D3D11_SDK_VERSION,
			result.device.GetAddressOf(),
			&featureLevel,
			result.context.GetAddressOf()
		);
	}
	assert(SUCCEEDED(createResult));
	assert(result.device);
	assert(result.context);
	assert(featureLevel >= D3D_FEATURE_LEVEL_11_0);
	return result;
}

struct FakeRenderSystem {
	int synchronizationCount = 0;

	void SynchronizeModelGeometryRuntime(){
		++synchronizationCount;
	}
};

void ValidateTaskContract(){
	const SystemAccess access = ModelGeometryRuntimeTaskRegistrar::BuildAccess();
	assert(access.componentReads.contains(typeid(ModelRendererComponent)));
	assert(access.resourceReads.contains(typeid(ModelData)));
	assert(access.resourceReads.contains(typeid(RenderPacketFrameBuffer)));
	assert(access.resourceWrites.contains(typeid(ModelGeometryRuntimeStorage)));
	assert(access.resourceWrites.contains(typeid(GraphicsContext)));
	assert(access.worldAccess == WorldAccessMode::None);
	assert(access.structuralAccess == StructuralAccess::None);

	FakeRenderSystem system;
	std::vector<SystemTask> tasks;
	SystemScheduleBuilder builder(nullptr, 9, tasks);
	ModelGeometryRuntimeTaskRegistrar::Register(system, builder);
	assert(tasks.size() == 1);
	const SystemTask& task = tasks.front();
	assert(task.name == "RenderSystem.ModelGeometry.Synchronize");
	assert(task.domain == SystemTaskDomain::Render);
	assert(task.order.phase == SystemPhase::Default);
	assert(task.order.priority == -100);
	assert(task.threadAffinity == ThreadAffinity::MainThread);
	task.execute({});
	assert(system.synchronizationCount == 1);
}

void ValidateRuntimeLifecycle(){
	NativeDeviceContext native = CreateWarpDevice();
	RHI::SwapChainDesc swapChainDesc;
	swapChainDesc.width = 64;
	swapChainDesc.height = 64;
	swapChainDesc.bufferCount = 2;
	swapChainDesc.format = RHI::Format::RGBA8_UNorm;
	RHI::D3D11RHIDevice device(
		native.device.Get(),
		native.context.Get(),
		nullptr,
		swapChainDesc
	);

	ModelData* rawModel = new ModelData();
	std::shared_ptr<ModelData> model(
		rawModel,
		[](ModelData* unused){ (void)unused; }
	);
	model->MeshGeometry.resize(1);
	model->MeshGeometry[0].vertices.resize(3);
	model->MeshGeometry[0].indices = {0, 1, 2};

	ModelRendererComponent renderer;
	renderer.model = model;
	RenderPacket packet;
	packet.kind = RenderPacketKind::Model;
	packet.bindings.modelRenderer = &renderer;
	const std::array<RenderPacket, 1> packets{packet};

	ModelGeometryRuntimeStorage storage;
	storage.Synchronize(device, packets, 1);
	assert(storage.Size() == 1);
	const ModelGeometryRuntime* runtime = storage.Find(model.get());
	assert(runtime);
	assert(runtime->MeshCount() == 1);
	const ModelGeometryRuntimeMesh* mesh = runtime->Mesh(0);
	assert(mesh);
	assert(mesh->IsReady());
	assert(mesh->vertexCount == 3);
	assert(mesh->indexCount == 3);
	assert(device.NativeBuffer(mesh->vertexBuffer) != nullptr);
	assert(device.NativeBuffer(mesh->indexBuffer) != nullptr);

	const RHI::BufferHandle vertexHandle = mesh->vertexBuffer;
	const RHI::BufferHandle indexHandle = mesh->indexBuffer;
	const RHI::BufferDesc* vertexDesc = device.GetBufferDesc(vertexHandle);
	const RHI::BufferDesc* indexDesc = device.GetBufferDesc(indexHandle);
	assert(vertexDesc);
	assert(indexDesc);
	assert(vertexDesc->usage == RHI::ResourceUsage::Immutable);
	assert(vertexDesc->initialState == RHI::ResourceState::VertexBuffer);
	assert(RHI::HasAnyFlag(vertexDesc->bindFlags, RHI::BufferBindFlags::Vertex));
	assert(indexDesc->usage == RHI::ResourceUsage::Immutable);
	assert(indexDesc->initialState == RHI::ResourceState::IndexBuffer);
	assert(RHI::HasAnyFlag(indexDesc->bindFlags, RHI::BufferBindFlags::Index));

	storage.Synchronize(device, packets, 2);
	assert(storage.Size() == 1);
	assert(storage.Find(model.get())->Mesh(0)->vertexBuffer == vertexHandle);
	assert(storage.Find(model.get())->Mesh(0)->indexBuffer == indexHandle);

	const std::array<RenderPacket, 0> noPackets{};
	storage.Synchronize(device, noPackets, 3);
	assert(storage.Size() == 0);
	assert(storage.Find(model.get()) == nullptr);
	assert(device.NativeBuffer(vertexHandle) == nullptr);
	assert(device.NativeBuffer(indexHandle) == nullptr);

	const ModelGeometryRuntimeStorageTelemetry telemetry = storage.Telemetry();
	assert(telemetry.synchronizationCount == 3);
	assert(telemetry.creationCount == 1);
	assert(telemetry.reuseCount == 1);
	assert(telemetry.releaseCount == 1);
}

void ValidateRenderableConnection(){
	const std::string renderable = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/RenderSystem/Renderable/Model/RenderableModel.cpp"
	);
	const std::string renderSystem = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/RenderSystem/renderSystem.cpp"
	);

	assert(renderable.find("GetModelGeometryRuntime().Find(model)") !=
		std::string::npos);
	assert(renderable.find("d3d11RhiDevice->NativeBuffer") !=
		std::string::npos);
	assert(renderable.find("sharedVertexBuffer") != std::string::npos);
	assert(renderable.find("sharedIndexBuffer") != std::string::npos);
	assert(renderable.find("hasLegacyStaticVertexBuffer") !=
		std::string::npos);
	assert(renderSystem.find("ModelGeometryRuntimeTaskRegistrar::Register") !=
		std::string::npos);
}

} // namespace

int main(){
	ValidateTaskContract();
	ValidateRuntimeLifecycle();
	ValidateRenderableConnection();
	return 0;
}
