#include <array>
#include <cassert>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include <d3d11.h>
#include <wrl/client.h>

#include "Engine/Scene/System/Render/Model/ModelGeometryRuntimeStorage.h"
#include "Engine/Scene/System/Render/Model/ModelGeometryRuntimeTaskRegistrar.h"
#include "Service/Graphics/RHI/D3D11/D3D11RHIDevice.h"
#include "Service/Graphics/RHI/RHIService.h"

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

RHI::SwapChainDesc MakeSwapChainDesc(){
	RHI::SwapChainDesc desc;
	desc.width = 64;
	desc.height = 64;
	desc.bufferCount = 2;
	desc.format = RHI::Format::RGBA8_UNorm;
	return desc;
}

std::unique_ptr<RHI::D3D11RHIDevice> CreateRhiDevice(
	const NativeDeviceContext& native
){
	return std::make_unique<RHI::D3D11RHIDevice>(
		native.device.Get(),
		native.context.Get(),
		nullptr,
		MakeSwapChainDesc()
	);
}

std::shared_ptr<ModelData> CreateTriangleModel(){
	ModelData* rawModel = new ModelData();
	std::shared_ptr<ModelData> model(
		rawModel,
		[](ModelData* unused){ (void)unused; }
	);
	model->MeshGeometry.resize(1);
	model->MeshGeometry[0].vertices.resize(3);
	model->MeshGeometry[0].vertices[0].Position = {0.0f, 0.0f, 0.0f};
	model->MeshGeometry[0].vertices[1].Position = {1.0f, 0.0f, 0.0f};
	model->MeshGeometry[0].vertices[2].Position = {0.0f, 1.0f, 0.0f};
	model->MeshGeometry[0].indices = {0, 1, 2};
	return model;
}

struct FakeRenderSystem {
	int synchronizationCount = 0;

	void SynchronizeModelGeometryRuntime(){
		++synchronizationCount;
	}
};

