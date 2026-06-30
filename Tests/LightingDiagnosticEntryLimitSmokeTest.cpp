#include <cassert>

#include "Engine/Scene/System/Render/RenderSystem/RenderPass/LightingPass/LightingDiagnosticEntryLimit.h"

int main(){
	using LightingDiagnosticEntryLimit::ResolveExpandedCount;

	assert(ResolveExpandedCount(4.0f, 0, 1) == 1);
	assert(ResolveExpandedCount(4.0f, 0, 2) == 2);
	assert(ResolveExpandedCount(4.0f, 0, 4) == 4);
	assert(ResolveExpandedCount(6.0f, 2, 4) == 2);
	assert(ResolveExpandedCount(1.0f, 3, 4) == 1);
	assert(ResolveExpandedCount(0.0f, 0, 4) == 1);
	assert(ResolveExpandedCount(4.0f, 4, 4) == 0);
	assert(ResolveExpandedCount(4.0f, 5, 4) == 0);
	return 0;
}
