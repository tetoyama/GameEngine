#include <cassert>

#include "Engine/Scene/System/Render/Culling/CullingMath.h"

int main(){
	const EntityAABB viewBounds{
		Vector3(-10.0f, -10.0f, -10.0f),
		Vector3(10.0f, 10.0f, 10.0f)
	};
	const CullingFrustum frustum =
		CullingMath::MakeAxisAlignedFrustum(viewBounds);

	const EntityAABB centered{
		Vector3(-1.0f, -1.0f, -1.0f),
		Vector3(1.0f, 1.0f, 1.0f)
	};
	const EntityAABB crossingEdge{
		Vector3(9.0f, -1.0f, -1.0f),
		Vector3(12.0f, 1.0f, 1.0f)
	};
	const EntityAABB separated{
		Vector3(11.0f, -1.0f, -1.0f),
		Vector3(13.0f, 1.0f, 1.0f)
	};
	const EntityAABB unavailable{
		Vector3(1.0f, 1.0f, 1.0f),
		Vector3(-1.0f, -1.0f, -1.0f)
	};

	assert(CullingMath::IntersectsFrustum(centered, frustum));
	assert(CullingMath::IntersectsFrustum(crossingEdge, frustum));
	assert(!CullingMath::IntersectsFrustum(separated, frustum));
	assert(CullingMath::IntersectsFrustum(unavailable, frustum));
	return 0;
}
