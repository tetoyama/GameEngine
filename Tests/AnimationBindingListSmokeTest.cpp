#include <cassert>

#include "Engine/Scene/System/Render/Animation/AnimationBindingList.h"

int main(){
	ModelRendererComponent component;
	assert(!AnimationBindingList::Contains(component, "Asset/Run.fbx"));
	assert(!AnimationBindingList::AddUnique(component, "", "Asset/Run.fbx"));
	assert(!AnimationBindingList::AddUnique(component, "Run", ""));
	assert(AnimationBindingList::AddUnique(
		component,
		"Run",
		"Asset/Run.fbx"
	));
	assert(AnimationBindingList::Contains(component, "Asset/Run.fbx"));
	assert(component.animations.size() == 1);
	assert(!AnimationBindingList::AddUnique(
		component,
		"RunAlias",
		"Asset/Run.fbx"
	));
	assert(component.animations.size() == 1);
	assert(AnimationBindingList::AddUnique(
		component,
		"Walk",
		"Asset/Walk.fbx"
	));
	assert(component.animations.size() == 2);
	return 0;
}
