#include "HelloWorld.h"
#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "DebugTools/DebugSystem.h"

void HelloWorld::OnStart() {
	context->manager->debug->LOG_INFO((text + std::string("")).c_str());
}

REGISTER_SCRIPT("HelloWorld", HelloWorld)

