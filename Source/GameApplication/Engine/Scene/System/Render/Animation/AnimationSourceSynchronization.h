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

} // namespace AnimationSourceSynchronization
