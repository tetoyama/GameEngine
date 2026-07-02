#include <cassert>

#include "Engine/Scene/System/Render/RenderSystem/RenderPass/PlayerView/PlayerViewRefreshPolicy.h"

int main(){
	using namespace PlayerViewRefreshPolicy;

	assert(ShouldRender(false, 0.0f, false));
	assert(ShouldRender(false, 0.0f, true));
	assert(ShouldRender(true, 0.0f, false));
	assert(!ShouldRender(true, 0.0f, true));
	assert(!ShouldRender(true, IdleRefreshIntervalSeconds - 0.01f, true));
	assert(ShouldRender(true, IdleRefreshIntervalSeconds, true));
	return 0;
}
