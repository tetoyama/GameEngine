#include <cassert>
#include <fstream>
#include <iterator>
#include <string>

namespace {

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

void ValidateTaskChain(){
	const std::string source = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Physic/PhysicSystemTasks.inl"
	);

	const std::size_t upload =
		RequireToken(source, "\"PhysicSystem.Scene.Upload\"");
	const std::size_t simulate =
		RequireToken(source, "\"PhysicSystem.Simulation.Simulate\"", upload);
	const std::size_t fetch =
		RequireToken(source, "\"PhysicSystem.Simulation.Fetch\"", simulate);
	const std::size_t download =
		RequireToken(source, "\"PhysicSystem.Scene.Download\"", fetch);
	const std::size_t dispatch =
		RequireToken(source, "\"PhysicSystem.Collision.Dispatch\"", download);

	assert(upload < simulate);
	assert(simulate < fetch);
	assert(fetch < download);
	assert(download < dispatch);

	const std::size_t simulateAffinity = RequireToken(
		source,
		"ThreadAffinity::AnyWorker",
		simulate
	);
	const std::size_t fetchAffinity = RequireToken(
		source,
		"ThreadAffinity::AnyWorker",
		fetch
	);
	const std::size_t downloadAffinity = RequireToken(
		source,
		"ThreadAffinity::MainThread",
		download
	);
	assert(simulateAffinity < fetch);
	assert(fetchAffinity < download);
	assert(downloadAffinity < dispatch);
}

void ValidateSameStepFetchContract(){
	const std::string source = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Physic/PhysicSystemTasks.inl"
	);
	const std::size_t beginFunction =
		RequireToken(source, "inline void PhysicSystem::PhysicsBegin(");
	const std::size_t fetchFunction =
		RequireToken(source, "inline void PhysicSystem::PhysicsFetch()", beginFunction);
	const std::size_t downloadFunction =
		RequireToken(source, "inline void PhysicSystem::PhysicsDownload()", fetchFunction);

	const std::string beginBody = source.substr(
		beginFunction,
		fetchFunction - beginFunction
	);
	const std::string fetchBody = source.substr(
		fetchFunction,
		downloadFunction - fetchFunction
	);

	assert(beginBody.find("g_pScene->simulate(fixedDeltaTime)") !=
		std::string::npos);
	assert(beginBody.find("fetchResults") == std::string::npos);
	assert(fetchBody.find("g_pScene->fetchResults(true)") !=
		std::string::npos);
	assert(fetchBody.find(
		"m_simulationInFlight.store(false, std::memory_order_release)"
	) != std::string::npos);
}

void ValidateAnalysisNamesMatchTasks(){
	const std::string taskSource = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Physic/PhysicSystemTasks.inl"
	);
	const std::string analysisSource = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Physic/PhysicsSimulationOverlapAnalysis.h"
	);

	assert(taskSource.find("PhysicSystem.Simulation.Simulate") !=
		std::string::npos);
	assert(taskSource.find("PhysicSystem.Simulation.Fetch") !=
		std::string::npos);
	assert(analysisSource.find("PhysicSystem.Simulation.Simulate") !=
		std::string::npos);
	assert(analysisSource.find("PhysicSystem.Simulation.Fetch") !=
		std::string::npos);
}

} // namespace

int main(){
	ValidateTaskChain();
	ValidateSameStepFetchContract();
	ValidateAnalysisNamesMatchTasks();
	return 0;
}
