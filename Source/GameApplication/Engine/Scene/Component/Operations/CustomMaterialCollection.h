#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Resources/Data/modelMaterialTypes.h"

namespace CustomMaterialCollection {

inline bool Contains(
	std::span<const CustomMaterialEntry> materials,
	CustomMaterialID id
) noexcept {
	if(id == InvalidCustomMaterialID) return false;
	return std::any_of(
		materials.begin(),
		materials.end(),
		[id](const CustomMaterialEntry& material){
			return material.id == id;
		}
	);
}

inline CustomMaterialID AllocateID(
	std::span<const CustomMaterialEntry> materials
){
	CustomMaterialID maximum = InvalidCustomMaterialID;
	for(const CustomMaterialEntry& material : materials){
		maximum = (std::max)(maximum, material.id);
	}
	if(maximum != (std::numeric_limits<CustomMaterialID>::max)()){
		return maximum + 1u;
	}

	std::unordered_set<CustomMaterialID> used;
	used.reserve(materials.size());
	for(const CustomMaterialEntry& material : materials){
		if(material.id != InvalidCustomMaterialID){
			used.insert(material.id);
		}
	}
	for(CustomMaterialID candidate = 1u;
		candidate != InvalidCustomMaterialID;
		++candidate){
		if(!used.contains(candidate)) return candidate;
	}
	return InvalidCustomMaterialID;
}

inline CustomMaterialEntry* Add(
	std::vector<CustomMaterialEntry>& materials,
	std::string name = {},
	MaterialDescriptor descriptor = {}
){
	const CustomMaterialID id = AllocateID(materials);
	if(id == InvalidCustomMaterialID) return nullptr;
	if(name.empty()){
		name = "Material " + std::to_string(id);
	}
	materials.push_back({id, std::move(name), std::move(descriptor)});
	return &materials.back();
}

inline bool Remove(
	std::vector<CustomMaterialEntry>& materials,
	CustomMaterialID id
) noexcept {
	const auto found = std::find_if(
		materials.begin(),
		materials.end(),
		[id](const CustomMaterialEntry& material){
			return material.id == id;
		}
	);
	if(found == materials.end()) return false;
	materials.erase(found);
	return true;
}

inline void Sanitize(std::vector<CustomMaterialEntry>& materials){
	std::unordered_set<CustomMaterialID> ids;
	ids.reserve(materials.size());
	std::vector<CustomMaterialEntry> sanitized;
	sanitized.reserve(materials.size());
	for(CustomMaterialEntry material : materials){
		if(material.id == InvalidCustomMaterialID ||
			!ids.insert(material.id).second){
			continue;
		}
		if(material.name.empty()){
			material.name = "Material " + std::to_string(material.id);
		}
		sanitized.push_back(std::move(material));
	}
	materials = std::move(sanitized);
}

} // namespace CustomMaterialCollection
