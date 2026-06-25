#include <cassert>

#include "Engine/Scene/System/Render/Culling/CullingVisibilitySet.h"

int main(){
	CullingVisibilitySet visibility;
	const CullingViewKey editorView{1, Entity{10, 2}};
	const CullingViewKey playerView{1, Entity{11, 3}};
	const Entity model{20, 4};
	const Entity terrain{21, 1};

	visibility.BeginFrame(42);
	assert(visibility.FrameSerial() == 42);
	assert(visibility.ViewCount() == 0);

	visibility.BeginView(editorView);
	visibility.ReserveView(editorView, 16);
	visibility.SetVisible(editorView, model);
	visibility.SetVisible(editorView, terrain);

	visibility.BeginView(playerView);
	visibility.SetVisible(playerView, terrain);

	assert(visibility.HasView(editorView));
	assert(visibility.HasView(playerView));
	assert(visibility.ViewCount() == 2);
	assert(visibility.VisibleCount(editorView) == 2);
	assert(visibility.VisibleCount(playerView) == 1);
	assert(visibility.IsVisible(editorView, model));
	assert(!visibility.IsVisible(playerView, model));
	assert(visibility.IsVisible(playerView, terrain));

	visibility.ClearView(playerView);
	assert(!visibility.HasView(playerView));
	assert(visibility.ViewCount() == 1);

	visibility.BeginFrame(43);
	assert(visibility.FrameSerial() == 43);
	assert(visibility.ViewCount() == 0);
	return 0;
}
