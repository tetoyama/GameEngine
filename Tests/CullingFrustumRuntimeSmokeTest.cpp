#include <cassert>
#include <DirectXMath.h>
#include "Engine/Scene/System/Render/Culling/CullingFrustumRuntime.h"

int main(){
	const auto projection = DirectX::XMMatrixOrthographicLH(
		20.0f,
		20.0f,
		0.0f,
		20.0f
	);
	const auto frustum = CullingFrustumRuntime::FromViewProjection(projection);

	const EntityAABB inside{
		Vector3(-1.0f, -1.0f, 4.0f),
		Vector3(1.0f, 1.0f, 6.0f)
	};
	const EntityAABB outsideSide{
		Vector3(11.0f, -1.0f, 4.0f),
		Vector3(12.0f, 1.0f, 6.0f)
	};
	const EntityAABB outsideNear{
		Vector3(-1.0f, -1.0f, -2.0f),
		Vector3(1.0f, 1.0f, -1.0f)
	};
	const EntityAABB outsideFar{
		Vector3(-1.0f, -1.0f, 21.0f),
		Vector3(1.0f, 1.0f, 22.0f)
	};

	assert(CullingMath::IntersectsFrustum(inside, frustum));
	assert(!CullingMath::IntersectsFrustum(outsideSide, frustum));
	assert(!CullingMath::IntersectsFrustum(outsideNear, frustum));
	assert(!CullingMath::IntersectsFrustum(outsideFar, frustum));
	return 0;
}
