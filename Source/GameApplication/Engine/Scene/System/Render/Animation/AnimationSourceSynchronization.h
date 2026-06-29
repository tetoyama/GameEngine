#pragma once

#include "Scene/Component/modelRendererComponent.h"

namespace AnimationSourceSynchronization {

inline bool Synchronize(ModelRendererComponent& component){
	if(!component.model) return false;

	bool imported = false;
	for(const auto& [alias, sourcePath] : component.animations){
		if(alias.empty() || sourcePath.empty() ||
			component.model->HasImportedAnimationSource(sourcePath)){
			continue;
		}

		component.model->LoadAnimationSource(
			sourcePath.c_str(),
			alias.c_str()
		);
		imported = imported ||
			component.model->HasImportedAnimationSource(sourcePath);
	}
	return imported;
}

} // namespace AnimationSourceSynchronization
