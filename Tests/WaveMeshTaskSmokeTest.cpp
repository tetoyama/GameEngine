#include <cassert>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "Engine/Scene/System/Render/Terrain/WaveMeshBuilder.h"
#include "Engine/Scene/System/Render/Terrain/WaveTaskRegistrar.h"

namespace {

class MockWaveSystem final : public ISystem {
public:
	const char* GetSystemName() const override {
		return "MockWaveSystem";
	}

	void BuildWaveVertices(){ ++buildCalls; }
	void UploadWaveVertices(){ ++uploadCalls; }

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

void ValidateBuilder(){
	std::vector<VERTEX_3D> vertices;
	std::vector<std::uint32_t> indices;
	assert(WaveMeshBuilder::BuildTopology(2, vertices, indices));
	assert(vertices.size() == 9);
	assert(indices.size() == 24);
	assert(vertices.front().Position.x == -1.0f);
	assert(vertices.front().Position.y == 0.0f);
	assert(vertices.front().Position.z == -1.0f);
	assert(vertices.back().Position.x == 1.0f);
	assert(vertices.back().Position.z == 1.0f);

	assert(WaveMeshBuilder::BuildAnimatedVertices(
		2,
		0.5f,
		2.0f,
		0.25f,
		vertices
	));
	assert(vertices.size() == 9);
	for(const VERTEX_3D& vertex : vertices){
		assert(std::isfinite(vertex.Position.y));
	}

	const std::uint64_t initialSignature =
		WaveMeshBuilder::ComputeInputSignature(2, 0.5f, 2.0f, 0.25f);
	assert(initialSignature !=
		WaveMeshBuilder::ComputeInputSignature(3, 0.5f, 2.0f, 0.25f));
	assert(initialSignature !=
		WaveMeshBuilder::ComputeInputSignature(2, 0.6f, 2.0f, 0.25f));
	assert(initialSignature !=
		WaveMeshBuilder::ComputeInputSignature(2, 0.5f, 2.0f, 0.5f));

	indices.resize(1);
	assert(!WaveMeshBuilder::BuildTopology(0, vertices, indices));
	assert(vertices.empty());
	assert(indices.empty());
	vertices.resize(1);
	assert(!WaveMeshBuilder::BuildAnimatedVertices(
		2,
		0.5f,
		0.0f,
		0.25f,
		vertices
	));
	assert(vertices.empty());
}

void ValidateTaskRegistrar(){
	MockWaveSystem system;
	std::vector<SystemTask> tasks;
	SystemScheduleBuilder builder(&system, 15, tasks);

	WaveTaskRegistrar::Register(system, builder);
	assert(tasks.size() == 2);

	const SystemTask& build = tasks[0];
	assert(build.name == "WaveSystem.Vertex.Build");
	assert(build.domain == SystemTaskDomain::Render);
	assert(build.order.phase == SystemPhase::Earliest);
	assert(build.threadAffinity == ThreadAffinity::AnyWorker);
	assert(build.access.componentWrites.contains(typeid(WaveComponent)));
	assert(build.access.resourceReads.contains(typeid(SceneManager)));

	const SystemTask& upload = tasks[1];
	assert(upload.name == "WaveSystem.Vertex.Upload");
	assert(upload.domain == SystemTaskDomain::Render);
	assert(upload.order.phase == SystemPhase::Early);
	assert(upload.threadAffinity == ThreadAffinity::MainThread);
	assert(upload.access.componentWrites.contains(typeid(WaveComponent)));
	assert(upload.access.resourceReads.contains(typeid(SceneManager)));
	assert(upload.access.resourceWrites.contains(typeid(GraphicsContext)));
}

void ValidateSystemContract(){
	const std::string systemSource = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/Terrain/waveSystem.h"
	);
	const std::size_t initialize =
		RequireToken(systemSource, "void Initialize() override");
	const std::size_t registerTasks =
		RequireToken(systemSource, "void RegisterTasks(", initialize);
	const std::size_t initialMesh =
		RequireToken(systemSource, "InitializeWaveMeshes();", initialize);
	assert(initialMesh < registerTasks);

	const std::size_t buildFunction =
		RequireToken(systemSource, "void BuildWaveVertices()");
	const std::size_t uploadFunction =
		RequireToken(systemSource, "void UploadWaveVertices()");
	assert(buildFunction < uploadFunction);
	const std::string buildBody = systemSource.substr(
		buildFunction,
		uploadFunction - buildFunction
	);
	assert(buildBody.find("GetDevice") == std::string::npos);
	assert(buildBody.find("Map(") == std::string::npos);
	assert(buildBody.find("WaveMeshBuilder::BuildAnimatedVertices") !=
		std::string::npos);

	const std::size_t uploadCall = RequireToken(
		systemSource,
		"WaveMeshUpload::UploadVertices(",
		uploadFunction
	);
	const std::size_t timeAdvance = RequireToken(
		systemSource,
		"component->Time += 0.02f * component->Speed;",
		uploadCall
	);
	assert(uploadCall < timeAdvance);
}

void ValidateTransactionalUploadAndRenderGuard(){
	const std::string uploadSource = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/Terrain/WaveMeshUpload.h"
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

	const std::string renderSource = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/RenderSystem/Renderable/Wave/RenderableWave.cpp"
	);
	const std::size_t vertexGuard =
		RequireToken(renderSource, "if(!vertexBuffer || !indexBuffer");
	const std::size_t draw = RequireToken(renderSource, "DrawIndexed(");
	assert(vertexGuard < draw);
}

} // namespace

int main(){
	ValidateBuilder();
	ValidateTaskRegistrar();
	ValidateSystemContract();
	ValidateTransactionalUploadAndRenderGuard();
	return 0;
}
