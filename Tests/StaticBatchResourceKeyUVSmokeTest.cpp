#include <cassert>

#include "Engine/Scene/System/Render/RenderSystem/RenderPacket/StaticBatchResourceKey.h"

int main(){
	const UVMatrixBuffer base =
		StaticBatchResourceKey::ResolveUVState(1.0f, 1.0f, 0);
	assert(base.UVStart.x == 0.0f);
	assert(base.UVStart.y == 0.0f);
	assert(base.UVEnd.x == 1.0f);
	assert(base.UVEnd.y == 1.0f);

	// 4 x 2 slices. Frame 3 is the last cell in the first row.
	const UVMatrixBuffer sliced =
		StaticBatchResourceKey::ResolveUVState(4.0f, 2.0f, 3);
	assert(sliced.UVStart.x == 0.75f);
	assert(sliced.UVStart.y == 0.0f);
	assert(sliced.UVEnd.x == 1.0f);
	assert(sliced.UVEnd.y == 0.5f);

	// Frame 4 starts the second row.
	const UVMatrixBuffer secondRow =
		StaticBatchResourceKey::ResolveUVState(4.0f, 2.0f, 4);
	assert(secondRow.UVStart.x == 0.0f);
	assert(secondRow.UVStart.y == 0.5f);
	assert(secondRow.UVEnd.x == 0.25f);
	assert(secondRow.UVEnd.y == 1.0f);

	// Values below one are repeat divisors, not animation slices.
	const UVMatrixBuffer repeated =
		StaticBatchResourceKey::ResolveUVState(0.25f, 0.5f, 99);
	assert(repeated.UVStart.x == 0.0f);
	assert(repeated.UVStart.y == 0.0f);
	assert(repeated.UVEnd.x == 4.0f);
	assert(repeated.UVEnd.y == 2.0f);

	// Animation frames are clamped to the last valid slice.
	const UVMatrixBuffer clamped =
		StaticBatchResourceKey::ResolveUVState(4.0f, 2.0f, 99);
	assert(clamped.UVStart.x == 0.75f);
	assert(clamped.UVStart.y == 0.5f);
	assert(clamped.UVEnd.x == 1.0f);
	assert(clamped.UVEnd.y == 1.0f);

	const UVMatrixBuffer invalid =
		StaticBatchResourceKey::ResolveUVState(0.0f, 2.0f, 1);
	assert(invalid.UVStart.x == 0.0f);
	assert(invalid.UVStart.y == 0.0f);
	assert(invalid.UVEnd.x == 1.0f);
	assert(invalid.UVEnd.y == 1.0f);

	const UVMatrixBuffer nullTexture =
		StaticBatchResourceKey::ResolveUVState(
			static_cast<const TextureComponent*>(nullptr)
		);
	assert(
		StaticBatchResourceKey::MakeUVStateKey(nullTexture) ==
		StaticBatchResourceKey::MakeUVStateKey(base)
	);
	assert(
		StaticBatchResourceKey::MakeUVStateKey(sliced) !=
		StaticBatchResourceKey::MakeUVStateKey(secondRow)
	);
	assert(
		StaticBatchResourceKey::MakeUVStateKey(base) !=
		StaticBatchResourceKey::MakeUVStateKey(repeated)
	);
	return 0;
}
