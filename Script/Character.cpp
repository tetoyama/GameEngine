#include "Character.h"
#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "DebugTools/DebugSystem.h"
#include "DebugTools/DebugSystem.cpp"

#include <string>

void Character::OnStart(){
	context->manager->debug->LOG_INFO(("Character Script Started on Entity ID: " + std::to_string(entity)).c_str());
}


void Character::OnUpdate(float dt){
	context->manager->debug->LOG_INFO(("Character Script OnUpdate on Entity ID: " + std::to_string(entity)).c_str());
}


REGISTER_SCRIPT("Character", Character)
