#include <cassert>

#include "Engine/Scene/System/Render/Animation/AnimationBindingList.h"

int main(){
	AnimationBindingList::BindingList bindings;
	assert(!AnimationBindingList::Contains(bindings, "Asset/Run.fbx"));
	assert(!AnimationBindingList::AddUnique(bindings, "", "Asset/Run.fbx"));
	assert(!AnimationBindingList::AddUnique(bindings, "Run", ""));
	assert(AnimationBindingList::AddUnique(
		bindings,
		"Run",
		"Asset/Run.fbx"
	));
	assert(AnimationBindingList::Contains(bindings, "Asset/Run.fbx"));
	assert(bindings.size() == 1);
	assert(!AnimationBindingList::AddUnique(
		bindings,
		"RunAlias",
		"Asset/Run.fbx"
	));
	assert(bindings.size() == 1);
	assert(AnimationBindingList::AddUnique(
		bindings,
		"Walk",
		"Asset/Walk.fbx"
	));
	assert(bindings.size() == 2);
	return 0;
}
