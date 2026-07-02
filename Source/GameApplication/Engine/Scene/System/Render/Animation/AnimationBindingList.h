#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace AnimationBindingList {

using BindingList = std::vector<std::pair<std::string, std::string>>;

inline bool Contains(
	const BindingList& bindings,
	std::string_view assetPath
) noexcept {
	for(const auto& binding : bindings){
		if(binding.second == assetPath) return true;
	}
	return false;
}

inline bool AddUnique(
	BindingList& bindings,
	std::string_view alias,
	std::string_view assetPath
){
	if(alias.empty() || assetPath.empty() || Contains(bindings, assetPath)){
		return false;
	}
	bindings.emplace_back(
		std::string(alias),
		std::string(assetPath)
	);
	return true;
}

} // namespace AnimationBindingList
