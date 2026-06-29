#pragma once

#include <string>
#include <utility>

#include "System/Render/Animation/AnimationBindingList.h"

namespace AnimationSourceSynchronization {

template<typename IsImported, typename ImportSource>
bool Synchronize(
	const AnimationBindingList::BindingList& bindings,
	IsImported&& isImported,
	ImportSource&& importSource
){
	bool imported = false;
	for(const auto& [alias, assetPath] : bindings){
		if(alias.empty() || assetPath.empty() || isImported(assetPath)){
			continue;
		}

		importSource(alias, assetPath);
		imported = imported || isImported(assetPath);
	}
	return imported;
}

template<typename Component>
bool Synchronize(Component& component){
	if(!component.model) return false;
	return Synchronize(
		component.animations,
		[&component](const std::string& assetPath){
			return component.model->HasImportedAnimationSource(assetPath);
		},
		[&component](
			const std::string& alias,
			const std::string& assetPath
		){
			component.model->LoadAnimationSource(
				assetPath.c_str(),
				alias.c_str()
			);
		}
	);
}

} // namespace AnimationSourceSynchronization
