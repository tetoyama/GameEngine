#include <cassert>
#include <DirectXMath.h>
#include "Engine/Scene/System/Render/Culling/CullingFrustumRuntime.h"

int main(){
	const auto projection = DirectX::XMMatrixOrthographicLH(20.0f, 20.0f, 0.0f, 20.0f);
	const auto frustum = CullingFrustumRuntime::FromViewProjection(projection);
	const EntityAABB inside{Vector3(-1.0f, -1.0f, 4.0f), Vector3(1.0f, 1.0f, 6.0f)};
	assert(CullingMath::IntersectsFrustum(inside, frustum));
	return 0;
}
