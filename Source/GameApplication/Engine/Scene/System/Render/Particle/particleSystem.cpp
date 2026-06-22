// =======================================================================
// 
// particleSystem.cpp
// 
// =======================================================================
#include "particleSystem.h"
#include "Scene.h"
#include "sceneManager.h"

#include "Backends/ImGui/ImGui.h"

#include "Entity/Entity.h"
#include "Registry/componentRegistry.h"

#include "Component/ParticleComponent.h"

#include "DebugTools/debugSystem.h"
#include "Graphics/graphicsContext.h"
#include "Graphics/mainRenderer.h"
#include <Component/transformComponent.h>

float RandomParticle() {
	return (float)(rand() % 200 - 100) / 100.0f;
}

void ParticleSystem::Initialize() {}

void ParticleSystem::Finalize() {}

void ParticleSystem::RegisterTasks(SystemScheduleBuilder& builder){
	using ParticleUpdateQuery = ECSQuery::ComponentQueryView<
		ECSQuery::Read<TransformComponent>,
		ECSQuery::Write<ParticleComponent>
	>;

	builder.AddQueryTask<ParticleUpdateQuery>(
		"ParticleSystem.Update",
		SystemTaskDomain::Frame,
		SystemPhase::Late,
		0,
		StructuralAccess::None,
		ThreadAffinity::AnyWorker,
		[this](const SystemTaskContext& context){
			Update(context.deltaTime);
		}
	);
}

void ParticleSystem::Start() {
	for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
		auto context = scene->GetSceneContext();
		const auto& Entities = context->component->FindEntitiesWithComponent<ParticleComponent>();
		if (Entities.empty()) {
			continue;
		}
		for (Entity entity : Entities) {
			ParticleComponent* particle = context->component->GetComponent<ParticleComponent>(entity);
			if (!particle) {
				continue;
			}
			particle->SpawnTimer = 0.0f;

			for (int i = 0; i < MAXPARTICLE; i++) {
				if (particle->isLoop) {
					particle->Particle[i].LifeTime = 0.0f;
				} else {
					if (particle->SpawnCount > i) {
						particle->Particle[i].LifeTime = particle->particleLifeTime;
						particle->Particle[i].Position = particle->SpawnPosition + Vector3(particle->SpawnPositionRandom.x * RandomParticle(), particle->SpawnPositionRandom.y * RandomParticle(), particle->SpawnPositionRandom.z * RandomParticle());
						particle->Particle[i].Speed = particle->StartSpeed + Vector3(particle->StartSpeedRandom.x * RandomParticle(), particle->StartSpeedRandom.y * RandomParticle(), particle->StartSpeedRandom.z * RandomParticle());
					}
				}
			}
		}
	}
}

void ParticleSystem::Update(float deltaTime){
	for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
		auto context = scene->GetSceneContext();
		const auto& Entities = context->component->FindEntitiesWithComponent<ParticleComponent>();
		if (Entities.empty()) {
			continue;
		}
		for (Entity entity : Entities) {
			ParticleComponent* particle = context->component->GetComponent<ParticleComponent>(entity);
			if (!particle) {
				continue;
			}
			particle->SpawnTimer += deltaTime;

			TransformComponent* transform = context->component->GetComponent<TransformComponent>(entity);
			if (transform) {

				for (int i = 0; i < MAXPARTICLE; i++) {
					if (particle->Particle[i].LifeTime > 0.0f) {
						particle->Particle[i].LifeTime -= deltaTime;
						particle->Particle[i].Position += particle->Particle[i].Speed * deltaTime;
						particle->Particle[i].Speed += particle->AddSpeed * deltaTime;
						particle->Particle[i].Speed *= Vector3(powf(particle->MulSpeed.x, deltaTime), powf(particle->MulSpeed.y, deltaTime), powf(particle->MulSpeed.z, deltaTime));
					}
				}

				if (particle->SpawnInterval == 0.0f) { continue; }
				while (particle->SpawnTimer > particle->SpawnInterval) {
					particle->SpawnTimer -= particle->SpawnInterval;
					for (int i = 0; i < MAXPARTICLE; i++) {
						if (particle->isLoop && particle->Particle[i].LifeTime <= 0.0f) {
							particle->Particle[i].LifeTime = particle->particleLifeTime;
							particle->Particle[i].Position = particle->SpawnPosition + Vector3(particle->SpawnPositionRandom.x * RandomParticle(), particle->SpawnPositionRandom.y * RandomParticle(), particle->SpawnPositionRandom.z * RandomParticle());
							particle->Particle[i].Speed = particle->StartSpeed + Vector3(particle->StartSpeedRandom.x * RandomParticle(), particle->StartSpeedRandom.y * RandomParticle(), particle->StartSpeedRandom.z * RandomParticle());
							break;
						}
					}
				}
			}
		}

	}
}
