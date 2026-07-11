#include <cassert>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "Engine/Scene/System/Render/Terrain/TerrainMeshBuilder.h"
#include "Engine/Scene/System/Render/Terrain/TerrainTaskRegistrar.h"

namespace {

class MockTerrainSystem final : public ISystem {
public:
	const char* GetSystemName() const override {
		return "MockTerrainSystem";
	}

	void BuildTerrainMeshes(){ ++buildCalls; }
	void UploadTerrainMeshes(){ ++uploadCalls; }

	int buildCalls = 0;
	int uploadCalls = 0;
};

std::string ReadTextFile(const char* path){
	std::ifstream stream(path, std::ios::binary);
	assert(stream && "contract source file must exist");
	return std::string(
		std::istreambuf_iterator<char>(stream),
		std::istreambuf_iterator<char>()
	);
}

std::size_t RequireToken(
	const std::string& text,
	const std::string& token,
	std::size_t offset = 0
){
	const std::size_t position = text.find(token, offset);
	assert(position != std::string::npos);
	return position;
}

void ValidateMeshBuilder(){
	const std::vector<float> heights{
		0.0f, 1.0f, 2.0f,
		3.0f, 4.0f, 5.0f,
		6.0f, 7.0f, 8.0f
	};

	std::vector<VERTEX_3D> vertices;
	std::vector<std::uint32_t> indices;
	assert(TerrainMeshBuilder::Build(2, heights, vertices, indices));
	assert(vertices.size() == 9);
	assert(indices.size() == 24);

	// HeightMapのZ方向反転を含む旧Terrain生成契約を維持する。
	assert(vertices.front().Position.x == -0.5f);
	assert(vertices.front().Position.y == 6.0f);
	assert(vertices.front().Position.z == -0.5f);
	assert(vertices.back().Position.x == 0.5f);
	assert(vertices.back().Position.y == 2.0f);
	assert(vertices.back().Position.z == 0.5f);

	const std::uint64_t initialSignature =
		TerrainMeshBuilder::ComputeSignature(2, heights);
	auto changedHeights = heights;
	changedHeights[4] += 0.25f;
	assert(initialSignature !=
		TerrainMeshBuilder::ComputeSignature(2, changedHeights));
	assert(initialSignature !=
		TerrainMeshBuilder::ComputeSignature(3, heights));

	vertices.resize(1);
	indices.resize(1);
	assert(!TerrainMeshBuilder::Build(0, heights, vertices, indices));
	assert(vertices.empty());
	assert(indices.empty());
}

void ValidateTaskRegistrar(){
	MockTerrainSystem system;
	std::vector<SystemTask> tasks;
	SystemScheduleBuilder builder(&system, 12, tasks);

	TerrainTaskRegistrar::Register(system, builder);
	assert(tasks.size() == 2);

	const SystemTask& build = tasks[0];
	assert(build.name == "TerrainSystem.Mesh.Build");
	assert(build.domain == SystemTaskDomain::Render);
	assert(build.order.phase == SystemPhase::Earliest);
	assert(build.threadAffinity == ThreadAffinity::AnyWorker);
	assert(build.access.componentWrites.contains(typeid(TerrainComponent)));
	assert(build.access.resourceReads.contains(typeid(SceneManager)));

	const SystemTask& upload = tasks[1];
	assert(upload.name == "TerrainSystem.Mesh.Upload");
	assert(upload.domain == SystemTaskDomain::Render);
	assert(upload.order.phase == SystemPhase::Early);
	assert(upload.threadAffinity == ThreadAffinity::MainThread);
	assert(upload.access.componentWrites.contains(typeid(TerrainComponent)));
	assert(upload.access.componentWrites.contains(typeid(ColliderComponent)));
	assert(upload.access.resourceReads.contains(typeid(SceneManager)));
	assert(upload.access.resourceWrites.contains(typeid(GraphicsContext)));
}

void ValidateInitializationAndRetryContract(){
	const std::string systemSource = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/Terrain/terrainSystem.h"
	);
	const std::size_t initialize =
		RequireToken(systemSource, "void Initialize() override");
	const std::size_t finalize =
		RequireToken(systemSource, "void Finalize() override", initialize);
	const std::size_t initialBuild =
		RequireToken(systemSource, "BuildTerrainMeshes();", initialize);
	const std::size_t initialUpload =
		RequireToken(systemSource, "UploadTerrainMeshes();", initialBuild);
	assert(initialBuild < initialUpload);
	assert(initialUpload < finalize);

	const std::size_t uploadFunction =
		RequireToken(systemSource, "void UploadTerrainMeshes()");
	const std::size_t uploadCall = RequireToken(
		systemSource,
		"TerrainMeshUpload::Upload(",
		uploadFunction
	);
	const std::size_t successReset = RequireToken(
		systemSource,
		"comp->meshBuildReady = false;",
		uploadCall
	);
	const std::size_t functionEnd =
		RequireToken(systemSource, "private:", uploadFunction);
	assert(uploadCall < successReset);
	assert(successReset < functionEnd);

	// Upload失敗時は成功ブロック外でstagingを破棄しない。
	const std::string failureTail = systemSource.substr(
		successReset,
		functionEnd - successReset
	);
	assert(failureTail.find("else{") == std::string::npos);
}

void ValidateTransactionalGpuCommit(){
	const std::string uploadSource = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/Terrain/TerrainMeshUpload.h"
	);
	const std::size_t firstCreate =
		RequireToken(uploadSource, "device->CreateBuffer(");
	const std::size_t secondCreate = RequireToken(
		uploadSource,
		"device->CreateBuffer(",
		firstCreate + 1
	);
	const std::size_t vertexCommit = RequireToken(
		uploadSource,
		"renderer.mesh.m_VertexBuffer = std::move(newVertexBuffer);"
	);
	const std::size_t indexCommit = RequireToken(
		uploadSource,
		"renderer.mesh.m_IndexBuffer = std::move(newIndexBuffer);"
	);
	assert(secondCreate < vertexCommit);
	assert(secondCreate < indexCommit);

	const std::string beforeCommit = uploadSource.substr(0, vertexCommit);
	assert(beforeCommit.find("renderer.mesh.m_VertexBuffer.Reset()") ==
		std::string::npos);
	assert(beforeCommit.find("renderer.mesh.m_IndexBuffer.Reset()") ==
		std::string::npos);
}

void ValidateRenderableBufferGuard(){
	const std::string renderableSource = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/RenderSystem/Renderable/Terrain/RenderableTerrain.cpp"
	);
	const std::size_t vertexGuard = RequireToken(
		renderableSource,
		"!meshRenderer->mesh.m_VertexBuffer"
	);
	const std::size_t indexGuard = RequireToken(
		renderableSource,
		"!meshRenderer->mesh.m_IndexBuffer"
	);
	const std::size_t countGuard = RequireToken(
		renderableSource,
		"meshRenderer->mesh.indexCount <= 0"
	);
	const std::size_t draw =
		RequireToken(renderableSource, "DrawIndexed(");
	assert(vertexGuard < draw);
	assert(indexGuard < draw);
	assert(countGuard < draw);
}

} // namespace

int main(){
	ValidateMeshBuilder();
	ValidateTaskRegistrar();
	ValidateInitializationAndRetryContract();
	ValidateTransactionalGpuCommit();
	ValidateRenderableBufferGuard();
	return 0;
}
