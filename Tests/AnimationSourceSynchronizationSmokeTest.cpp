#include <cassert>
#include <memory>

#include "Engine/Scene/System/Render/Animation/AnimationSourceSynchronization.h"

int main(){
	ModelRendererComponent component;
	assert(!AnimationSourceSynchronization::Synchronize(component));

	component.model = std::make_shared<ModelData>();
	component.animations.emplace_back("", "");
	component.animations.emplace_back("MissingAlias", "");
	component.animations.emplace_back("", "Asset/Animation/Missing.fbx");
	assert(!AnimationSourceSynchronization::Synchronize(component));

	AnimationData imported;
	imported.isImported = true;
	imported.FilePath = "Asset/Animation/Run.fbx";
	component.model->m_Animation.emplace("Run", imported);
	component.animations.emplace_back("Run", imported.FilePath);
	assert(component.model->HasImportedAnimationSource(imported.FilePath));
	assert(!AnimationSourceSynchronization::Synchronize(component));
	assert(component.model->m_Animation.size() == 1);
	return 0;
}
