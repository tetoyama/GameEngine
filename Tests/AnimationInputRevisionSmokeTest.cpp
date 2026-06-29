#include <array>
#include <cassert>

#include "Engine/Scene/System/Render/Animation/AnimationInputRevision.h"

int main(){
	std::array<AnimationBlend, 2> animations{};
	animations[0].name = "Idle";
	animations[0].weight = 0.75f;
	animations[0].animationStartTime = 1.0f;
	animations[0].isLoop = true;
	animations[1].name = "Run";
	animations[1].weight = 0.25f;
	animations[1].animationStartTime = 3.0f;
	animations[1].isLoop = false;

	const std::uint64_t original =
		AnimationInputRevision::Compute(animations, 10.0f);
	assert(original != 0);
	assert(original == AnimationInputRevision::Compute(animations, 10.0f));

	auto changed = animations;
	changed[0].name = "Walk";
	assert(original != AnimationInputRevision::Compute(changed, 10.0f));

	changed = animations;
	changed[0].weight = 0.5f;
	assert(original != AnimationInputRevision::Compute(changed, 10.0f));

	changed = animations;
	changed[0].animationStartTime = 2.0f;
	assert(original != AnimationInputRevision::Compute(changed, 10.0f));

	changed = animations;
	changed[0].isLoop = false;
	assert(original != AnimationInputRevision::Compute(changed, 10.0f));

	assert(original != AnimationInputRevision::Compute(animations, 11.0f));
	assert(
		AnimationInputRevision::Compute(
			std::span<const AnimationBlend>{},
			0.0f
		) != 0
	);
	return 0;
}