void ValidateTaskContract(){
	const SystemAccess access = ModelGeometryRuntimeTaskRegistrar::BuildAccess();
	assert(access.componentReads.empty());
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

void ValidateRhiDeviceOwnershipGeneration(){
	NativeDeviceContext native = CreateWarpDevice();
	RHI::RenderHardwareInterfaceService service;
	assert(service.GetDevice() == nullptr);
	assert(service.GetDeviceGeneration() == RHI::InvalidDeviceGeneration);

	assert(service.AdoptDevice(CreateRhiDevice(native)));
	RHI::IRHIDevice* firstDevice = service.GetDevice();
	const RHI::DeviceGeneration firstGeneration =
		service.GetDeviceGeneration();
	assert(firstDevice);
	assert(firstGeneration != RHI::InvalidDeviceGeneration);

	std::unique_ptr<RHI::IRHIDevice> released = service.ReleaseDevice();
	assert(released.get() == firstDevice);
	assert(service.GetDevice() == nullptr);
	const RHI::DeviceGeneration releasedGeneration =
		service.GetDeviceGeneration();
	assert(releasedGeneration != firstGeneration);

	assert(service.AdoptDevice(std::move(released)));
	assert(service.GetDevice() == firstDevice);
	const RHI::DeviceGeneration readoptedGeneration =
		service.GetDeviceGeneration();
	assert(readoptedGeneration != releasedGeneration);

	service.ResetDevice();
	assert(service.GetDevice() == nullptr);
	assert(service.GetDeviceGeneration() != readoptedGeneration);
}

void ValidateRuntimeLifecycleAndGeometryRevision(){
	NativeDeviceContext nativeA = CreateWarpDevice();
	NativeDeviceContext nativeB = CreateWarpDevice();
	std::unique_ptr<RHI::D3D11RHIDevice> deviceA = CreateRhiDevice(nativeA);
	std::unique_ptr<RHI::D3D11RHIDevice> deviceB = CreateRhiDevice(nativeB);

	std::shared_ptr<ModelData> model = CreateTriangleModel();
	RenderPacket packet;
	packet.kind = RenderPacketKind::Model;
	packet.modelResource = model;
	const std::array<RenderPacket, 1> packets{packet};

	constexpr RHI::DeviceGeneration deviceGenerationA = 11;
	constexpr RHI::DeviceGeneration deviceGenerationB = 12;
	ModelGeometryRuntimeStorage storage;
	storage.Synchronize(*deviceA, packets, 1, deviceGenerationA);
	assert(storage.Size() == 1);
	assert(storage.IsBoundTo(*deviceA, deviceGenerationA));
	const ModelGeometryRuntime* runtime = storage.Find(model.get());
	assert(runtime);
	assert(runtime->MeshCount() == 1);
	assert(runtime->GeometryRevision() == model->GetGeometryRevision());
	const ModelGeometryRuntimeMesh* mesh = runtime->Mesh(0);
	assert(mesh);
	assert(mesh->IsReady());
	assert(mesh->vertexCount == 3);
	assert(mesh->indexCount == 3);
	assert(deviceA->NativeBuffer(mesh->vertexBuffer) != nullptr);
	assert(deviceA->NativeBuffer(mesh->indexBuffer) != nullptr);

	const RHI::BufferHandle initialVertexHandle = mesh->vertexBuffer;
	const RHI::BufferHandle initialIndexHandle = mesh->indexBuffer;
	const RHI::BufferDesc* vertexDesc = deviceA->GetBufferDesc(initialVertexHandle);
	const RHI::BufferDesc* indexDesc = deviceA->GetBufferDesc(initialIndexHandle);
	assert(vertexDesc);
	assert(indexDesc);
	assert(vertexDesc->usage == RHI::ResourceUsage::Immutable);
	assert(vertexDesc->initialState == RHI::ResourceState::VertexBuffer);
	assert(RHI::HasAnyFlag(vertexDesc->bindFlags, RHI::BufferBindFlags::Vertex));
	assert(indexDesc->usage == RHI::ResourceUsage::Immutable);
	assert(indexDesc->initialState == RHI::ResourceState::IndexBuffer);
	assert(RHI::HasAnyFlag(indexDesc->bindFlags, RHI::BufferBindFlags::Index));

	// Vertex / Index数が同じでも明示Revisionが進めばRuntimeを置換する。
	model->MeshGeometry[0].vertices[0].Position.x = 42.0f;
	const std::uint64_t changedRevision = model->MarkGeometryDirty();
	storage.Synchronize(*deviceA, packets, 2, deviceGenerationA);
	runtime = storage.Find(model.get());
	assert(runtime);
	assert(runtime->GeometryRevision() == changedRevision);
	mesh = runtime->Mesh(0);
	assert(mesh);
	assert(mesh->vertexBuffer != initialVertexHandle);
	assert(mesh->indexBuffer != initialIndexHandle);
	assert(deviceA->NativeBuffer(initialVertexHandle) == nullptr);
	assert(deviceA->NativeBuffer(initialIndexHandle) == nullptr);

	const RHI::BufferHandle deviceAVertexHandle = mesh->vertexBuffer;
	const RHI::BufferHandle deviceAIndexHandle = mesh->indexBuffer;
	storage.Synchronize(*deviceA, packets, 3, deviceGenerationA);
	assert(storage.Find(model.get())->Mesh(0)->vertexBuffer == deviceAVertexHandle);
	assert(storage.Find(model.get())->Mesh(0)->indexBuffer == deviceAIndexHandle);

	// Device Epoch変更時は旧DeviceへDestroyを発行せずAbandonし、新Deviceで再生成する。
	storage.Synchronize(*deviceB, packets, 4, deviceGenerationB);
	assert(storage.IsBoundTo(*deviceB, deviceGenerationB));
	assert(deviceA->NativeBuffer(deviceAVertexHandle) != nullptr);
	assert(deviceA->NativeBuffer(deviceAIndexHandle) != nullptr);
	const ModelGeometryRuntime* deviceBRuntime = storage.Find(model.get());
	assert(deviceBRuntime);
	assert(deviceB->NativeBuffer(deviceBRuntime->Mesh(0)->vertexBuffer) != nullptr);
	assert(deviceB->NativeBuffer(deviceBRuntime->Mesh(0)->indexBuffer) != nullptr);

	// Test内では旧Deviceを生存させているため、Abandonされた旧Handleを明示清掃する。
	assert(deviceA->DestroyBuffer(deviceAIndexHandle));
	assert(deviceA->DestroyBuffer(deviceAVertexHandle));

	assert(storage.Reset(*deviceB, deviceGenerationB));
	assert(storage.Size() == 0);
	assert(storage.BoundDeviceGeneration() == RHI::InvalidDeviceGeneration);

	const ModelGeometryRuntimeStorageTelemetry telemetry = storage.Telemetry();
	assert(telemetry.synchronizationCount == 4);
	assert(telemetry.creationCount == 2);
	assert(telemetry.reuseCount == 1);
	assert(telemetry.replacementCount == 1);
	assert(telemetry.geometryRevisionReplacementCount == 1);
	assert(telemetry.deviceTransitionCount == 1);
	assert(telemetry.abandonedEntryCount == 1);
	assert(telemetry.releaseCount == 2);
}

void ValidateExpiredDeviceAbandon(){
	std::shared_ptr<ModelData> model = CreateTriangleModel();
	RenderPacket packet;
	packet.kind = RenderPacketKind::Model;
	packet.modelResource = model;
	const std::array<RenderPacket, 1> packets{packet};

	ModelGeometryRuntimeStorage storage;
	NativeDeviceContext native = CreateWarpDevice();
	{
		std::unique_ptr<RHI::D3D11RHIDevice> device = CreateRhiDevice(native);
		storage.Synchronize(*device, packets, 1, 21);
		assert(storage.Size() == 1);
		assert(!device->GetLifetimeToken().expired());
	}

	// DeviceのResource Poolは既に破棄済み。Resetは保存済みPointerを触らず
	// EntryをAbandonし、失敗を診断へ残す。
	assert(!storage.Reset());
	assert(storage.Size() == 0);
	const ModelGeometryRuntimeStorageTelemetry telemetry = storage.Telemetry();
	assert(telemetry.abandonedEntryCount == 1);
	assert(telemetry.resetFailureCount == 1);
}

void ValidateRenderableConnection(){
	const std::string renderable = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/RenderSystem/Renderable/Model/RenderableModel.cpp"
	);
	const std::string renderSystem = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/RenderSystem/renderSystem.cpp"
	);
	const std::string extraction = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/RenderSystem/RenderWorld/RenderWorldExtraction.h"
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
	assert(renderSystem.find("GetDeviceGeneration()") != std::string::npos);
	assert(extraction.find("packet.modelResource = modelRenderer->model") !=
		std::string::npos);
}

} // namespace

int main(){
	ValidateTaskContract();
	ValidateRhiDeviceOwnershipGeneration();
	ValidateRuntimeLifecycleAndGeometryRevision();
	ValidateExpiredDeviceAbandon();
	ValidateRenderableConnection();
	return 0;
}
