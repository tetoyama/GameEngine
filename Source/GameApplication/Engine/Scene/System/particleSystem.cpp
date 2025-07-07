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
		TransformComponent* transform = m_context->component->GetComponent<TransformComponent>(entity);
		if(transform){
			
			for(int i = 0; i < MAXPARTICLE; i++){
				if(particle->Particle[i].LifeTime <= 0.0f){
					particle->Particle[i].LifeTime = particle->particleLifeTime;
					particle->Particle[i].Position = transform->position;
					break;
				} else{

				}
			}
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
		TransformComponent* transform = m_context->component->GetComponent<TransformComponent>(entity);
		if(transform){

			for(int i = 0; i < MAXPARTICLE; i++){
				if(particle->Particle[i].LifeTime <= 0.0f){
					particle->Particle[i].LifeTime = particle->particleLifeTime;
					particle->Particle[i].Position = transform->position;
					break;
				} else{
					particle->Particle[i].LifeTime -= deltaTime;
					particle->Particle[i].Position += Vector3(0, 1 * deltaTime, 0);
				}
			}
		}
	}
}

void ParticleSystem::Draw(){

}
