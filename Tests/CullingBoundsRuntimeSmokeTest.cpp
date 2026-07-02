#include <cassert>
#include <cmath>
#include <DirectXMath.h>
#include "Engine/Scene/System/Render/Culling/CullingBoundsRuntime.h"

static bool Near(float a, float b){
	return std::fabs(a - b) < 0.0001f;
}

int main(){
	CullingComponent culling;
	culling.localBounds = {
		Vector3(-1.0f, -1.0f, -1.0f),
		Vector3(1.0f, 1.0f, 1.0f)
	};

	const auto firstMatrix =
		DirectX::XMMatrixScaling(2.0f, 3.0f, 4.0f) *
		DirectX::XMMatrixTranslation(10.0f, 20.0f, 30.0f);
	assert(CullingBoundsRuntime::UpdateWorldBounds(culling, firstMatrix, 5));
	assert(culling.boundsValid);
	assert(Near(culling.worldBounds.min.x, 8.0f));
	assert(Near(culling.worldBounds.max.z, 34.0f));

	const auto firstRevision = culling.transformRevision;
	assert(!CullingBoundsRuntime::UpdateWorldBounds(culling, firstMatrix, 5));
	assert(culling.transformRevision == firstRevision);

	const auto movedMatrix = DirectX::XMMatrixTranslation(-4.0f, 2.0f, 8.0f);
	assert(CullingBoundsRuntime::UpdateWorldBounds(culling, movedMatrix, 5));
	assert(culling.transformRevision != firstRevision);
	assert(Near(culling.worldBounds.min.x, -5.0f));
	assert(Near(culling.worldBounds.max.x, -3.0f));

	assert(CullingBoundsRuntime::UpdateWorldBounds(culling, movedMatrix, 6));
	assert(culling.sourceRevision == 6);
	return 0;
}
