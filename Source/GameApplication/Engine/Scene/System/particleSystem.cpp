#include "particleSystem.h"
#include "Scene.h"
#include "sceneManager.h"

#include "Backends/ImGui/ImGui.h"

#include "Entity/Entity.h"
#include "Registry/componentRegistry.h"

#include "Component/ParticleComponent.h"

#include "Engine/DebugTools/debugSystem.h"
#include "Engine/Graphics/graphicsContext.h"
#include "Engine/Graphics/mainRenderer.h"
#include <Component/transformComponent.h>

float RandomParticle() {
	return (float)(rand() % 200 - 100) / 100.0f;
}

void ParticleSystem::Initialize() {}

void ParticleSystem::Finalize() {}

void ParticleSystem::Start() {
	// ライトコンポーネントを持つエンティティの検索
	const auto& Entities = m_context->component->FindEntitiesWithComponent<ParticleComponent>();
	if(Entities.empty()){
		return;
	}
	for(Entity entity : Entities){
		ParticleComponent* particle = m_context->component->GetComponent<ParticleComponent>(entity);
		if(!particle){
			continue;
		}
		particle->SpawnTimer = 0.0f;

		for (int i = 0; i < MAXPARTICLE; i++) {
			particle->Particle[i].LifeTime = 0.0f;
		}
	}
}

void ParticleSystem::Update(float deltaTime){
	// ライトコンポーネントを持つエンティティの検索
	const auto& Entities = m_context->component->FindEntitiesWithComponent<ParticleComponent>();
	if(Entities.empty()){
		return;
	}
	for(Entity entity : Entities){
		ParticleComponent* particle = m_context->component->GetComponent<ParticleComponent>(entity);
		if(!particle){
			continue;
		}
		particle->SpawnTimer += deltaTime;

		TransformComponent* transform = m_context->component->GetComponent<TransformComponent>(entity);
		if(transform){

			if (particle->SpawnRate == 0.0f) {
				particle->SpawnRate = 0.00001f;
			}
			while (particle->SpawnTimer > particle->SpawnRate) {
				particle->SpawnTimer -= particle->SpawnRate;
				for (int i = 0; i < MAXPARTICLE; i++) {
					if (particle->Particle[i].LifeTime <= 0.0f) {
						particle->Particle[i].LifeTime = particle->particleLifeTime;
						particle->Particle[i].Position = Vector3(0,0,0);
						particle->Particle[i].Speed = particle->StartSpeed + Vector3(particle->StartRandom.x * RandomParticle(), particle->StartRandom.y * RandomParticle(), particle->StartRandom.z * RandomParticle());
						break;
					}
				}
			}


			for (int i = 0; i < MAXPARTICLE; i++) {
				if (particle->Particle[i].LifeTime > 0.0f) {

					particle->Particle[i].LifeTime -= deltaTime;
					particle->Particle[i].Position += particle->Particle[i].Speed * deltaTime;
					particle->Particle[i].Speed += particle->AddSpeed * deltaTime;
				}
			}
		}
	}
}

void ParticleSystem::Draw(){

}
