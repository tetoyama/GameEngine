#include "Character.h"
#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "DebugTools/DebugSystem.h"

#include <string>

void Character::OnStart(){
	ref.GetScene()->manager->debug->LOG_INFO(("Character Script Started on Entity ID: " + std::to_string(ref.GetEntityID())).c_str());
}

void Character::OnUpdate(float dt){
	ref.GetScene()->manager->debug->LOG_INFO(("Character Script OnUpdate on Entity ID: " + std::to_string(ref.GetEntityID())).c_str());
	ref.GetScene()->manager->debug->LOG_INFO(("Character Speed: " + std::to_string(speed)).c_str());
}

