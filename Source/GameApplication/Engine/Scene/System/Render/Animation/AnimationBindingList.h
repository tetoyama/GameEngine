#pragma once

#include <string>
#include <string_view>

// ModelRendererComponent定義後にincludeする軽量Helper。
namespace AnimationBindingList {

inline bool Contains(
	const ModelRendererComponent& component,
	std::string_view assetPath
) noexcept {
	for(const auto& binding : component.animations){
		if(binding.second == assetPath) return true;
	}
	return false;
}

inline bool AddUnique(
	ModelRendererComponent& component,
	std::string_view alias,
	std::string_view assetPath
){
	if(alias.empty() || assetPath.empty() || Contains(component, assetPath)){
		return false;
	}
	component.animations.emplace_back(
		std::string(alias),
		std::string(assetPath)
	);
	return true;
}

} // namespace AnimationBindingList
