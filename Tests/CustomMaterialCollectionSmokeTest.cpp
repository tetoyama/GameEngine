#include <cassert>
#include <limits>
#include <string>
#include <vector>

#include "Engine/Scene/Component/Operations/CustomMaterialCollection.h"

int main(){
	std::vector<CustomMaterialEntry> materials;
	assert(CustomMaterialCollection::AllocateID(materials) == 1);
	assert(!CustomMaterialCollection::Contains(materials, 1));

	CustomMaterialEntry* first = CustomMaterialCollection::Add(materials);
	assert(first);
	assert(first->id == 1);
	assert(first->name == "Material 1");
	assert(CustomMaterialCollection::Contains(materials, 1));

	MaterialDescriptor customDescriptor;
	customDescriptor.shaderID = 7;
	CustomMaterialEntry* second = CustomMaterialCollection::Add(
		materials,
		"Glass",
		customDescriptor
	);
	assert(second);
	assert(second->id == 2);
	assert(second->name == "Glass");
	assert(second->inlineMaterial.shaderID == 7);

	assert(CustomMaterialCollection::Remove(materials, 1));
	assert(!CustomMaterialCollection::Contains(materials, 1));
	assert(!CustomMaterialCollection::Remove(materials, 1));
	assert(CustomMaterialCollection::AllocateID(materials) == 3);

	materials.push_back({InvalidCustomMaterialID, "Invalid", {}});
	materials.push_back({2, "Duplicate", {}});
	materials.push_back({5, "", {}});
	CustomMaterialCollection::Sanitize(materials);
	assert(materials.size() == 2);
	assert(materials[0].id == 2);
	assert(materials[0].name == "Glass");
	assert(materials[1].id == 5);
	assert(materials[1].name == "Material 5");

	materials.clear();
	materials.push_back({
		(std::numeric_limits<CustomMaterialID>::max)(),
		"Maximum",
		{}
	});
	materials.push_back({1, "One", {}});
	materials.push_back({3, "Three", {}});
	assert(CustomMaterialCollection::AllocateID(materials) == 2);
	CustomMaterialEntry* wrapped = CustomMaterialCollection::Add(materials);
	assert(wrapped);
	assert(wrapped->id == 2);
	return 0;
}
