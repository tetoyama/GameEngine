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
	const std::string header = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Physic/physicSystem.h"
	);
	const std::size_t beginFunction =
		RequireToken(source, "inline void PhysicSystem::PhysicsBegin(");
	const std::size_t fetchFunction =
		RequireToken(source, "inline void PhysicSystem::PhysicsFetch()", beginFunction);
	const std::size_t drainFunction =
		RequireToken(source, "inline bool PhysicSystem::DrainSimulation(", fetchFunction);
	const std::size_t downloadFunction =
		RequireToken(source, "inline void PhysicSystem::PhysicsDownload()", drainFunction);

	const std::string beginBody = source.substr(
		beginFunction,
		fetchFunction - beginFunction
	);
	const std::string fetchBody = source.substr(
		fetchFunction,
		drainFunction - fetchFunction
	);
	const std::string drainBody = source.substr(
		drainFunction,
		downloadFunction - drainFunction
	);

	assert(header.find("std::mutex m_simulationMutex;") != std::string::npos);
	const std::size_t beginStateLock = RequireToken(
		beginBody,
		"std::scoped_lock simulationLock(m_simulationMutex);"
	);
	const std::size_t beginLock =
		RequireToken(beginBody, "g_pScene->lockWrite();", beginStateLock);
	const std::size_t simulateCall =
		RequireToken(beginBody, "g_pScene->simulate(fixedDeltaTime)", beginLock);
	const std::size_t beginUnlock =
		RequireToken(beginBody, "g_pScene->unlockWrite();", simulateCall);
	assert(beginStateLock < beginLock);
	assert(beginLock < simulateCall);
	assert(simulateCall < beginUnlock);
	assert(beginBody.find("lockRead") == std::string::npos);
	assert(beginBody.find("fetchResults") == std::string::npos);
	assert(beginBody.find("if(!submitted)") != std::string::npos);

	const std::size_t fetchStateLock = RequireToken(
		fetchBody,
		"std::scoped_lock simulationLock(m_simulationMutex);"
	);
	const std::size_t errorState =
		RequireToken(fetchBody, "physx::PxU32 errorState = 0;", fetchStateLock);
	const std::size_t fetchLock =
		RequireToken(fetchBody, "g_pScene->lockWrite();", errorState);
	const std::size_t fetchCall = RequireToken(
		fetchBody,
		"g_pScene->fetchResults(true, &errorState)",
		fetchLock
	);
	const std::size_t fetchUnlock =
		RequireToken(fetchBody, "g_pScene->unlockWrite();", fetchCall);
	const std::size_t clearInFlight = RequireToken(
		fetchBody,
		"m_simulationInFlight.store(false, std::memory_order_release)",
		fetchUnlock
	);
	const std::size_t fetchedGuard =
		RequireToken(fetchBody, "if(!fetched)", clearInFlight);
	assert(fetchStateLock < fetchLock);
	assert(fetchLock < fetchCall);
	assert(fetchCall < fetchUnlock);
	assert(fetchUnlock < clearInFlight);
	assert(clearInFlight < fetchedGuard);
	assert(fetchBody.find("lockRead") == std::string::npos);

	const std::size_t drainStateLock = RequireToken(
		drainBody,
		"std::scoped_lock simulationLock(m_simulationMutex);"
	);
	const std::size_t drainFetch = RequireToken(
		drainBody,
		"g_pScene->fetchResults(true, &errorState)",
		drainStateLock
	);
	const std::size_t drainClear = RequireToken(
		drainBody,
		"m_simulationInFlight.store(false, std::memory_order_release)",
		drainFetch
	);
	assert(drainStateLock < drainFetch);
	assert(drainFetch < drainClear);
	assert(drainBody.find("lockRead") == std::string::npos);
}

void ValidateShutdownLifecycle(){
	const std::string source = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Physic/physicSystem.cpp"
	);
	const std::size_t finalizeFunction =
		RequireToken(source, "void PhysicSystem::Finalize()");
	const std::size_t attachFunction =
		RequireToken(source, "ActorEntityInfo* PhysicSystem::AttachActorEntityInfo(", finalizeFunction);
	const std::string finalizeBody = source.substr(
		finalizeFunction,
		attachFunction - finalizeFunction
	);

	const std::size_t disableCallback =
		RequireToken(finalizeBody, "m_simCallback->m_active = false");
	const std::size_t drain =
		RequireToken(finalizeBody, "DrainSimulation(\"Finalize\")", disableCallback);
	const std::size_t releaseScene =
		RequireToken(finalizeBody, "g_pScene->release();", drain);
	const std::size_t getTransport =
		RequireToken(finalizeBody, "g_pPvd->getTransport()", releaseScene);
	const std::size_t releasePvd =
		RequireToken(finalizeBody, "g_pPvd->release();", getTransport);
	const std::size_t releaseTransport =
		RequireToken(finalizeBody, "transport->release();", releasePvd);
	assert(disableCallback < drain);
	assert(drain < releaseScene);
	assert(getTransport < releasePvd);
	assert(releasePvd < releaseTransport);
	assert(finalizeBody.find("disconnect()") == std::string::npos);

	const std::size_t stopFunction =
		RequireToken(source, "void PhysicSystem::Stop()");
	const std::size_t encodeFunction =
		RequireToken(source, "YAML::Node PhysicSystem::encode()", stopFunction);
	const std::string stopBody = source.substr(
		stopFunction,
		encodeFunction - stopFunction
	);
	const std::size_t stopDrain =
		RequireToken(stopBody, "DrainSimulation(\"Stop\")");
	const std::size_t releaseCollider =
		RequireToken(stopBody, "ReleaseColliderRuntime(collider)", stopDrain);
	assert(stopDrain < releaseCollider);
	assert(stopBody.find("simulate(") == std::string::npos);
	assert(stopBody.find("fetchResults(") == std::string::npos);
	assert(stopBody.find("lockRead") == std::string::npos);

	const std::size_t fixedUpdateFunction =
		RequireToken(source, "void PhysicSystem::FixedUpdate(float deltaTime)");
	const std::size_t drawLayerFunction =
		RequireToken(source, "void PhysicSystem::DrawLayerEditor()", fixedUpdateFunction);
	const std::string fixedUpdateBody = source.substr(
		fixedUpdateFunction,
		drawLayerFunction - fixedUpdateFunction
	);
	const std::size_t upload = RequireToken(fixedUpdateBody, "PhysicsUpload();");
	const std::size_t begin = RequireToken(fixedUpdateBody, "PhysicsBegin(deltaTime);", upload);
	const std::size_t fetch = RequireToken(fixedUpdateBody, "PhysicsFetch();", begin);
	const std::size_t download = RequireToken(fixedUpdateBody, "PhysicsDownload();", fetch);
	const std::size_t dispatch = RequireToken(fixedUpdateBody, "CollisionEventDispatch();", download);
	assert(upload < begin);
	assert(begin < fetch);
	assert(fetch < download);
	assert(download < dispatch);
	assert(fixedUpdateBody.find("g_pScene->simulate") == std::string::npos);
	assert(fixedUpdateBody.find("g_pScene->fetchResults") == std::string::npos);
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
	ValidateShutdownLifecycle();
	ValidateAnalysisNamesMatchTasks();
	return 0;
}
